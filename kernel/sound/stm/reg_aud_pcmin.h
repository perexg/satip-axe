#ifndef __SND_STM_AUD_PCMIN_H
#define __SND_STM_AUD_PCMIN_H

/*
 * AUD_PCMIN_RST
 */

#define offset__AUD_PCMIN_RST(ip) 0x00
#define get__AUD_PCMIN_RST(ip) readl(ip->base + \
	offset__AUD_PCMIN_RST(ip))
#define set__AUD_PCMIN_RST(ip, value) writel(value, ip->base + \
	offset__AUD_PCMIN_RST(ip))

/* RSTP */

#define shift__AUD_PCMIN_RST__RSTP(ip) 0
#define mask__AUD_PCMIN_RST__RSTP(ip) 0x1
#define get__AUD_PCMIN_RST__RSTP(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_RST(ip)) >> shift__AUD_PCMIN_RST__RSTP(ip)) & \
	mask__AUD_PCMIN_RST__RSTP(ip))
#define set__AUD_PCMIN_RST__RSTP(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMIN_RST(ip)) & ~(mask__AUD_PCMIN_RST__RSTP(ip) << \
	shift__AUD_PCMIN_RST__RSTP(ip))) | (((value) & \
	mask__AUD_PCMIN_RST__RSTP(ip)) << shift__AUD_PCMIN_RST__RSTP(ip)), \
	ip->base + offset__AUD_PCMIN_RST(ip))

#define value__AUD_PCMIN_RST__RSTP__RUNNING(ip) 0x0
#define mask__AUD_PCMIN_RST__RSTP__RUNNING(ip) \
	(value__AUD_PCMIN_RST__RSTP__RUNNING(ip) << \
	shift__AUD_PCMIN_RST__RSTP(ip))
#define set__AUD_PCMIN_RST__RSTP__RUNNING(ip) \
	set__AUD_PCMIN_RST__RSTP(ip, value__AUD_PCMIN_RST__RSTP__RUNNING(ip))

#define value__AUD_PCMIN_RST__RSTP__RESET(ip) 0x1
#define mask__AUD_PCMIN_RST__RSTP__RESET(ip) \
	(value__AUD_PCMIN_RST__RSTP__RESET(ip) << \
	shift__AUD_PCMIN_RST__RSTP(ip))
#define set__AUD_PCMIN_RST__RSTP__RESET(ip) \
	set__AUD_PCMIN_RST__RSTP(ip, value__AUD_PCMIN_RST__RSTP__RESET(ip))



/*
 * AUD_PCMIN_DATA
 */

#define offset__AUD_PCMIN_DATA(ip) 0x04
#define get__AUD_PCMIN_DATA(ip) readl(ip->base + \
	offset__AUD_PCMIN_DATA(ip))
#define set__AUD_PCMIN_DATA(ip, value) writel(value, ip->base + \
	offset__AUD_PCMIN_DATA(ip))

/* DATA */

#define shift__AUD_PCMIN_DATA__DATA(ip) 0
#define mask__AUD_PCMIN_DATA__DATA(ip) 0xffffffff
#define get__AUD_PCMIN_DATA__DATA(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_DATA(ip)) >> shift__AUD_PCMIN_DATA__DATA(ip)) & \
	mask__AUD_PCMIN_DATA__DATA(ip))
#define set__AUD_PCMIN_DATA__DATA(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMIN_DATA(ip)) & ~(mask__AUD_PCMIN_DATA__DATA(ip) << \
	shift__AUD_PCMIN_DATA__DATA(ip))) | (((value) & \
	mask__AUD_PCMIN_DATA__DATA(ip)) << shift__AUD_PCMIN_DATA__DATA(ip)), \
	ip->base + offset__AUD_PCMIN_DATA(ip))



/*
 * AUD_PCMIN_ITS
 */

#define offset__AUD_PCMIN_ITS(ip) 0x08
#define get__AUD_PCMIN_ITS(ip) readl(ip->base + \
	offset__AUD_PCMIN_ITS(ip))
#define set__AUD_PCMIN_ITS(ip, value) writel(value, ip->base + \
	offset__AUD_PCMIN_ITS(ip))

/* OVF */

#define shift__AUD_PCMIN_ITS__OVF(ip) 0
#define mask__AUD_PCMIN_ITS__OVF(ip) 0x1
#define get__AUD_PCMIN_ITS__OVF(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_ITS(ip)) >> shift__AUD_PCMIN_ITS__OVF(ip)) & \
	mask__AUD_PCMIN_ITS__OVF(ip))
#define set__AUD_PCMIN_ITS__OVF(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMIN_ITS(ip)) & ~(mask__AUD_PCMIN_ITS__OVF(ip) << \
	shift__AUD_PCMIN_ITS__OVF(ip))) | (((value) & \
	mask__AUD_PCMIN_ITS__OVF(ip)) << shift__AUD_PCMIN_ITS__OVF(ip)), \
	ip->base + offset__AUD_PCMIN_ITS(ip))

#define value__AUD_PCMIN_ITS__OVF__PENDING(ip) 0x1
#define mask__AUD_PCMIN_ITS__OVF__PENDING(ip) \
	(value__AUD_PCMIN_ITS__OVF__PENDING(ip) << \
	shift__AUD_PCMIN_ITS__OVF(ip))
#define set__AUD_PCMIN_ITS__OVF__PENDING(ip) \
	set__AUD_PCMIN_ITS__OVF(ip, value__AUD_PCMIN_ITS__OVF__PENDING(ip))

/* VSYNC */

#define shift__AUD_PCMIN_ITS__VSYNC(ip) 1
#define mask__AUD_PCMIN_ITS__VSYNC(ip) 0x1
#define get__AUD_PCMIN_ITS__VSYNC(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_ITS(ip)) >> shift__AUD_PCMIN_ITS__VSYNC(ip)) & \
	mask__AUD_PCMIN_ITS__VSYNC(ip))
#define set__AUD_PCMIN_ITS__VSYNC(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMIN_ITS(ip)) & ~(mask__AUD_PCMIN_ITS__VSYNC(ip) << \
	shift__AUD_PCMIN_ITS__VSYNC(ip))) | (((value) & \
	mask__AUD_PCMIN_ITS__VSYNC(ip)) << shift__AUD_PCMIN_ITS__VSYNC(ip)), \
	ip->base + offset__AUD_PCMIN_ITS(ip))

#define value__AUD_PCMIN_ITS__VSYNC__PENDING(ip) 0x1
#define mask__AUD_PCMIN_ITS__VSYNC__PENDING(ip) \
	(value__AUD_PCMIN_ITS__VSYNC__PENDING(ip) << \
	shift__AUD_PCMIN_ITS__VSYNC(ip))
#define set__AUD_PCMIN_ITS__VSYNC__PENDING(ip) \
	set__AUD_PCMIN_ITS__VSYNC(ip, \
	value__AUD_PCMIN_ITS__VSYNC__PENDING(ip))



/*
 * AUD_PCMIN_ITS_CLR
 */

#define offset__AUD_PCMIN_ITS_CLR(ip) 0x0c
#define get__AUD_PCMIN_ITS_CLR(ip) readl(ip->base + \
	offset__AUD_PCMIN_ITS_CLR(ip))
#define set__AUD_PCMIN_ITS_CLR(ip, value) writel(value, ip->base + \
	offset__AUD_PCMIN_ITS_CLR(ip))

/* OVF */

#define shift__AUD_PCMIN_ITS_CLR__OVF(ip) 0
#define mask__AUD_PCMIN_ITS_CLR__OVF(ip) 0x1
#define get__AUD_PCMIN_ITS_CLR__OVF(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_ITS_CLR(ip)) >> shift__AUD_PCMIN_ITS_CLR__OVF(ip)) & \
	mask__AUD_PCMIN_ITS_CLR__OVF(ip))
#define set__AUD_PCMIN_ITS_CLR__OVF(ip, value) writel((readl(ip->base \
	+ offset__AUD_PCMIN_ITS_CLR(ip)) & ~(mask__AUD_PCMIN_ITS_CLR__OVF(ip) \
	<< shift__AUD_PCMIN_ITS_CLR__OVF(ip))) | (((value) & \
	mask__AUD_PCMIN_ITS_CLR__OVF(ip)) << \
	shift__AUD_PCMIN_ITS_CLR__OVF(ip)), ip->base + \
	offset__AUD_PCMIN_ITS_CLR(ip))

