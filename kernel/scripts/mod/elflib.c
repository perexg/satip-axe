#include "elflib.h"

#define PRINTF __attribute__ ((format (printf, 1, 2)))

PRINTF void fatal(const char *fmt, ...)
{
	va_list arglist;

	fprintf(stderr, "FATAL: ");

	va_start(arglist, fmt);
	vfprintf(stderr, fmt, arglist);
	va_end(arglist);

	exit(1);
}

PRINTF void warn(const char *fmt, ...)
{
	va_list arglist;

	fprintf(stderr, "WARNING: ");

	va_start(arglist, fmt);
	vfprintf(stderr, fmt, arglist);
	va_end(arglist);
}

PRINTF void merror(const char *fmt, ...)
{
	va_list arglist;

	fprintf(stderr, "ERROR: ");

	va_start(arglist, fmt);
	vfprintf(stderr, fmt, arglist);
	va_end(arglist);
}

static void *grab_file_rw(const char *filename, unsigned long *size,
			  unsigned char write)
{
	struct stat st;
	void *map;
	int fd, open_flag, map_flag;

	if (write) {
		open_flag = O_RDWR;
		map_flag = MAP_SHARED;
	} else {
		open_flag = O_RDONLY;
		map_flag = MAP_PRIVATE;
	}

	fd = open(filename, open_flag);
	if (fd < 0 || fstat(fd, &st) != 0)
		return NULL;

	*size = st.st_size;
	map = mmap(NULL, *size, PROT_READ|PROT_WRITE, map_flag, fd, 0);
	close(fd);

	if (map == MAP_FAILED)
		return NULL;
	return map;
}

static void set_ksymtable(struct elf_info *info, enum ksymtab_type type,
			  Elf_Ehdr *hdr, Elf_Shdr *sechdrs, unsigned int secidx,
			  const char *secname)
{

	info->ksym_tables[type].start = (struct kernel_symbol *) \
			((void *) hdr + sechdrs[secidx].sh_offset);
	info->ksym_tables[type].stop = (struct kernel_symbol *) \
	((void *) hdr + sechdrs[secidx].sh_offset + sechdrs[secidx].sh_size);
	info->ksym_tables[type].name = strdup(secname);
	info->ksym_tables[type].sec = secidx;
}

