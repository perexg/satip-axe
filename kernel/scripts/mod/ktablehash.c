/*
 * Functions used by modpost to create data sections for Fast LKM hash loader
 *
 * Copyright (C) 2008-2009 STMicroelectronics Ltd
 *
 * Author(s): Carmelo Amoroso <carmelo.amoroso@st.com>, STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#define EMPTY_SLOT -1
#include "modpost.h"

/**
 * Record hash_values for unresolved symbols
 **/

void add_undef_hash(struct buffer *b, struct module *mod)
{
	struct symbol *s;
	unsigned int i;

	buf_printf(b, "#ifdef CONFIG_LKM_ELF_HASH\n");
	buf_printf(b, "static unsigned long __symtab_hash[]\n");
	buf_printf(b, "__used\n");
	buf_printf(b, "__attribute__((section(\".undef.hash\"))) = {\n\t");

	for (s = mod->unres, i = 0; s; s = s->next) {
		/*
		 * Fill with zero, the order of unresolved symbol is not yet
		 * correct. This will create a placeholder for the hash values.
		 */
		buf_printf(b, "%lx, ", 0L);
		if (!(++i % 24))
			buf_printf(b, "\n\t");
	}
	buf_printf(b, "\n};\n");
	buf_printf(b, "#endif\n");
}

/* Array used to determine the number of hash table buckets to use
   based on the number of symbols there are.  If there are fewer than
   3 symbols we use 1 bucket, fewer than 17 symbols we use 3 buckets,
   fewer than 37 we use 17 buckets, and so forth.  We never use more
   than 32771 buckets.  */

static const size_t elf_buckets[] = {
	1, 3, 17, 37, 67, 97, 131, 197, 263, 521, 1031, 2053, 4099, 8209,
	16411, 32771, 0
};

/* FIXME: must implement the optimized algorithm for best size choosing */
static uint32_t compute_bucket_count(unsigned long int nsyms, int gnu_hash)
{
	uint32_t best_size = 0;
	unsigned int i;
	/*
	 * This is the fallback solution if no 64bit type is available or if we
	 * are not supposed to spend much time on optimizations.  We select the
	 * bucket count using a fixed set of numbers.
	 */
	for (i = 0; elf_buckets[i] != 0; i++) {
		best_size = elf_buckets[i];
		if (nsyms < elf_buckets[i + 1])
			break;
	}
	if (gnu_hash && best_size < 2)
		best_size = 2;
	return best_size;
}

/* Handle collisions: return the max of the used slot of the chain
  -1 in case of error */
static int handle_collision(struct elf_htable *htable, unsigned long idx,
		     unsigned long value)
{
	uint32_t *slot;

	/* sanity check: check chain's boundary */
	if (idx >= htable->nchain)
		return -1;

	slot = &htable->chains[idx];

	/* Fill the chains[idx] with the new value, if empty. */
	if (*slot == EMPTY_SLOT) {
		*slot = value;
		return 0;
	}
	/*
	 * If the slot is already used, used the value itself
	 * as a new index for the next chain entry.
	 * Do it recursively.
	 */
	return handle_collision(htable, *slot, value);
}

static int fill_hashtable(struct elf_htable *htable,
			  struct kernel_symbol *start,
			  struct kernel_symbol *stop,
			  long kstrings_base)
{
	struct kernel_symbol *ksym;
	uint32_t nb;
	unsigned long hvalue;
	int last_chain_slot;

	/* sanity check */
	if ((htable->elf_buckets == NULL) || (htable->chains == NULL))
		return -1;
	/* Initialize buckets and chains with EMPTY_SLOT */
	memset(htable->elf_buckets, EMPTY_SLOT,
	       htable->nbucket * sizeof(uint32_t));
	memset(htable->chains, EMPTY_SLOT, htable->nchain * sizeof(uint32_t));

	nb = htable->nbucket;
	for (ksym = start, hvalue = 0; ksym < stop; ksym++, hvalue++) {
		const unsigned char *name = GET_KSTRING(ksym, kstrings_base);
		unsigned long h = gnu_hash(name);
		unsigned long idx = h % nb;
		uint32_t *slot = &htable->elf_buckets[idx];

		/*
		 * Store the index of the export symbol ksym in its
		 * related __ksymtable in the hash table buckets for
		 * using during lookup.
		 * If the slot is alredy used ( != -1) then we have a collision
		 * it needs to create an entry in the chain
		 */
		if (*slot == EMPTY_SLOT)
			*slot = hvalue;
		else {
			if (handle_collision(htable, *slot, hvalue) < 0)
				/* Something wrong happened */
				return -1;
		}
	}
	/*
	 * Update the chain lenght with the best value
	 * so that we will cut unused entries beyond this upper limit
	 * In the best case, when there are not collisions, htable->chains
	 * will be 0 size... good !
	 */
	/* Look for upper chains empty slot */
	for (last_chain_slot = htable->nchain; --last_chain_slot >= 0 &&
	     htable->chains[last_chain_slot] == EMPTY_SLOT;)
		;

	htable->nchain = last_chain_slot + 1;
	return 0;
}