#define value__AUD_PCMIN_ITS_CLR__OVF__CLEAR(ip) 0x1
#define mask__AUD_PCMIN_ITS_CLR__OVF__CLEAR(ip) \
	(value__AUD_PCMIN_ITS_CLR__OVF__CLEAR(ip) << \
	shift__AUD_PCMIN_ITS_CLR__OVF(ip))
#define set__AUD_PCMIN_ITS_CLR__OVF__CLEAR(ip) \
	set__AUD_PCMIN_ITS_CLR__OVF(ip, \
	value__AUD_PCMIN_ITS_CLR__OVF__CLEAR(ip))

/* VSYNC */

#define shift__AUD_PCMIN_ITS_CLR__VSYNC(ip) 1
#define mask__AUD_PCMIN_ITS_CLR__VSYNC(ip) 0x1
#define get__AUD_PCMIN_ITS_CLR__VSYNC(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_ITS_CLR(ip)) >> shift__AUD_PCMIN_ITS_CLR__VSYNC(ip)) \
	& mask__AUD_PCMIN_ITS_CLR__VSYNC(ip))
#define set__AUD_PCMIN_ITS_CLR__VSYNC(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMIN_ITS_CLR(ip)) & \
	~(mask__AUD_PCMIN_ITS_CLR__VSYNC(ip) << \
	shift__AUD_PCMIN_ITS_CLR__VSYNC(ip))) | (((value) & \
	mask__AUD_PCMIN_ITS_CLR__VSYNC(ip)) << \
	shift__AUD_PCMIN_ITS_CLR__VSYNC(ip)), ip->base + \
	offset__AUD_PCMIN_ITS_CLR(ip))



/*
 * AUD_PCMIN_IT_EN
 */

#define offset__AUD_PCMIN_IT_EN(ip) 0x10
#define get__AUD_PCMIN_IT_EN(ip) readl(ip->base + \
	offset__AUD_PCMIN_IT_EN(ip))
#define set__AUD_PCMIN_IT_EN(ip, value) writel(value, ip->base + \
	offset__AUD_PCMIN_IT_EN(ip))

/* OVF */

#define shift__AUD_PCMIN_IT_EN__OVF(ip) 0
#define mask__AUD_PCMIN_IT_EN__OVF(ip) 0x1
#define get__AUD_PCMIN_IT_EN__OVF(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_IT_EN(ip)) >> shift__AUD_PCMIN_IT_EN__OVF(ip)) & \
	mask__AUD_PCMIN_IT_EN__OVF(ip))
#define set__AUD_PCMIN_IT_EN__OVF(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMIN_IT_EN(ip)) & ~(mask__AUD_PCMIN_IT_EN__OVF(ip) << \
	shift__AUD_PCMIN_IT_EN__OVF(ip))) | (((value) & \
	mask__AUD_PCMIN_IT_EN__OVF(ip)) << shift__AUD_PCMIN_IT_EN__OVF(ip)), \
	ip->base + offset__AUD_PCMIN_IT_EN(ip))

#define value__AUD_PCMIN_IT_EN__OVF__DISABLED(ip) 0x0
#define mask__AUD_PCMIN_IT_EN__OVF__DISABLED(ip) \
	(value__AUD_PCMIN_IT_EN__OVF__DISABLED(ip) << \
	shift__AUD_PCMIN_IT_EN__OVF(ip))
#define set__AUD_PCMIN_IT_EN__OVF__DISABLED(ip) \
	set__AUD_PCMIN_IT_EN__OVF(ip, \
	value__AUD_PCMIN_IT_EN__OVF__DISABLED(ip))

#define value__AUD_PCMIN_IT_EN__OVF__ENABLED(ip) 0x1
#define mask__AUD_PCMIN_IT_EN__OVF__ENABLED(ip) \
	(value__AUD_PCMIN_IT_EN__OVF__ENABLED(ip) << \
	shift__AUD_PCMIN_IT_EN__OVF(ip))
#define set__AUD_PCMIN_IT_EN__OVF__ENABLED(ip) \
	set__AUD_PCMIN_IT_EN__OVF(ip, \
	value__AUD_PCMIN_IT_EN__OVF__ENABLED(ip))

/* VSYNC */

#define shift__AUD_PCMIN_IT_EN__VSYNC(ip) 1
#define mask__AUD_PCMIN_IT_EN__VSYNC(ip) 0x1
#define get__AUD_PCMIN_IT_EN__VSYNC(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_IT_EN(ip)) >> shift__AUD_PCMIN_IT_EN__VSYNC(ip)) & \
	mask__AUD_PCMIN_IT_EN__VSYNC(ip))
#define set__AUD_PCMIN_IT_EN__VSYNC(ip, value) writel((readl(ip->base \
	+ offset__AUD_PCMIN_IT_EN(ip)) & ~(mask__AUD_PCMIN_IT_EN__VSYNC(ip) << \
	shift__AUD_PCMIN_IT_EN__VSYNC(ip))) | (((value) & \
	mask__AUD_PCMIN_IT_EN__VSYNC(ip)) << \
	shift__AUD_PCMIN_IT_EN__VSYNC(ip)), ip->base + \
	offset__AUD_PCMIN_IT_EN(ip))

#define value__AUD_PCMIN_IT_EN__VSYNC__DISABLED(ip) 0x0
#define mask__AUD_PCMIN_IT_EN__VSYNC__DISABLED(ip) \
	(value__AUD_PCMIN_IT_EN__VSYNC__DISABLED(ip) << \
	shift__AUD_PCMIN_IT_EN__VSYNC(ip))
#define set__AUD_PCMIN_IT_EN__VSYNC__DISABLED(ip) \
	set__AUD_PCMIN_IT_EN__VSYNC(ip, \
	value__AUD_PCMIN_IT_EN__VSYNC__DISABLED(ip))

#define value__AUD_PCMIN_IT_EN__VSYNC__ENABLED(ip) 0x1
#define mask__AUD_PCMIN_IT_EN__VSYNC__ENABLED(ip) \
	(value__AUD_PCMIN_IT_EN__VSYNC__ENABLED(ip) << \
	shift__AUD_PCMIN_IT_EN__VSYNC(ip))
#define set__AUD_PCMIN_IT_EN__VSYNC__ENABLED(ip) \
	set__AUD_PCMIN_IT_EN__VSYNC(ip, \
	value__AUD_PCMIN_IT_EN__VSYNC__ENABLED(ip))



/*
 * AUD_PCMIN_IT_EN_SET
 */

#define offset__AUD_PCMIN_IT_EN_SET(ip) 0x14
#define get__AUD_PCMIN_IT_EN_SET(ip) readl(ip->base + \
	offset__AUD_PCMIN_IT_EN_SET(ip))
#define set__AUD_PCMIN_IT_EN_SET(ip, value) writel(value, ip->base + \
	offset__AUD_PCMIN_IT_EN_SET(ip))

/* OVF */

#define shift__AUD_PCMIN_IT_EN_SET__OVF(ip) 0
#define mask__AUD_PCMIN_IT_EN_SET__OVF(ip) 0x1
#define get__AUD_PCMIN_IT_EN_SET__OVF(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_IT_EN_SET(ip)) >> \
	shift__AUD_PCMIN_IT_EN_SET__OVF(ip)) & \
	mask__AUD_PCMIN_IT_EN_SET__OVF(ip))
#define set__AUD_PCMIN_IT_EN_SET__OVF(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMIN_IT_EN_SET(ip)) & \
	~(mask__AUD_PCMIN_IT_EN_SET__OVF(ip) << \
	shift__AUD_PCMIN_IT_EN_SET__OVF(ip))) | (((value) & \
	mask__AUD_PCMIN_IT_EN_SET__OVF(ip)) << \
	shift__AUD_PCMIN_IT_EN_SET__OVF(ip)), ip->base + \
	offset__AUD_PCMIN_IT_EN_SET(ip))

#define value__AUD_PCMIN_IT_EN_SET__OVF__SET(ip) 0x1
#define mask__AUD_PCMIN_IT_EN_SET__OVF__SET(ip) \
	(value__AUD_PCMIN_IT_EN_SET__OVF__SET(ip) << \
	shift__AUD_PCMIN_IT_EN_SET__OVF(ip))
#define set__AUD_PCMIN_IT_EN_SET__OVF__SET(ip) \
	set__AUD_PCMIN_IT_EN_SET__OVF(ip, \
	value__AUD_PCMIN_IT_EN_SET__OVF__SET(ip))

/* VSYNC */

