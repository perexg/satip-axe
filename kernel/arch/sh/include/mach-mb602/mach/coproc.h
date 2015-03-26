/* ------------------------------------------------------------------------
 *
 *  HW dependent actions: STb5202 platform
 */

#define N_COPROC        2

#define COPR_RAM_START	 	0x4000000	/* ST231 LMI RAM base address */

#define SYSCON_REGS_BASE        P2SEGADDR(0x19001000)
#define SYSCFG_09		(SYSCON_REGS_BASE + 0x124)
#define SYSCFG_BOOT_REG(x)	(SYSCON_REGS_BASE + (x ? 0x168 : 0x170))
#define SYSCFG_RESET_REG(x)	(SYSCON_REGS_BASE + (x ? 0x16c : 0x174))
