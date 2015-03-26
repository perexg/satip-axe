#ifndef _LIBELF_H_
#define _LIBELF_H_

#include <linux/elf.h>
#include <linux/module.h>

struct ELFinfo {
	uint8_t	*base;	/* Base address of ELF image in memory  */
	Elf_Ehdr	*header; /* Base address of ELF header in memory */
	uint32_t	size;	/* Total size of ELF data in bytes */
	uint32_t	mmapped;	/* Set to 1 if ELF file mmapped */
	Elf_Shdr	*secbase;	/* Section headers base address */
	Elf32_Phdr	*progbase;	/* Program headers base address */
	char		*strtab;	/* String table for section headers */
	uint32_t	strtabsize;	/* Size of string table */
	uint32_t	strsecindex;	/* Section header index for strings */
	uint32_t	numsections;	/* Number of sections */
	uint32_t	numpheaders;	/* Number of program headers */
};

struct typess {
	uint32_t	val;
	char 		*name;
};

#define ELF_TYPES	{0, "NULL"}, \
	{1, "PROGBITS"}, \
	{2, "SYMTAB"}, \
	{3, "STRTAB"}, \
	{4, "RELA"}, \
	{5, "HASH"}, \
	{6, "DYNAMIC"}, \
	{7, "NOTE"}, \
	{8, "NOBITS"}, \
	{9, "REL"}, \
	{10, "SHLIB"}, \
	{11, "DYNSYM"}, \
	{14, "INIT_ARRAY"}, \
	{15, "FINI_ARRAY"}, \
	{16, "PREINIT_ARRAY"}, \
	{17, "GROUP"}, \
	{18, "SYMTAB_SHNDX"}, \
	{0x6ffffff6, "GNU_HASH"}, \
	{0x6ffffff7, "GNU_PRELINK_LIBLIST"}, \
	{0x6ffffff8, "CHECKSUM"}, \
	{0x6ffffffd, "GNU version definitions"}, \
	{0x6ffffffe, "GNU version needs"}, \
	{0x6fffffff, "GNU version symbol table"}, \
	{0xffffffff, NULL}

extern unsigned int ELF_checkIdent(Elf32_Ehdr *);
extern struct ELFinfo *ELF_initFromMem(uint8_t *, uint32_t, int);
extern uint32_t ELF_free(struct ELFinfo *);
extern Elf32_Shdr *ELF_getSectionByIndex(const struct ELFinfo *, uint32_t);
extern Elf32_Shdr *ELF_getSectionByNameCheck(const struct ELFinfo *,
					const char *, uint32_t *, int, int);
extern void ELF_printHeaderInfo(const struct ELFinfo *);
extern void ELF_printSectionInfo(const struct ELFinfo *);
extern unsigned long ELF_findBaseAddrCheck(Elf32_Ehdr *, Elf32_Shdr *,
					unsigned long *, int, int);
extern int ELF_searchSectionType(const struct ELFinfo *, const char *, int *);
extern unsigned long ELF_checkPhMemSize(const struct ELFinfo *);
extern unsigned long ELF_checkPhMinVaddr(const struct ELFinfo *);

static inline Elf32_Shdr *ELF_getSectionByName(const struct ELFinfo *elfinfo,
					const char *secname, uint32_t *index)
{
	return ELF_getSectionByNameCheck(elfinfo, secname, index,
						SHF_NULL, SHT_NULL);
}
static inline unsigned long ELF_findBaseAddr(Elf32_Ehdr *hdr,
				Elf32_Shdr *sechdrs, unsigned long *base)
{
	return ELF_findBaseAddrCheck(hdr, sechdrs, base, SHF_NULL, SHT_NULL);
}

#endif /* _LIBELF_H_ */