#define shift__AUD_PCMIN_IT_EN_SET__VSYNC(ip) 1
#define mask__AUD_PCMIN_IT_EN_SET__VSYNC(ip) 0x1
#define get__AUD_PCMIN_IT_EN_SET__VSYNC(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_IT_EN_SET(ip)) >> \
	shift__AUD_PCMIN_IT_EN_SET__VSYNC(ip)) & \
	mask__AUD_PCMIN_IT_EN_SET__VSYNC(ip))
#define set__AUD_PCMIN_IT_EN_SET__VSYNC(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMIN_IT_EN_SET(ip)) & \
	~(mask__AUD_PCMIN_IT_EN_SET__VSYNC(ip) << \
	shift__AUD_PCMIN_IT_EN_SET__VSYNC(ip))) | (((value) & \
	mask__AUD_PCMIN_IT_EN_SET__VSYNC(ip)) << \
	shift__AUD_PCMIN_IT_EN_SET__VSYNC(ip)), ip->base + \
	offset__AUD_PCMIN_IT_EN_SET(ip))

#define value__AUD_PCMIN_IT_EN_SET__VSYNC__SET(ip) 0x1
#define mask__AUD_PCMIN_IT_EN_SET__VSYNC__SET(ip) \
	(value__AUD_PCMIN_IT_EN_SET__VSYNC__SET(ip) << \
	shift__AUD_PCMIN_IT_EN_SET__VSYNC(ip))
#define set__AUD_PCMIN_IT_EN_SET__VSYNC__SET(ip) \
	set__AUD_PCMIN_IT_EN_SET__VSYNC(ip, \
	value__AUD_PCMIN_IT_EN_SET__VSYNC__SET(ip))



/*
 * AUD_PCMIN_IT_EN_CLR
 */

#define offset__AUD_PCMIN_IT_EN_CLR(ip) 0x18
#define get__AUD_PCMIN_IT_EN_CLR(ip) readl(ip->base + \
	offset__AUD_PCMIN_IT_EN_CLR(ip))
#define set__AUD_PCMIN_IT_EN_CLR(ip, value) writel(value, ip->base + \
	offset__AUD_PCMIN_IT_EN_CLR(ip))

/* OVF */

#define shift__AUD_PCMIN_IT_EN_CLR__OVF(ip) 0
#define mask__AUD_PCMIN_IT_EN_CLR__OVF(ip) 0x1
#define get__AUD_PCMIN_IT_EN_CLR__OVF(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_IT_EN_CLR(ip)) >> \
	shift__AUD_PCMIN_IT_EN_CLR__OVF(ip)) & \
	mask__AUD_PCMIN_IT_EN_CLR__OVF(ip))
#define set__AUD_PCMIN_IT_EN_CLR__OVF(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMIN_IT_EN_CLR(ip)) & \
	~(mask__AUD_PCMIN_IT_EN_CLR__OVF(ip) << \
	shift__AUD_PCMIN_IT_EN_CLR__OVF(ip))) | (((value) & \
	mask__AUD_PCMIN_IT_EN_CLR__OVF(ip)) << \
	shift__AUD_PCMIN_IT_EN_CLR__OVF(ip)), ip->base + \
	offset__AUD_PCMIN_IT_EN_CLR(ip))

#define value__AUD_PCMIN_IT_EN_CLR__OVF__CLEAR(ip) 0x1
#define mask__AUD_PCMIN_IT_EN_CLR__OVF__CLEAR(ip) \
	(value__AUD_PCMIN_IT_EN_CLR__OVF__CLEAR(ip) << \
	shift__AUD_PCMIN_IT_EN_CLR__OVF(ip))
#define set__AUD_PCMIN_IT_EN_CLR__OVF__CLEAR(ip) \
	set__AUD_PCMIN_IT_EN_CLR__OVF(ip, \
	value__AUD_PCMIN_IT_EN_CLR__OVF__CLEAR(ip))

/* VSYNC */

#define shift__AUD_PCMIN_IT_EN_CLR__VSYNC(ip) 1
#define mask__AUD_PCMIN_IT_EN_CLR__VSYNC(ip) 0x1
#define get__AUD_PCMIN_IT_EN_CLR__VSYNC(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_IT_EN_CLR(ip)) >> \
	shift__AUD_PCMIN_IT_EN_CLR__VSYNC(ip)) & \
	mask__AUD_PCMIN_IT_EN_CLR__VSYNC(ip))
#define set__AUD_PCMIN_IT_EN_CLR__VSYNC(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMIN_IT_EN_CLR(ip)) & \
	~(mask__AUD_PCMIN_IT_EN_CLR__VSYNC(ip) << \
	shift__AUD_PCMIN_IT_EN_CLR__VSYNC(ip))) | (((value) & \
	mask__AUD_PCMIN_IT_EN_CLR__VSYNC(ip)) << \
	shift__AUD_PCMIN_IT_EN_CLR__VSYNC(ip)), ip->base + \
	offset__AUD_PCMIN_IT_EN_CLR(ip))

#define value__AUD_PCMIN_IT_EN_CLR__VSYNC__CLEAR(ip) 0x1
#define mask__AUD_PCMIN_IT_EN_CLR__VSYNC__CLEAR(ip) \
	(value__AUD_PCMIN_IT_EN_CLR__VSYNC__CLEAR(ip) << \
	shift__AUD_PCMIN_IT_EN_CLR__VSYNC(ip))
#define set__AUD_PCMIN_IT_EN_CLR__VSYNC__CLEAR(ip) \
	set__AUD_PCMIN_IT_EN_CLR__VSYNC(ip, \
	value__AUD_PCMIN_IT_EN_CLR__VSYNC__CLEAR(ip))



/*
 * AUD_PCMIN_CTRL
 */

#define offset__AUD_PCMIN_CTRL(ip) 0x1c
#define get__AUD_PCMIN_CTRL(ip) readl(ip->base + \
	offset__AUD_PCMIN_CTRL(ip))
#define set__AUD_PCMIN_CTRL(ip, value) writel(value, ip->base + \
	offset__AUD_PCMIN_CTRL(ip))

/* MODE */

#define shift__AUD_PCMIN_CTRL__MODE(ip) 0
#define mask__AUD_PCMIN_CTRL__MODE(ip) 0x3
#define get__AUD_PCMIN_CTRL__MODE(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_CTRL(ip)) >> shift__AUD_PCMIN_CTRL__MODE(ip)) & \
	mask__AUD_PCMIN_CTRL__MODE(ip))
#define set__AUD_PCMIN_CTRL__MODE(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMIN_CTRL(ip)) & ~(mask__AUD_PCMIN_CTRL__MODE(ip) << \
	shift__AUD_PCMIN_CTRL__MODE(ip))) | (((value) & \
	mask__AUD_PCMIN_CTRL__MODE(ip)) << shift__AUD_PCMIN_CTRL__MODE(ip)), \
	ip->base + offset__AUD_PCMIN_CTRL(ip))

#define value__AUD_PCMIN_CTRL__MODE__OFF(ip) 0x0
#define mask__AUD_PCMIN_CTRL__MODE__OFF(ip) \
	(value__AUD_PCMIN_CTRL__MODE__OFF(ip) << \
	shift__AUD_PCMIN_CTRL__MODE(ip))
#define set__AUD_PCMIN_CTRL__MODE__OFF(ip) \
	set__AUD_PCMIN_CTRL__MODE(ip, value__AUD_PCMIN_CTRL__MODE__OFF(ip))

#define value__AUD_PCMIN_CTRL__MODE__PCM(ip) 0x2
#define mask__AUD_PCMIN_CTRL__MODE__PCM(ip) \
	(value__AUD_PCMIN_CTRL__MODE__PCM(ip) << \
	shift__AUD_PCMIN_CTRL__MODE(ip))
#define set__AUD_PCMIN_CTRL__MODE__PCM(ip) \
	set__AUD_PCMIN_CTRL__MODE(ip, value__AUD_PCMIN_CTRL__MODE__PCM(ip))

#define value__AUD_PCMIN_CTRL__MODE__CD(ip) 0x3
#define mask__AUD_PCMIN_CTRL__MODE__CD(ip) \
	(value__AUD_PCMIN_CTRL__MODE__CD(ip) << \
	shift__AUD_PCMIN_CTRL__MODE(ip))
#define set__AUD_PCMIN_CTRL__MODE__CD(ip) \
	set__AUD_PCMIN_CTRL__MODE(ip, value__AUD_PCMIN_CTRL__MODE__CD(ip))

