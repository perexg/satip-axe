/*
 * Post-process kernel image and kernel modules for Fast LKM hash loader
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
 * Usage: it is called directly by kbuild
 *        (see $(rule_ksymhash) defined in scripts/Makefile.ksymhash)
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/stat.h>

#include "elflib.h"

static inline
void compute_exported_hash(const struct elf_info *elf, enum ksymtab_type tp)
{

	struct kernel_symbol *sym;
	struct kernel_symbol *start = elf->ksym_tables[tp].start;
	struct kernel_symbol *stop = elf->ksym_tables[tp].stop;

	for (sym = start; sym < stop; sym++)
		sym->hash_value = gnu_hash(GET_KSTRING(sym, elf->kstr_offset));
}

static void compute_unresolved_hash(struct elf_info *elf)
{

	Elf_Sym *sym;
	ksym_hash_t *hash_values = elf->undef_hash.start;

	if (!hash_values)
		/* .undef.hash section is not present */
		return;

	for (sym = elf->symtab_start; sym < elf->symtab_stop; sym++) {
		if (sym->st_shndx == SHN_UNDEF) {
			/* undefined symbol */
			if (ELF_ST_BIND(sym->st_info) != STB_GLOBAL &&
			    ELF_ST_BIND(sym->st_info) != STB_WEAK)
				continue;
			else {
				/* GLOBAL or WEAK undefined symbols */
				*hash_values = gnu_hash((unsigned char *)
						(elf->strtab + sym->st_name));
				/*
				 * The hash_values array stored into the
				 * .undef.hash section that is ordered as the
				 * undefined symbols of the .symtab
				 */
				hash_values++;
			}
		}
	}
}

int main(int argc, char **argv)
{

	enum ksymtab_type k;
	struct elf_info info = { };

	if (!parse_writable_elf(&info, argv[1]))
		exit(1);

	for (k = KSYMTAB; k < KSYMTAB_ALL; k++)
		if (info.ksym_tables[k].name)
			/* Compute hash value for exported symbols */
			compute_exported_hash(&info, k);

	compute_unresolved_hash(&info);

	parse_elf_finish(&info);
	return 0;
}
