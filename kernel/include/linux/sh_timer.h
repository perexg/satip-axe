#ifndef __SH_TIMER_H__
#define __SH_TIMER_H__
struct sh_timer_config {
	char *name;
	long channel_offset;
	int timer_bit;
	char *clk;
	unsigned long clockevent_rating;
	unsigned long clocksource_rating;
};

struct sh_timer_callb {
	void	(*timer_start) (void *priv);
	void	(*timer_stop) (void *priv);
	void	(*set_rate) (void *priv, unsigned long rate);
	void	*tmu_priv;
};

struct sh_timer_callb *sh_timer_register(void *handler, void *data);
void sh_timer_unregister(void *priv);
#endif /* __SH_TIMER_H__ */