/* MEM_FMT */

#define shift__AUD_PCMIN_CTRL__MEM_FMT(ip) 2
#define mask__AUD_PCMIN_CTRL__MEM_FMT(ip) 0x1
#define get__AUD_PCMIN_CTRL__MEM_FMT(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_CTRL(ip)) >> shift__AUD_PCMIN_CTRL__MEM_FMT(ip)) & \
	mask__AUD_PCMIN_CTRL__MEM_FMT(ip))
#define set__AUD_PCMIN_CTRL__MEM_FMT(ip, value) writel((readl(ip->base \
	+ offset__AUD_PCMIN_CTRL(ip)) & ~(mask__AUD_PCMIN_CTRL__MEM_FMT(ip) << \
	shift__AUD_PCMIN_CTRL__MEM_FMT(ip))) | (((value) & \
	mask__AUD_PCMIN_CTRL__MEM_FMT(ip)) << \
	shift__AUD_PCMIN_CTRL__MEM_FMT(ip)), ip->base + \
	offset__AUD_PCMIN_CTRL(ip))

#define value__AUD_PCMIN_CTRL__MEM_FMT__16_BITS_0_BITS(ip) 0x0
#define mask__AUD_PCMIN_CTRL__MEM_FMT__16_BITS_0_BITS(ip) \
	(value__AUD_PCMIN_CTRL__MEM_FMT__16_BITS_0_BITS(ip) << \
	shift__AUD_PCMIN_CTRL__MEM_FMT(ip))
#define set__AUD_PCMIN_CTRL__MEM_FMT__16_BITS_0_BITS(ip) \
	set__AUD_PCMIN_CTRL__MEM_FMT(ip, \
	value__AUD_PCMIN_CTRL__MEM_FMT__16_BITS_0_BITS(ip))

#define value__AUD_PCMIN_CTRL__MEM_FMT__16_BITS_16_BITS(ip) 0x1
#define mask__AUD_PCMIN_CTRL__MEM_FMT__16_BITS_16_BITS(ip) \
	(value__AUD_PCMIN_CTRL__MEM_FMT__16_BITS_16_BITS(ip) << \
	shift__AUD_PCMIN_CTRL__MEM_FMT(ip))
#define set__AUD_PCMIN_CTRL__MEM_FMT__16_BITS_16_BITS(ip) \
	set__AUD_PCMIN_CTRL__MEM_FMT(ip, \
	value__AUD_PCMIN_CTRL__MEM_FMT__16_BITS_16_BITS(ip))

/* RND */

#define shift__AUD_PCMIN_CTRL__RND(ip) 3
#define mask__AUD_PCMIN_CTRL__RND(ip) 0x1
#define get__AUD_PCMIN_CTRL__RND(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_CTRL(ip)) >> shift__AUD_PCMIN_CTRL__RND(ip)) & \
	mask__AUD_PCMIN_CTRL__RND(ip))
#define set__AUD_PCMIN_CTRL__RND(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMIN_CTRL(ip)) & ~(mask__AUD_PCMIN_CTRL__RND(ip) << \
	shift__AUD_PCMIN_CTRL__RND(ip))) | (((value) & \
	mask__AUD_PCMIN_CTRL__RND(ip)) << shift__AUD_PCMIN_CTRL__RND(ip)), \
	ip->base + offset__AUD_PCMIN_CTRL(ip))

#define value__AUD_PCMIN_CTRL__RND__NO_ROUNDING(ip) 0x0
#define mask__AUD_PCMIN_CTRL__RND__NO_ROUNDING(ip) \
	(value__AUD_PCMIN_CTRL__RND__NO_ROUNDING(ip) << \
	shift__AUD_PCMIN_CTRL__RND(ip))
#define set__AUD_PCMIN_CTRL__RND__NO_ROUNDING(ip) \
	set__AUD_PCMIN_CTRL__RND(ip, \
	value__AUD_PCMIN_CTRL__RND__NO_ROUNDING(ip))

#define value__AUD_PCMIN_CTRL__RND__16_BITS_ROUNDING(ip) 0x1
#define mask__AUD_PCMIN_CTRL__RND__16_BITS_ROUNDING(ip) \
	(value__AUD_PCMIN_CTRL__RND__16_BITS_ROUNDING(ip) << \
	shift__AUD_PCMIN_CTRL__RND(ip))
#define set__AUD_PCMIN_CTRL__RND__16_BITS_ROUNDING(ip) \
	set__AUD_PCMIN_CTRL__RND(ip, \
	value__AUD_PCMIN_CTRL__RND__16_BITS_ROUNDING(ip))

/* NUM_FRAMES */

#define shift__AUD_PCMIN_CTRL__NUM_FRAMES(ip) (ip->ver < \
	4 ? 4 : -1)
#define mask__AUD_PCMIN_CTRL__NUM_FRAMES(ip) (ip->ver < \
	4 ? 0xfffffff : -1)
#define get__AUD_PCMIN_CTRL__NUM_FRAMES(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_CTRL(ip)) >> shift__AUD_PCMIN_CTRL__NUM_FRAMES(ip)) \
	& mask__AUD_PCMIN_CTRL__NUM_FRAMES(ip))
#define set__AUD_PCMIN_CTRL__NUM_FRAMES(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMIN_CTRL(ip)) & \
	~(mask__AUD_PCMIN_CTRL__NUM_FRAMES(ip) << \
	shift__AUD_PCMIN_CTRL__NUM_FRAMES(ip))) | (((value) & \
	mask__AUD_PCMIN_CTRL__NUM_FRAMES(ip)) << \
	shift__AUD_PCMIN_CTRL__NUM_FRAMES(ip)), ip->base + \
	offset__AUD_PCMIN_CTRL(ip))

/* MASTER_CLK_DIV */

#define shift__AUD_PCMIN_CTRL__MASTER_CLK_DIV(ip) (ip->ver < \
	4 ? -1 : 4)
#define mask__AUD_PCMIN_CTRL__MASTER_CLK_DIV(ip) (ip->ver < \
	4 ? -1 : 0xf)
#define get__AUD_PCMIN_CTRL__MASTER_CLK_DIV(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_CTRL(ip)) >> \
	shift__AUD_PCMIN_CTRL__MASTER_CLK_DIV(ip)) & \
	mask__AUD_PCMIN_CTRL__MASTER_CLK_DIV(ip))
#define set__AUD_PCMIN_CTRL__MASTER_CLK_DIV(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMIN_CTRL(ip)) & \
	~(mask__AUD_PCMIN_CTRL__MASTER_CLK_DIV(ip) << \
	shift__AUD_PCMIN_CTRL__MASTER_CLK_DIV(ip))) | (((value) & \
	mask__AUD_PCMIN_CTRL__MASTER_CLK_DIV(ip)) << \
	shift__AUD_PCMIN_CTRL__MASTER_CLK_DIV(ip)), ip->base + \
	offset__AUD_PCMIN_CTRL(ip))



/*
 * AUD_PCMIN_STA
 */

#define offset__AUD_PCMIN_STA(ip) 0x20
#define get__AUD_PCMIN_STA(ip) readl(ip->base + \
	offset__AUD_PCMIN_STA(ip))
#define set__AUD_PCMIN_STA(ip, value) writel(value, ip->base + \
	offset__AUD_PCMIN_STA(ip))

/* RUN_STOP */

#define shift__AUD_PCMIN_STA__RUN_STOP(ip) 0
#define mask__AUD_PCMIN_STA__RUN_STOP(ip) 0x1
#define get__AUD_PCMIN_STA__RUN_STOP(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_STA(ip)) >> shift__AUD_PCMIN_STA__RUN_STOP(ip)) & \
	mask__AUD_PCMIN_STA__RUN_STOP(ip))
#define set__AUD_PCMIN_STA__RUN_STOP(ip, value) writel((readl(ip->base \
	+ offset__AUD_PCMIN_STA(ip)) & ~(mask__AUD_PCMIN_STA__RUN_STOP(ip) << \
	shift__AUD_PCMIN_STA__RUN_STOP(ip))) | (((value) & \
	mask__AUD_PCMIN_STA__RUN_STOP(ip)) << \
	shift__AUD_PCMIN_STA__RUN_STOP(ip)), ip->base + \
	offset__AUD_PCMIN_STA(ip))

#define value__AUD_PCMIN_STA__RUN_STOP__STOPPED(ip) 0x0
#define mask__AUD_PCMIN_STA__RUN_STOP__STOPPED(ip) \
	(value__AUD_PCMIN_STA__RUN_STOP__STOPPED(ip) << \
	shift__AUD_PCMIN_STA__RUN_STOP(ip))
