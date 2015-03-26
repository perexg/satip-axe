#ifndef __MMU_H
#define __MMU_H

/*
 * Privileged Space Mapping Buffer (PMB) definitions
 */
#define PMB_PASCR		0xff000070
#define PMB_IRMCR		0xff000078

#define PASCR_SE		0x80000000

#define PMB_ADDR		0xf6100000
#define PMB_DATA		0xf7100000
#define PMB_ENTRY_MAX		16
#define PMB_E_MASK		0x0000000f
#define PMB_E_SHIFT		8

#define PMB_SZ_16M		0x00000000
#define PMB_SZ_64M		0x00000010
#define PMB_SZ_128M		0x00000080
#define PMB_SZ_512M		0x00000090
#define PMB_SZ_MASK		PMB_SZ_512M
#define PMB_C			0x00000008
#define PMB_WT			0x00000001
#define PMB_UB			0x00000200
#define PMB_V			0x00000100

#define PMB_NO_ENTRY		(-1)

#ifndef __ASSEMBLY__

/* Default "unsigned long" context */
typedef unsigned long mm_context_id_t[NR_CPUS];

typedef struct {
#ifdef CONFIG_MMU
	mm_context_id_t		id;
	void			*vdso;
#else
	unsigned long		end_brk;
#endif
#ifdef CONFIG_BINFMT_ELF_FDPIC
	unsigned long		exec_fdpic_loadmap;
	unsigned long		interp_fdpic_loadmap;
#endif
} mm_context_t;

#ifdef CONFIG_PMB
/* arch/sh/mm/pmb.c */
bool __in_29bit_mode(void);

long pmb_remap(unsigned long phys, unsigned long size, unsigned long flags);
int pmb_unmap(unsigned long addr);
void pmb_init(void);
int pmb_virt_to_phys(void *addr, unsigned long *phys, unsigned long *flags);

#else

#ifdef CONFIG_29BIT
#define __in_29bit_mode()	(1)
#else
#define __in_29bit_mode()	(0)
#endif

#endif /* CONFIG_PMB */
#endif /* __ASSEMBLY__ */

#endif /* __MMU_H */