static void add_elf_hashtable(struct buffer *b, const struct elf_info *elf,
			      enum ksymtab_type type)
{
	const char *table = elf->ksym_tables[type].name;
	struct kernel_symbol *kstart = elf->ksym_tables[type].start;
	struct kernel_symbol *kstop = elf->ksym_tables[type].stop;

	struct elf_htable htable = {
		.nbucket = 0,
		.nchain = 0,
		.elf_buckets = NULL,
		.chains = NULL,
	};

	unsigned long nsyms = (unsigned long)(kstop - kstart);
	unsigned long i;

	htable.nbucket = compute_bucket_count(nsyms, 0);
	htable.elf_buckets = (uint32_t *)
				malloc(htable.nbucket * sizeof(uint32_t));

	if (!htable.elf_buckets)
		return;
	/*
	 * Worst case: the chain is as long as the maximum number of
	 * exported symbols should be put in the chain
	 */
	htable.nchain = nsyms;
	htable.chains = (uint32_t *) malloc(htable.nchain * sizeof(uint32_t));
	if (!htable.chains)
		return;

	/*
	 * When handling relocatable objects as at this stage, ksym->name
	 * is always an offset with respect to the __ksymtab_strings, so we must
	 * use elf->kstrings for both vmlinux.o and <module>.o
	 */
	if (fill_hashtable(&htable, kstart, kstop, (long) elf->kstrings) < 0)
		return;
	buf_printf(b, "#ifdef CONFIG_LKM_ELF_HASH\n\n");
	buf_printf(b, "#include <linux/types.h>\n");

	buf_printf(b, "static uint32_t htable%s[]\n", table);
	buf_printf(b, "__used\n");
	buf_printf(b, "__attribute__((section(\"%s.htable\"))) = {\n", table);

	/* 1st entry is nbucket */
	buf_printf(b, "\t%u, /* bucket lenght*/\n", htable.nbucket);
	/* 2nd entry is nchain */
	buf_printf(b, "\t%u, /* chain lenght */\n", htable.nchain);
	buf_printf(b, "\t/* the buckets */\n\t");
	for (i = 0; i < htable.nbucket; i++) {
		buf_printf(b, "%4d, ", htable.elf_buckets[i]);
		if (!((i+1) % 12))
			buf_printf(b, "\n\t");
	}
	buf_printf(b, "\n\t/* the chains */\n\t");
	for (i = 0; i < htable.nchain; i++) {
		buf_printf(b, "%4d, ", htable.chains[i]);
		if (!((i+1) % 12))
			buf_printf(b, "\n\t");
	}
	buf_printf(b, "\n};\n");
	buf_printf(b, "#endif\n");
	free(htable.elf_buckets);
	free(htable.chains);
}

/*
 * Add hash table (ELF SysV) for exported symbols
 */
void add_ksymtable_hash(struct buffer *b, struct module *mod)
{

	enum ksymtab_type k;

	/* Skip __ksymtab_strings */
	for (k = KSYMTAB; k < KSYMTAB_ALL; k++)
		if (mod->info->ksym_tables[k].name)
			add_elf_hashtable(b, mod->info, k);
}