#define set__AUD_PCMIN_STA__RUN_STOP__STOPPED(ip) \
	set__AUD_PCMIN_STA__RUN_STOP(ip, \
	value__AUD_PCMIN_STA__RUN_STOP__STOPPED(ip))

#define value__AUD_PCMIN_STA__RUN_STOP__RUNNING(ip) 0x1
#define mask__AUD_PCMIN_STA__RUN_STOP__RUNNING(ip) \
	(value__AUD_PCMIN_STA__RUN_STOP__RUNNING(ip) << \
	shift__AUD_PCMIN_STA__RUN_STOP(ip))
#define set__AUD_PCMIN_STA__RUN_STOP__RUNNING(ip) \
	set__AUD_PCMIN_STA__RUN_STOP(ip, \
	value__AUD_PCMIN_STA__RUN_STOP__RUNNING(ip))

/* OVF */

#define shift__AUD_PCMIN_STA__OVF(ip) 1
#define mask__AUD_PCMIN_STA__OVF(ip) 0x1
#define get__AUD_PCMIN_STA__OVF(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_STA(ip)) >> shift__AUD_PCMIN_STA__OVF(ip)) & \
	mask__AUD_PCMIN_STA__OVF(ip))
#define set__AUD_PCMIN_STA__OVF(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMIN_STA(ip)) & ~(mask__AUD_PCMIN_STA__OVF(ip) << \
	shift__AUD_PCMIN_STA__OVF(ip))) | (((value) & \
	mask__AUD_PCMIN_STA__OVF(ip)) << shift__AUD_PCMIN_STA__OVF(ip)), \
	ip->base + offset__AUD_PCMIN_STA(ip))

#define value__AUD_PCMIN_STA__OVF__OVERFLOW_DETECTED(ip) 0x1
#define mask__AUD_PCMIN_STA__OVF__OVERFLOW_DETECTED(ip) \
	(value__AUD_PCMIN_STA__OVF__OVERFLOW_DETECTED(ip) << \
	shift__AUD_PCMIN_STA__OVF(ip))
#define set__AUD_PCMIN_STA__OVF__OVERFLOW_DETECTED(ip) \
	set__AUD_PCMIN_STA__OVF(ip, \
	value__AUD_PCMIN_STA__OVF__OVERFLOW_DETECTED(ip))

/* SAMPL_CNT */

#define shift__AUD_PCMIN_STA__SAMPL_CNT(ip) 2
#define mask__AUD_PCMIN_STA__SAMPL_CNT(ip) 0xffff
#define get__AUD_PCMIN_STA__SAMPL_CNT(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_STA(ip)) >> shift__AUD_PCMIN_STA__SAMPL_CNT(ip)) & \
	mask__AUD_PCMIN_STA__SAMPL_CNT(ip))
#define set__AUD_PCMIN_STA__SAMPL_CNT(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMIN_STA(ip)) & \
	~(mask__AUD_PCMIN_STA__SAMPL_CNT(ip) << \
	shift__AUD_PCMIN_STA__SAMPL_CNT(ip))) | (((value) & \
	mask__AUD_PCMIN_STA__SAMPL_CNT(ip)) << \
	shift__AUD_PCMIN_STA__SAMPL_CNT(ip)), ip->base + \
	offset__AUD_PCMIN_STA(ip))

/* VSYNC */

#define shift__AUD_PCMIN_STA__VSYNC(ip) 18
#define mask__AUD_PCMIN_STA__VSYNC(ip) 0x1
#define get__AUD_PCMIN_STA__VSYNC(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_STA(ip)) >> shift__AUD_PCMIN_STA__VSYNC(ip)) & \
	mask__AUD_PCMIN_STA__VSYNC(ip))
#define set__AUD_PCMIN_STA__VSYNC(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMIN_STA(ip)) & ~(mask__AUD_PCMIN_STA__VSYNC(ip) << \
	shift__AUD_PCMIN_STA__VSYNC(ip))) | (((value) & \
	mask__AUD_PCMIN_STA__VSYNC(ip)) << shift__AUD_PCMIN_STA__VSYNC(ip)), \
	ip->base + offset__AUD_PCMIN_STA(ip))

/* NFRAMES */

#define shift__AUD_PCMIN_STA__NFRAMES(ip) (ip->ver < \
	4 ? 19 : -1)
#define mask__AUD_PCMIN_STA__NFRAMES(ip) (ip->ver < \
	4 ? 0x1 : -1)
#define get__AUD_PCMIN_STA__NFRAMES(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_STA(ip)) >> shift__AUD_PCMIN_STA__NFRAMES(ip)) & \
	mask__AUD_PCMIN_STA__NFRAMES(ip))
#define set__AUD_PCMIN_STA__NFRAMES(ip, value) writel((readl(ip->base \
	+ offset__AUD_PCMIN_STA(ip)) & ~(mask__AUD_PCMIN_STA__NFRAMES(ip) << \
	shift__AUD_PCMIN_STA__NFRAMES(ip))) | (((value) & \
	mask__AUD_PCMIN_STA__NFRAMES(ip)) << \
	shift__AUD_PCMIN_STA__NFRAMES(ip)), ip->base + \
	offset__AUD_PCMIN_STA(ip))

#define value__AUD_PCMIN_STA__NFRAMES__DONE(ip) (ip->ver < \
	4 ? 0x1 : -1)
#define mask__AUD_PCMIN_STA__NFRAMES__DONE(ip) \
	(value__AUD_PCMIN_STA__NFRAMES__DONE(ip) << \
	shift__AUD_PCMIN_STA__NFRAMES(ip))
#define set__AUD_PCMIN_STA__NFRAMES__DONE(ip) \
	set__AUD_PCMIN_STA__NFRAMES(ip, \
	value__AUD_PCMIN_STA__NFRAMES__DONE(ip))

/* SAMPLES_IN_FIFO */

#define shift__AUD_PCMIN_STA__SAMPLES_IN_FIFO(ip) (ip->ver < \
	4 ? -1 : 19)
#define mask__AUD_PCMIN_STA__SAMPLES_IN_FIFO(ip) (ip->ver < \
	4 ? -1 : 0x7f)
#define get__AUD_PCMIN_STA__SAMPLES_IN_FIFO(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_STA(ip)) >> \
	shift__AUD_PCMIN_STA__SAMPLES_IN_FIFO(ip)) & \
	mask__AUD_PCMIN_STA__SAMPLES_IN_FIFO(ip))
#define set__AUD_PCMIN_STA__SAMPLES_IN_FIFO(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMIN_STA(ip)) & \
	~(mask__AUD_PCMIN_STA__SAMPLES_IN_FIFO(ip) << \
	shift__AUD_PCMIN_STA__SAMPLES_IN_FIFO(ip))) | (((value) & \
	mask__AUD_PCMIN_STA__SAMPLES_IN_FIFO(ip)) << \
	shift__AUD_PCMIN_STA__SAMPLES_IN_FIFO(ip)), ip->base + \
	offset__AUD_PCMIN_STA(ip))



/*
 * AUD_PCMIN_FMT
 */

#define offset__AUD_PCMIN_FMT(ip) 0x24
#define get__AUD_PCMIN_FMT(ip) readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip))
#define set__AUD_PCMIN_FMT(ip, value) writel(value, ip->base + \
	offset__AUD_PCMIN_FMT(ip))

/* NBIT */

#define shift__AUD_PCMIN_FMT__NBIT(ip) 0
#define mask__AUD_PCMIN_FMT__NBIT(ip) 0x1
#define get__AUD_PCMIN_FMT__NBIT(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) >> shift__AUD_PCMIN_FMT__NBIT(ip)) & \
	mask__AUD_PCMIN_FMT__NBIT(ip))
#define set__AUD_PCMIN_FMT__NBIT(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) & ~(mask__AUD_PCMIN_FMT__NBIT(ip) << \
	shift__AUD_PCMIN_FMT__NBIT(ip))) | (((value) & \
	mask__AUD_PCMIN_FMT__NBIT(ip)) << shift__AUD_PCMIN_FMT__NBIT(ip)), \
	ip->base + offset__AUD_PCMIN_FMT(ip))

#define value__AUD_PCMIN_FMT__NBIT__32_BITS(ip) 0x0
#define mask__AUD_PCMIN_FMT__NBIT__32_BITS(ip) \
	(value__AUD_PCMIN_FMT__NBIT__32_BITS(ip) << \
	shift__AUD_PCMIN_FMT__NBIT(ip))