static int parse_elf_rw(struct elf_info *info, const char *filename,
			unsigned char write)
{
	unsigned int i;
	Elf_Ehdr *hdr;
	Elf_Shdr *sechdrs;
	Elf_Sym  *sym;
	char *lkm_suffix;

	hdr = grab_file_rw(filename, &info->size, write);
	if (!hdr) {
		perror(filename);
		exit(1);
	}
	info->hdr = hdr;
	if (info->size < sizeof(*hdr)) {
		/* file too small, assume this is an empty .o file */
		return 0;
	}
	/* Is this a valid ELF file? */
	if ((hdr->e_ident[EI_MAG0] != ELFMAG0) ||
	    (hdr->e_ident[EI_MAG1] != ELFMAG1) ||
	    (hdr->e_ident[EI_MAG2] != ELFMAG2) ||
	    (hdr->e_ident[EI_MAG3] != ELFMAG3)) {
		/* Not an ELF file - silently ignore it */
		return 0;
	}

	/* Fix endianness in ELF header */
	hdr->e_type      = TO_NATIVE(hdr->e_type);
	hdr->e_machine   = TO_NATIVE(hdr->e_machine);
	hdr->e_version   = TO_NATIVE(hdr->e_version);
	hdr->e_entry     = TO_NATIVE(hdr->e_entry);
	hdr->e_phoff     = TO_NATIVE(hdr->e_phoff);
	hdr->e_shoff     = TO_NATIVE(hdr->e_shoff);
	hdr->e_flags     = TO_NATIVE(hdr->e_flags);
	hdr->e_ehsize    = TO_NATIVE(hdr->e_ehsize);
	hdr->e_phentsize = TO_NATIVE(hdr->e_phentsize);
	hdr->e_phnum     = TO_NATIVE(hdr->e_phnum);
	hdr->e_shentsize = TO_NATIVE(hdr->e_shentsize);
	hdr->e_shnum     = TO_NATIVE(hdr->e_shnum);
	hdr->e_shstrndx  = TO_NATIVE(hdr->e_shstrndx);
	sechdrs = (void *)hdr + hdr->e_shoff;
	info->sechdrs = sechdrs;

	/* Check if file offset is correct */
	if (hdr->e_shoff > info->size) {
		fatal("section header offset=%lu in file '%s' is bigger than "
		      "filesize=%lu\n", (unsigned long)hdr->e_shoff,
		      filename, info->size);
		return 0;
	}

	/* Fix endianness in section headers */
	for (i = 0; i < hdr->e_shnum; i++) {
		sechdrs[i].sh_name      = TO_NATIVE(sechdrs[i].sh_name);
		sechdrs[i].sh_type      = TO_NATIVE(sechdrs[i].sh_type);
		sechdrs[i].sh_flags     = TO_NATIVE(sechdrs[i].sh_flags);
		sechdrs[i].sh_addr      = TO_NATIVE(sechdrs[i].sh_addr);
		sechdrs[i].sh_offset    = TO_NATIVE(sechdrs[i].sh_offset);
		sechdrs[i].sh_size      = TO_NATIVE(sechdrs[i].sh_size);
		sechdrs[i].sh_link      = TO_NATIVE(sechdrs[i].sh_link);
		sechdrs[i].sh_info      = TO_NATIVE(sechdrs[i].sh_info);
		sechdrs[i].sh_addralign = TO_NATIVE(sechdrs[i].sh_addralign);
		sechdrs[i].sh_entsize   = TO_NATIVE(sechdrs[i].sh_entsize);
	}
	/* Find symbol tables and text section. */
	for (i = 1; i < hdr->e_shnum; i++) {
		const char *secstrings
			= (void *)hdr + sechdrs[hdr->e_shstrndx].sh_offset;
		const char *secname;
		int nobits = sechdrs[i].sh_type == SHT_NOBITS;

		if (!nobits && sechdrs[i].sh_offset > info->size) {
			fatal("%s is truncated. sechdrs[i].sh_offset=%lu > "
			      "sizeof(*hrd)=%zu\n", filename,
			      (unsigned long)sechdrs[i].sh_offset,
			      sizeof(*hdr));
			return 0;
		}
		secname = secstrings + sechdrs[i].sh_name;

		if (strcmp(secname, ".text") == 0)
			info->base_addr = sechdrs[i].sh_addr -
					  sechdrs[i].sh_offset;
		else if (strcmp(secname, ".modinfo") == 0) {
			if (nobits)
				fatal("%s has NOBITS .modinfo\n", filename);
			info->modinfo = (void *)hdr + sechdrs[i].sh_offset;
			info->modinfo_len = sechdrs[i].sh_size;
		} else if (strcmp(secname, "__ksymtab") == 0)
			set_ksymtable(info, KSYMTAB, hdr, sechdrs, i, secname);
		else if (strcmp(secname, "__ksymtab_unused") == 0)
			set_ksymtable(info, KSYMTAB_UNUSED, hdr, sechdrs, i,
				      secname);
		else if (strcmp(secname, "__ksymtab_gpl") == 0)
			set_ksymtable(info, KSYMTAB_GPL, hdr, sechdrs, i,
				      secname);
		else if (strcmp(secname, "__ksymtab_unused_gpl") == 0)
			set_ksymtable(info, KSYMTAB_UNUSED_GPL, hdr, sechdrs, i,
				      secname);
		else if (strcmp(secname, "__ksymtab_gpl_future") == 0)
			set_ksymtable(info, KSYMTAB_GPL_FUTURE, hdr, sechdrs, i,
				      secname);
		else if (strcmp(secname, "__ksymtab_strings") == 0)
			info->kstrings = (void *)hdr + sechdrs[i].sh_offset;
		else if (strcmp(secname, "__markers_strings") == 0)
			info->markers_strings_sec = i;
		else if (strcmp(secname, ".undef.hash") == 0) {
			info->undef_hash.start = (void *)
			    hdr + sechdrs[i].sh_offset;
			info->undef_hash.stop = (void *)
			    hdr + sechdrs[i].sh_offset + sechdrs[i].sh_size;
		}

		if (sechdrs[i].sh_type != SHT_SYMTAB)
			continue;

		info->symtab_start = (void *)hdr + sechdrs[i].sh_offset;
		info->symtab_stop  = (void *)hdr + sechdrs[i].sh_offset
					+ sechdrs[i].sh_size;
		info->strtab       = (void *)hdr +
					sechdrs[sechdrs[i].sh_link].sh_offset;
	}
	if (!info->symtab_start)
		fatal("%s has no symtab?\n", filename);

	/* Check if it is the vmlinux or lkm */
	lkm_suffix = strstr(filename, ".ko");
	if (lkm_suffix && (strlen(lkm_suffix) == 3))
		/* Likely this is a lkm */
		/*
		 * ksym->name is an offset with respect to the start of the
		 *  __ksymtab_strings
		 */
		info->kstr_offset = (long) info->kstrings;
	else
		/*
		 * In this case, ksym->name is the absolute value of the string
		 * into the __ksymtab_strings
		 */
		 info->kstr_offset = (long)info->hdr - (long)info->base_addr;

	/* Fix endianness in symbols */
	for (sym = info->symtab_start; sym < info->symtab_stop; sym++) {
		sym->st_shndx = TO_NATIVE(sym->st_shndx);
		sym->st_name  = TO_NATIVE(sym->st_name);
		sym->st_value = TO_NATIVE(sym->st_value);
		sym->st_size  = TO_NATIVE(sym->st_size);
	}
	return 1;
}

ksym_hash_t gnu_hash(const unsigned char *name)
{
	ksym_hash_t h = 5381;
	unsigned char c;
	for (c = *name; c != '\0'; c = *++name)
		h = h * 33 + c;
	return h & 0xffffffff;
}
void *grab_file(const char *filename, unsigned long *size)
{
	/* Read-only */
	return grab_file_rw(filename, size, 0);
}

void release_file(void *file, unsigned long size)
{
	munmap(file, size);
}

int parse_elf(struct elf_info *info, const char *filename)
{
	/* Read-only */
	return parse_elf_rw(info, filename, 0);
}

int parse_writable_elf(struct elf_info *info, const char *filename)
{
	return parse_elf_rw(info, filename, 1);
}

void parse_elf_finish(struct elf_info *info)
{
	release_file(info->hdr, info->size);
}
