/*kprobe_example.c*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/kallsyms.h>
#include <linux/sched.h>

/*For each probe you need to allocate a kprobe structure*/
static struct kprobe kp;

/*kprobe pre_handler: called just before the probed instruction is executed*/
int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
#if 1				/* KPROBE-SH */
	printk("pre_handler: p->addr=0x%p\n", p->addr);
#else				/* KPROBE-SH */
	printk("pre_handler: p->addr=0x%p, eip=%lx, eflags=0x%lx\n",
	       p->addr, regs->eip, regs->eflags);
#endif				/* KPROBE-SH */
	dump_stack();
	return 0;
}

/*kprobe post_handler: called after the probed instruction is executed*/
void handler_post(struct kprobe *p, struct pt_regs *regs, unsigned long flags)
{
#if 1				/* KPROBE-SH */
	printk("post_handler: p->addr=0x%p\n", p->addr);
#else				/* KPROBE-SH */
	printk("post_handler: p->addr=0x%p, eflags=0x%lx\n",
	       p->addr, regs->eflags);
#endif				/* KPROBE-SH */
}

/* fault_handler: this is called if an exception is generated for any
 * instruction within the pre- or post-handler, or when Kprobes
 * single-steps the probed instruction.
 */
int handler_fault(struct kprobe *p, struct pt_regs *regs, int trapnr)
{
	printk("fault_handler: p->addr=0x%p, trap #%dn", p->addr, trapnr);
	/* Return 0 because we don't handle the fault. */
	return 0;
}

int init_module(void)
{
	int ret;
	kp.pre_handler = handler_pre;
	kp.post_handler = handler_post;
	kp.fault_handler = handler_fault;
	kp.addr = (kprobe_opcode_t *) kallsyms_lookup_name("do_fork");
	/* register the kprobe now */
	if (!kp.addr) {
		printk("Couldn't find %s to plant kprobe\n", "do_fork");
		return -3;
	}
	if ((ret = register_kprobe(&kp) < 0)) {
		printk("register_kprobe failed, returned %d\n", ret);
		return -2;
	}
	printk("kprobe registered\n");
	return 0;
}

void cleanup_module(void)
{
	unregister_kprobe(&kp);
	printk("kprobe unregistered\n");
}

MODULE_LICENSE("GPL");