#define set__AUD_PCMIN_FMT__NBIT__32_BITS(ip) \
	set__AUD_PCMIN_FMT__NBIT(ip, value__AUD_PCMIN_FMT__NBIT__32_BITS(ip))

#define value__AUD_PCMIN_FMT__NBIT__16_BITS(ip) 0x1
#define mask__AUD_PCMIN_FMT__NBIT__16_BITS(ip) \
	(value__AUD_PCMIN_FMT__NBIT__16_BITS(ip) << \
	shift__AUD_PCMIN_FMT__NBIT(ip))
#define set__AUD_PCMIN_FMT__NBIT__16_BITS(ip) \
	set__AUD_PCMIN_FMT__NBIT(ip, value__AUD_PCMIN_FMT__NBIT__16_BITS(ip))

/* DATA_SIZE */

#define shift__AUD_PCMIN_FMT__DATA_SIZE(ip) 1
#define mask__AUD_PCMIN_FMT__DATA_SIZE(ip) 0x3
#define get__AUD_PCMIN_FMT__DATA_SIZE(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) >> shift__AUD_PCMIN_FMT__DATA_SIZE(ip)) & \
	mask__AUD_PCMIN_FMT__DATA_SIZE(ip))
#define set__AUD_PCMIN_FMT__DATA_SIZE(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMIN_FMT(ip)) & \
	~(mask__AUD_PCMIN_FMT__DATA_SIZE(ip) << \
	shift__AUD_PCMIN_FMT__DATA_SIZE(ip))) | (((value) & \
	mask__AUD_PCMIN_FMT__DATA_SIZE(ip)) << \
	shift__AUD_PCMIN_FMT__DATA_SIZE(ip)), ip->base + \
	offset__AUD_PCMIN_FMT(ip))

#define value__AUD_PCMIN_FMT__DATA_SIZE__24_BITS(ip) 0x0
#define mask__AUD_PCMIN_FMT__DATA_SIZE__24_BITS(ip) \
	(value__AUD_PCMIN_FMT__DATA_SIZE__24_BITS(ip) << \
	shift__AUD_PCMIN_FMT__DATA_SIZE(ip))
#define set__AUD_PCMIN_FMT__DATA_SIZE__24_BITS(ip) \
	set__AUD_PCMIN_FMT__DATA_SIZE(ip, \
	value__AUD_PCMIN_FMT__DATA_SIZE__24_BITS(ip))

#define value__AUD_PCMIN_FMT__DATA_SIZE__20_BITS(ip) 0x1
#define mask__AUD_PCMIN_FMT__DATA_SIZE__20_BITS(ip) \
	(value__AUD_PCMIN_FMT__DATA_SIZE__20_BITS(ip) << \
	shift__AUD_PCMIN_FMT__DATA_SIZE(ip))
#define set__AUD_PCMIN_FMT__DATA_SIZE__20_BITS(ip) \
	set__AUD_PCMIN_FMT__DATA_SIZE(ip, \
	value__AUD_PCMIN_FMT__DATA_SIZE__20_BITS(ip))

#define value__AUD_PCMIN_FMT__DATA_SIZE__18_BITS(ip) 0x2
#define mask__AUD_PCMIN_FMT__DATA_SIZE__18_BITS(ip) \
	(value__AUD_PCMIN_FMT__DATA_SIZE__18_BITS(ip) << \
	shift__AUD_PCMIN_FMT__DATA_SIZE(ip))
#define set__AUD_PCMIN_FMT__DATA_SIZE__18_BITS(ip) \
	set__AUD_PCMIN_FMT__DATA_SIZE(ip, \
	value__AUD_PCMIN_FMT__DATA_SIZE__18_BITS(ip))

#define value__AUD_PCMIN_FMT__DATA_SIZE__16_BITS(ip) 0x3
#define mask__AUD_PCMIN_FMT__DATA_SIZE__16_BITS(ip) \
	(value__AUD_PCMIN_FMT__DATA_SIZE__16_BITS(ip) << \
	shift__AUD_PCMIN_FMT__DATA_SIZE(ip))
#define set__AUD_PCMIN_FMT__DATA_SIZE__16_BITS(ip) \
	set__AUD_PCMIN_FMT__DATA_SIZE(ip, \
	value__AUD_PCMIN_FMT__DATA_SIZE__16_BITS(ip))

/* LR_POL */

#define shift__AUD_PCMIN_FMT__LR_POL(ip) 3
#define mask__AUD_PCMIN_FMT__LR_POL(ip) 0x1
#define get__AUD_PCMIN_FMT__LR_POL(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) >> shift__AUD_PCMIN_FMT__LR_POL(ip)) & \
	mask__AUD_PCMIN_FMT__LR_POL(ip))
#define set__AUD_PCMIN_FMT__LR_POL(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) & ~(mask__AUD_PCMIN_FMT__LR_POL(ip) << \
	shift__AUD_PCMIN_FMT__LR_POL(ip))) | (((value) & \
	mask__AUD_PCMIN_FMT__LR_POL(ip)) << shift__AUD_PCMIN_FMT__LR_POL(ip)), \
	ip->base + offset__AUD_PCMIN_FMT(ip))

#define value__AUD_PCMIN_FMT__LR_POL__LEFT_LOW(ip) 0x0
#define mask__AUD_PCMIN_FMT__LR_POL__LEFT_LOW(ip) \
	(value__AUD_PCMIN_FMT__LR_POL__LEFT_LOW(ip) << \
	shift__AUD_PCMIN_FMT__LR_POL(ip))
#define set__AUD_PCMIN_FMT__LR_POL__LEFT_LOW(ip) \
	set__AUD_PCMIN_FMT__LR_POL(ip, \
	value__AUD_PCMIN_FMT__LR_POL__LEFT_LOW(ip))

#define value__AUD_PCMIN_FMT__LR_POL__LEFT_HIGH(ip) 0x1
#define mask__AUD_PCMIN_FMT__LR_POL__LEFT_HIGH(ip) \
	(value__AUD_PCMIN_FMT__LR_POL__LEFT_HIGH(ip) << \
	shift__AUD_PCMIN_FMT__LR_POL(ip))
#define set__AUD_PCMIN_FMT__LR_POL__LEFT_HIGH(ip) \
	set__AUD_PCMIN_FMT__LR_POL(ip, \
	value__AUD_PCMIN_FMT__LR_POL__LEFT_HIGH(ip))

/* SCLK_EDGE */

#define shift__AUD_PCMIN_FMT__SCLK_EDGE(ip) 4
#define mask__AUD_PCMIN_FMT__SCLK_EDGE(ip) 0x1
#define get__AUD_PCMIN_FMT__SCLK_EDGE(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) >> shift__AUD_PCMIN_FMT__SCLK_EDGE(ip)) & \
	mask__AUD_PCMIN_FMT__SCLK_EDGE(ip))
#define set__AUD_PCMIN_FMT__SCLK_EDGE(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMIN_FMT(ip)) & \
	~(mask__AUD_PCMIN_FMT__SCLK_EDGE(ip) << \
	shift__AUD_PCMIN_FMT__SCLK_EDGE(ip))) | (((value) & \
	mask__AUD_PCMIN_FMT__SCLK_EDGE(ip)) << \
	shift__AUD_PCMIN_FMT__SCLK_EDGE(ip)), ip->base + \
	offset__AUD_PCMIN_FMT(ip))

#define value__AUD_PCMIN_FMT__SCLK_EDGE__RISING(ip) 0x0
#define mask__AUD_PCMIN_FMT__SCLK_EDGE__RISING(ip) \
	(value__AUD_PCMIN_FMT__SCLK_EDGE__RISING(ip) << \
	shift__AUD_PCMIN_FMT__SCLK_EDGE(ip))
#define set__AUD_PCMIN_FMT__SCLK_EDGE__RISING(ip) \
	set__AUD_PCMIN_FMT__SCLK_EDGE(ip, \
	value__AUD_PCMIN_FMT__SCLK_EDGE__RISING(ip))

#define value__AUD_PCMIN_FMT__SCLK_EDGE__FALLING(ip) 0x1
#define mask__AUD_PCMIN_FMT__SCLK_EDGE__FALLING(ip) \
	(value__AUD_PCMIN_FMT__SCLK_EDGE__FALLING(ip) << \
	shift__AUD_PCMIN_FMT__SCLK_EDGE(ip))
