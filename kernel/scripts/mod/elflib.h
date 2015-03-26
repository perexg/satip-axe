#include <sys/stat.h>
#include <elf.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "elfconfig.h"

#if KERNEL_ELFCLASS == ELFCLASS32

#define Elf_Ehdr    Elf32_Ehdr
#define Elf_Shdr    Elf32_Shdr
#define Elf_Sym     Elf32_Sym
#define Elf_Addr    Elf32_Addr
#define Elf_Section Elf32_Section
#define ELF_ST_BIND ELF32_ST_BIND
#define ELF_ST_TYPE ELF32_ST_TYPE

#define Elf_Rel     Elf32_Rel
#define Elf_Rela    Elf32_Rela
#define ELF_R_SYM   ELF32_R_SYM
#define ELF_R_TYPE  ELF32_R_TYPE

/* It needs to match sizeof within kernel
 * as defined in include/linux/module.h
 */
#define ksym_t      uint32_t
#define kstr_t      uint32_t
#define ksym_hash_t uint32_t
#else

#define Elf_Ehdr    Elf64_Ehdr
#define Elf_Shdr    Elf64_Shdr
#define Elf_Sym     Elf64_Sym
#define Elf_Addr    Elf64_Addr
#define Elf_Section Elf64_Section
#define ELF_ST_BIND ELF64_ST_BIND
#define ELF_ST_TYPE ELF64_ST_TYPE

#define Elf_Rel     Elf64_Rel
#define Elf_Rela    Elf64_Rela
#define ELF_R_SYM   ELF64_R_SYM
#define ELF_R_TYPE  ELF64_R_TYPE

/* It needs to match sizeof within kernel
 * as defined in include/linux/module.h
 */
#define ksym_t      uint64_t
#define kstr_t      uint64_t
#define ksym_hash_t uint64_t
#endif

/* The 64-bit MIPS ELF ABI uses an unusual reloc format. */
typedef struct
{
	Elf32_Word    r_sym;	/* Symbol index */
	unsigned char r_ssym;	/* Special symbol for 2nd relocation */
	unsigned char r_type3;	/* 3rd relocation type */
	unsigned char r_type2;	/* 2nd relocation type */
	unsigned char r_type1;	/* 1st relocation type */
} _Elf64_Mips_R_Info;

typedef union
{
	Elf64_Xword		r_info_number;
	_Elf64_Mips_R_Info	r_info_fields;
} _Elf64_Mips_R_Info_union;

#define ELF64_MIPS_R_SYM(i) \
  ((__extension__ (_Elf64_Mips_R_Info_union)(i)).r_info_fields.r_sym)

#define ELF64_MIPS_R_TYPE(i) \
  ((__extension__ (_Elf64_Mips_R_Info_union)(i)).r_info_fields.r_type1)

#if KERNEL_ELFDATA != HOST_ELFDATA

static inline void __endian(const void *src, void *dest, unsigned int size)
{
	unsigned int i;
	for (i = 0; i < size; i++)
		((unsigned char *)dest)[i] =
		    ((unsigned char *)src)[size - i - 1];
}

#define TO_NATIVE(x)						\
({								\
	typeof(x) __x;						\
	__endian(&(x), &(__x), sizeof(__x));			\
	__x;							\
})

#else /* endianness matches */

#define TO_NATIVE(x) (x)

#endif

#define GET_KSTRING(__ksym, __offset) (unsigned char *)(__ksym->name + __offset)

/* We have no more than 5 kernel symbol tables
	__ksymtab
	__ksymtab_gpl
	__ksymtab_unused
	__ksymtab_unused_gpl
	__ksymtab_gpl_future
*/

enum ksymtab_type {
	KSYMTAB = 0,
	KSYMTAB_GPL,
	KSYMTAB_UNUSED,
	KSYMTAB_UNUSED_GPL,
	KSYMTAB_GPL_FUTURE,
	KSYMTAB_ALL,
};

/*
 * This maps the ELF hash table
 * The entries in the .hash table always have a size of 32 bits.
 */
struct elf_htable {
	uint32_t nbucket;
	uint32_t nchain;
	uint32_t *elf_buckets;
	uint32_t *chains;
};

/* as defined in include/linux/module.h */
struct kernel_symbol {
	ksym_t value;
	kstr_t name;
	ksym_hash_t hash_value;
};

struct kernel_symtab {
	const char *name;
	struct kernel_symbol *start;
	struct kernel_symbol *stop;
	Elf_Section sec;
};

struct elf_info {
	unsigned long size;
	Elf_Ehdr     *hdr;
	Elf_Shdr     *sechdrs;
	Elf_Sym *symtab_start;
	Elf_Sym *symtab_stop;
	long kstr_offset;
	unsigned long base_addr;

	struct {
		ksym_hash_t *start;
		ksym_hash_t *stop;
	} undef_hash;

	struct kernel_symtab ksym_tables[KSYMTAB_ALL];
	Elf_Section  markers_strings_sec;
	const char   *strtab;
	const char   *kstrings;
	char	     *modinfo;
	unsigned int modinfo_len;
};

/* from elflib.c */
void fatal(const char *fmt, ...);
void warn(const char *fmt, ...);
void merror(const char *fmt, ...);

ksym_hash_t gnu_hash(const unsigned char *name);
void *grab_file(const char *filename, unsigned long *size);
void release_file(void *file, unsigned long size);
int parse_elf(struct elf_info *info, const char *filename);
int parse_writable_elf(struct elf_info *info, const char *filename);
void parse_elf_finish(struct elf_info *info);


