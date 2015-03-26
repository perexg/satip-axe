#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <elf.h>

#include "elflib.h"

#define NOFAIL(ptr)   do_nofail((ptr), #ptr)
void *do_nofail(void *ptr, const char *expr);

struct buffer {
	char *p;
	int pos;
	int size;
};

void __attribute__((format(printf, 2, 3)))
buf_printf(struct buffer *buf, const char *fmt, ...);

void
buf_write(struct buffer *buf, const char *s, int len);

struct module {
	struct module *next;
	const char *name;
	int gpl_compatible;
	struct symbol *unres;
	struct elf_info *info;
	int seen;
	int skip;
	int has_init;
	int has_cleanup;
	struct buffer dev_table_buf;
	char **markers;
	size_t nmarkers;
	char	     srcversion[25];
};

/* How a symbol is exported */
enum export {
	export_plain,      export_unused,     export_gpl,
	export_unused_gpl, export_gpl_future, export_unknown
};


/* A hash of all exported symbols,
 * struct symbol is also used for lists of unresolved symbols */

#define SYMBOL_HASH_SIZE 1024

struct symbol {
	struct symbol *next;
	struct module *module;
	unsigned int crc;
	int crc_valid;
	unsigned int weak:1;
	unsigned int vmlinux:1;    /* 1 if symbol is defined in vmlinux */
	unsigned int kernel:1;     /* 1 if symbol is from kernel
				    *  (only for external modules) **/
	unsigned int preloaded:1;  /* 1 if symbol from Module.symvers */
	enum export  export;       /* Type of export */
	char name[0];
};

/* file2alias.c */
extern unsigned int cross_build;
void handle_moddevtable(struct module *mod, struct elf_info *info,
			Elf_Sym *sym, const char *symname);
void add_moddevtable(struct buffer *buf, struct module *mod);

/* sumversion.c */
void maybe_frob_rcs_version(const char *modfilename,
			    char *version,
			    void *modinfo,
			    unsigned long modinfo_offset);
void get_src_version(const char *modname, char sum[], unsigned sumlen);

/* from modpost.c */
char* get_next_line(unsigned long *pos, void *file, unsigned long size);

/* from ktablehash.c */
#ifdef CONFIG_LKM_ELF_HASH
void add_undef_hash(struct buffer *b, struct module *mod);
void add_ksymtable_hash(struct buffer *b, struct module *mod);
#endif