#define set__AUD_PCMIN_FMT__SCLK_EDGE__FALLING(ip) \
	set__AUD_PCMIN_FMT__SCLK_EDGE(ip, \
	value__AUD_PCMIN_FMT__SCLK_EDGE__FALLING(ip))

/* PADDING */

#define shift__AUD_PCMIN_FMT__PADDING(ip) 5
#define mask__AUD_PCMIN_FMT__PADDING(ip) 0x1
#define get__AUD_PCMIN_FMT__PADDING(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) >> shift__AUD_PCMIN_FMT__PADDING(ip)) & \
	mask__AUD_PCMIN_FMT__PADDING(ip))
#define set__AUD_PCMIN_FMT__PADDING(ip, value) writel((readl(ip->base \
	+ offset__AUD_PCMIN_FMT(ip)) & ~(mask__AUD_PCMIN_FMT__PADDING(ip) << \
	shift__AUD_PCMIN_FMT__PADDING(ip))) | (((value) & \
	mask__AUD_PCMIN_FMT__PADDING(ip)) << \
	shift__AUD_PCMIN_FMT__PADDING(ip)), ip->base + \
	offset__AUD_PCMIN_FMT(ip))

#define value__AUD_PCMIN_FMT__PADDING__1_CYCLE_DELAY(ip) 0x0
#define mask__AUD_PCMIN_FMT__PADDING__1_CYCLE_DELAY(ip) \
	(value__AUD_PCMIN_FMT__PADDING__1_CYCLE_DELAY(ip) << \
	shift__AUD_PCMIN_FMT__PADDING(ip))
#define set__AUD_PCMIN_FMT__PADDING__1_CYCLE_DELAY(ip) \
	set__AUD_PCMIN_FMT__PADDING(ip, \
	value__AUD_PCMIN_FMT__PADDING__1_CYCLE_DELAY(ip))

#define value__AUD_PCMIN_FMT__PADDING__NO_DELAY(ip) 0x1
#define mask__AUD_PCMIN_FMT__PADDING__NO_DELAY(ip) \
	(value__AUD_PCMIN_FMT__PADDING__NO_DELAY(ip) << \
	shift__AUD_PCMIN_FMT__PADDING(ip))
#define set__AUD_PCMIN_FMT__PADDING__NO_DELAY(ip) \
	set__AUD_PCMIN_FMT__PADDING(ip, \
	value__AUD_PCMIN_FMT__PADDING__NO_DELAY(ip))

/* ALIGN */

#define shift__AUD_PCMIN_FMT__ALIGN(ip) 6
#define mask__AUD_PCMIN_FMT__ALIGN(ip) 0x1
#define get__AUD_PCMIN_FMT__ALIGN(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) >> shift__AUD_PCMIN_FMT__ALIGN(ip)) & \
	mask__AUD_PCMIN_FMT__ALIGN(ip))
#define set__AUD_PCMIN_FMT__ALIGN(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) & ~(mask__AUD_PCMIN_FMT__ALIGN(ip) << \
	shift__AUD_PCMIN_FMT__ALIGN(ip))) | (((value) & \
	mask__AUD_PCMIN_FMT__ALIGN(ip)) << shift__AUD_PCMIN_FMT__ALIGN(ip)), \
	ip->base + offset__AUD_PCMIN_FMT(ip))

#define value__AUD_PCMIN_FMT__ALIGN__RIGHT(ip) 0x0
#define mask__AUD_PCMIN_FMT__ALIGN__RIGHT(ip) \
	(value__AUD_PCMIN_FMT__ALIGN__RIGHT(ip) << \
	shift__AUD_PCMIN_FMT__ALIGN(ip))
#define set__AUD_PCMIN_FMT__ALIGN__RIGHT(ip) \
	set__AUD_PCMIN_FMT__ALIGN(ip, value__AUD_PCMIN_FMT__ALIGN__RIGHT(ip))

#define value__AUD_PCMIN_FMT__ALIGN__LEFT(ip) 0x1
#define mask__AUD_PCMIN_FMT__ALIGN__LEFT(ip) \
	(value__AUD_PCMIN_FMT__ALIGN__LEFT(ip) << \
	shift__AUD_PCMIN_FMT__ALIGN(ip))
#define set__AUD_PCMIN_FMT__ALIGN__LEFT(ip) \
	set__AUD_PCMIN_FMT__ALIGN(ip, value__AUD_PCMIN_FMT__ALIGN__LEFT(ip))

/* ORDER */

#define shift__AUD_PCMIN_FMT__ORDER(ip) 7
#define mask__AUD_PCMIN_FMT__ORDER(ip) 0x1
#define get__AUD_PCMIN_FMT__ORDER(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) >> shift__AUD_PCMIN_FMT__ORDER(ip)) & \
	mask__AUD_PCMIN_FMT__ORDER(ip))
#define set__AUD_PCMIN_FMT__ORDER(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) & ~(mask__AUD_PCMIN_FMT__ORDER(ip) << \
	shift__AUD_PCMIN_FMT__ORDER(ip))) | (((value) & \
	mask__AUD_PCMIN_FMT__ORDER(ip)) << shift__AUD_PCMIN_FMT__ORDER(ip)), \
	ip->base + offset__AUD_PCMIN_FMT(ip))

#define value__AUD_PCMIN_FMT__ORDER__LSB_FIRST(ip) 0x0
#define mask__AUD_PCMIN_FMT__ORDER__LSB_FIRST(ip) \
	(value__AUD_PCMIN_FMT__ORDER__LSB_FIRST(ip) << \
	shift__AUD_PCMIN_FMT__ORDER(ip))
#define set__AUD_PCMIN_FMT__ORDER__LSB_FIRST(ip) \
	set__AUD_PCMIN_FMT__ORDER(ip, \
	value__AUD_PCMIN_FMT__ORDER__LSB_FIRST(ip))

#define value__AUD_PCMIN_FMT__ORDER__MSB_FIRST(ip) 0x1
#define mask__AUD_PCMIN_FMT__ORDER__MSB_FIRST(ip) \
	(value__AUD_PCMIN_FMT__ORDER__MSB_FIRST(ip) << \
	shift__AUD_PCMIN_FMT__ORDER(ip))
#define set__AUD_PCMIN_FMT__ORDER__MSB_FIRST(ip) \
	set__AUD_PCMIN_FMT__ORDER(ip, \
	value__AUD_PCMIN_FMT__ORDER__MSB_FIRST(ip))

/* NUM_CH */

#define shift__AUD_PCMIN_FMT__NUM_CH(ip) (ip->ver < \
	4 ? -1 : 8)
#define mask__AUD_PCMIN_FMT__NUM_CH(ip) (ip->ver < \
	4 ? -1 : 0x7)
#define get__AUD_PCMIN_FMT__NUM_CH(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) >> shift__AUD_PCMIN_FMT__NUM_CH(ip)) & \
	mask__AUD_PCMIN_FMT__NUM_CH(ip))
#define set__AUD_PCMIN_FMT__NUM_CH(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) & ~(mask__AUD_PCMIN_FMT__NUM_CH(ip) << \
	shift__AUD_PCMIN_FMT__NUM_CH(ip))) | (((value) & \
	mask__AUD_PCMIN_FMT__NUM_CH(ip)) << shift__AUD_PCMIN_FMT__NUM_CH(ip)), \
	ip->base + offset__AUD_PCMIN_FMT(ip))

#define value__AUD_PCMIN_FMT__NUM_CH__1_CHANNEL(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define mask__AUD_PCMIN_FMT__NUM_CH__1_CHANNEL(ip) \
	(value__AUD_PCMIN_FMT__NUM_CH__1_CHANNEL(ip) << \
	shift__AUD_PCMIN_FMT__NUM_CH(ip))
#define set__AUD_PCMIN_FMT__NUM_CH__1_CHANNEL(ip) \
	set__AUD_PCMIN_FMT__NUM_CH(ip, \
	value__AUD_PCMIN_FMT__NUM_CH__1_CHANNEL(ip))

#define value__AUD_PCMIN_FMT__NUM_CH__2_CHANNELS(ip) (ip->ver < \
	4 ? -1 : 0x2)
#define mask__AUD_PCMIN_FMT__NUM_CH__2_CHANNELS(ip) \
	(value__AUD_PCMIN_FMT__NUM_CH__2_CHANNELS(ip) << \
	shift__AUD_PCMIN_FMT__NUM_CH(ip))
#define set__AUD_PCMIN_FMT__NUM_CH__2_CHANNELS(ip) \
	set__AUD_PCMIN_FMT__NUM_CH(ip, \
	value__AUD_PCMIN_FMT__NUM_CH__2_CHANNELS(ip))

#define value__AUD_PCMIN_FMT__NUM_CH__3_CHANNELS(ip) (ip->ver < \
	4 ? -1 : 0x3)
#define mask__AUD_PCMIN_FMT__NUM_CH__3_CHANNELS(ip) \
	(value__AUD_PCMIN_FMT__NUM_CH__3_CHANNELS(ip) << \
	shift__AUD_PCMIN_FMT__NUM_CH(ip))
#define set__AUD_PCMIN_FMT__NUM_CH__3_CHANNELS(ip) \
	set__AUD_PCMIN_FMT__NUM_CH(ip, \
	value__AUD_PCMIN_FMT__NUM_CH__3_CHANNELS(ip))

#define value__AUD_PCMIN_FMT__NUM_CH__4_CHANNELS(ip) (ip->ver < \
	4 ? -1 : 0x4)
#define mask__AUD_PCMIN_FMT__NUM_CH__4_CHANNELS(ip) \
	(value__AUD_PCMIN_FMT__NUM_CH__4_CHANNELS(ip) << \
	shift__AUD_PCMIN_FMT__NUM_CH(ip))
#define set__AUD_PCMIN_FMT__NUM_CH__4_CHANNELS(ip) \
	set__AUD_PCMIN_FMT__NUM_CH(ip, \
	value__AUD_PCMIN_FMT__NUM_CH__4_CHANNELS(ip))

#define value__AUD_PCMIN_FMT__NUM_CH__5_CHANNELS(ip) (ip->ver < \
	4 ? -1 : 0x5)
#define mask__AUD_PCMIN_FMT__NUM_CH__5_CHANNELS(ip) \
	(value__AUD_PCMIN_FMT__NUM_CH__5_CHANNELS(ip) << \
	shift__AUD_PCMIN_FMT__NUM_CH(ip))
#define set__AUD_PCMIN_FMT__NUM_CH__5_CHANNELS(ip) \
	set__AUD_PCMIN_FMT__NUM_CH(ip, \
	value__AUD_PCMIN_FMT__NUM_CH__5_CHANNELS(ip))

/* BACK_STALLING */

#define shift__AUD_PCMIN_FMT__BACK_STALLING(ip) (ip->ver < \
	4 ? -1 : 11)
#define mask__AUD_PCMIN_FMT__BACK_STALLING(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define get__AUD_PCMIN_FMT__BACK_STALLING(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) >> shift__AUD_PCMIN_FMT__BACK_STALLING(ip)) \
	& mask__AUD_PCMIN_FMT__BACK_STALLING(ip))
#define set__AUD_PCMIN_FMT__BACK_STALLING(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMIN_FMT(ip)) & \
	~(mask__AUD_PCMIN_FMT__BACK_STALLING(ip) << \
	shift__AUD_PCMIN_FMT__BACK_STALLING(ip))) | (((value) & \
	mask__AUD_PCMIN_FMT__BACK_STALLING(ip)) << \
	shift__AUD_PCMIN_FMT__BACK_STALLING(ip)), ip->base + \
	offset__AUD_PCMIN_FMT(ip))

#define value__AUD_PCMIN_FMT__BACK_STALLING__DISABLED(ip) (ip->ver < \
	4 ? -1 : 0x0)
#define mask__AUD_PCMIN_FMT__BACK_STALLING__DISABLED(ip) \
	(value__AUD_PCMIN_FMT__BACK_STALLING__DISABLED(ip) << \
	shift__AUD_PCMIN_FMT__BACK_STALLING(ip))
#define set__AUD_PCMIN_FMT__BACK_STALLING__DISABLED(ip) \
	set__AUD_PCMIN_FMT__BACK_STALLING(ip, \
	value__AUD_PCMIN_FMT__BACK_STALLING__DISABLED(ip))

#define value__AUD_PCMIN_FMT__BACK_STALLING__ENABLED(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define mask__AUD_PCMIN_FMT__BACK_STALLING__ENABLED(ip) \
	(value__AUD_PCMIN_FMT__BACK_STALLING__ENABLED(ip) << \
	shift__AUD_PCMIN_FMT__BACK_STALLING(ip))
#define set__AUD_PCMIN_FMT__BACK_STALLING__ENABLED(ip) \
	set__AUD_PCMIN_FMT__BACK_STALLING(ip, \
	value__AUD_PCMIN_FMT__BACK_STALLING__ENABLED(ip))

/* MASTER_CLKEDGE */

#define shift__AUD_PCMIN_FMT__MASTER_CLKEDGE(ip) (ip->ver < \
	4 ? -1 : 12)
#define mask__AUD_PCMIN_FMT__MASTER_CLKEDGE(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define get__AUD_PCMIN_FMT__MASTER_CLKEDGE(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) >> \
	shift__AUD_PCMIN_FMT__MASTER_CLKEDGE(ip)) & \
	mask__AUD_PCMIN_FMT__MASTER_CLKEDGE(ip))
#define set__AUD_PCMIN_FMT__MASTER_CLKEDGE(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMIN_FMT(ip)) & \
	~(mask__AUD_PCMIN_FMT__MASTER_CLKEDGE(ip) << \
	shift__AUD_PCMIN_FMT__MASTER_CLKEDGE(ip))) | (((value) & \
	mask__AUD_PCMIN_FMT__MASTER_CLKEDGE(ip)) << \
	shift__AUD_PCMIN_FMT__MASTER_CLKEDGE(ip)), ip->base + \
	offset__AUD_PCMIN_FMT(ip))

#define value__AUD_PCMIN_FMT__MASTER_CLKEDGE__RISING(ip) (ip->ver < \
	4 ? -1 : 0x0)
#define mask__AUD_PCMIN_FMT__MASTER_CLKEDGE__RISING(ip) \
	(value__AUD_PCMIN_FMT__MASTER_CLKEDGE__RISING(ip) << \
	shift__AUD_PCMIN_FMT__MASTER_CLKEDGE(ip))
#define set__AUD_PCMIN_FMT__MASTER_CLKEDGE__RISING(ip) \
	set__AUD_PCMIN_FMT__MASTER_CLKEDGE(ip, \
	value__AUD_PCMIN_FMT__MASTER_CLKEDGE__RISING(ip))

#define value__AUD_PCMIN_FMT__MASTER_CLKEDGE__FALLING(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define mask__AUD_PCMIN_FMT__MASTER_CLKEDGE__FALLING(ip) \
	(value__AUD_PCMIN_FMT__MASTER_CLKEDGE__FALLING(ip) << \
	shift__AUD_PCMIN_FMT__MASTER_CLKEDGE(ip))
#define set__AUD_PCMIN_FMT__MASTER_CLKEDGE__FALLING(ip) \
	set__AUD_PCMIN_FMT__MASTER_CLKEDGE(ip, \
	value__AUD_PCMIN_FMT__MASTER_CLKEDGE__FALLING(ip))

/* DMA_REQ_TRIG_LMT */

#define shift__AUD_PCMIN_FMT__DMA_REQ_TRIG_LMT(ip) (ip->ver < \
	4 ? -1 : 13)
#define mask__AUD_PCMIN_FMT__DMA_REQ_TRIG_LMT(ip) (ip->ver < \
	4 ? -1 : 0x7f)
#define get__AUD_PCMIN_FMT__DMA_REQ_TRIG_LMT(ip) ((readl(ip->base + \
	offset__AUD_PCMIN_FMT(ip)) >> \
	shift__AUD_PCMIN_FMT__DMA_REQ_TRIG_LMT(ip)) & \
	mask__AUD_PCMIN_FMT__DMA_REQ_TRIG_LMT(ip))
#define set__AUD_PCMIN_FMT__DMA_REQ_TRIG_LMT(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMIN_FMT(ip)) & \
	~(mask__AUD_PCMIN_FMT__DMA_REQ_TRIG_LMT(ip) << \
	shift__AUD_PCMIN_FMT__DMA_REQ_TRIG_LMT(ip))) | (((value) & \
	mask__AUD_PCMIN_FMT__DMA_REQ_TRIG_LMT(ip)) << \
	shift__AUD_PCMIN_FMT__DMA_REQ_TRIG_LMT(ip)), ip->base + \
	offset__AUD_PCMIN_FMT(ip))



#endif
