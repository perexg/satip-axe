#ifndef __SND_STM_AUD_PCMOUT_H
#define __SND_STM_AUD_PCMOUT_H

/*
 * AUD_PCMOUT_RST
 */

#define offset__AUD_PCMOUT_RST(ip) 0x00
#define get__AUD_PCMOUT_RST(ip) readl(ip->base + \
	offset__AUD_PCMOUT_RST(ip))
#define set__AUD_PCMOUT_RST(ip, value) writel(value, ip->base + \
	offset__AUD_PCMOUT_RST(ip))

/* SRSTP */

#define shift__AUD_PCMOUT_RST__SRSTP(ip) 0
#define mask__AUD_PCMOUT_RST__SRSTP(ip) 0x1
#define get__AUD_PCMOUT_RST__SRSTP(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_RST(ip)) >> shift__AUD_PCMOUT_RST__SRSTP(ip)) & \
	mask__AUD_PCMOUT_RST__SRSTP(ip))
#define set__AUD_PCMOUT_RST__SRSTP(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMOUT_RST(ip)) & ~(mask__AUD_PCMOUT_RST__SRSTP(ip) << \
	shift__AUD_PCMOUT_RST__SRSTP(ip))) | (((value) & \
	mask__AUD_PCMOUT_RST__SRSTP(ip)) << shift__AUD_PCMOUT_RST__SRSTP(ip)), \
	ip->base + offset__AUD_PCMOUT_RST(ip))

#define value__AUD_PCMOUT_RST__SRSTP__RUNNING(ip) 0x0
#define mask__AUD_PCMOUT_RST__SRSTP__RUNNING(ip) \
	(value__AUD_PCMOUT_RST__SRSTP__RUNNING(ip) << \
	shift__AUD_PCMOUT_RST__SRSTP(ip))
#define set__AUD_PCMOUT_RST__SRSTP__RUNNING(ip) \
	set__AUD_PCMOUT_RST__SRSTP(ip, \
	value__AUD_PCMOUT_RST__SRSTP__RUNNING(ip))

#define value__AUD_PCMOUT_RST__SRSTP__RESET(ip) 0x1
#define mask__AUD_PCMOUT_RST__SRSTP__RESET(ip) \
	(value__AUD_PCMOUT_RST__SRSTP__RESET(ip) << \
	shift__AUD_PCMOUT_RST__SRSTP(ip))
#define set__AUD_PCMOUT_RST__SRSTP__RESET(ip) \
	set__AUD_PCMOUT_RST__SRSTP(ip, \
	value__AUD_PCMOUT_RST__SRSTP__RESET(ip))



/*
 * AUD_PCMOUT_DATA
 */

#define offset__AUD_PCMOUT_DATA(ip) 0x04
#define get__AUD_PCMOUT_DATA(ip) readl(ip->base + \
	offset__AUD_PCMOUT_DATA(ip))
#define set__AUD_PCMOUT_DATA(ip, value) writel(value, ip->base + \
	offset__AUD_PCMOUT_DATA(ip))

/* DATA */

#define shift__AUD_PCMOUT_DATA__DATA(ip) 0
#define mask__AUD_PCMOUT_DATA__DATA(ip) 0xffffffff
#define get__AUD_PCMOUT_DATA__DATA(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_DATA(ip)) >> shift__AUD_PCMOUT_DATA__DATA(ip)) & \
	mask__AUD_PCMOUT_DATA__DATA(ip))
#define set__AUD_PCMOUT_DATA__DATA(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMOUT_DATA(ip)) & ~(mask__AUD_PCMOUT_DATA__DATA(ip) << \
	shift__AUD_PCMOUT_DATA__DATA(ip))) | (((value) & \
	mask__AUD_PCMOUT_DATA__DATA(ip)) << shift__AUD_PCMOUT_DATA__DATA(ip)), \
	ip->base + offset__AUD_PCMOUT_DATA(ip))



/*
 * AUD_PCMOUT_ITS
 */

#define offset__AUD_PCMOUT_ITS(ip) 0x08
#define get__AUD_PCMOUT_ITS(ip) readl(ip->base + \
	offset__AUD_PCMOUT_ITS(ip))
#define set__AUD_PCMOUT_ITS(ip, value) writel(value, ip->base + \
	offset__AUD_PCMOUT_ITS(ip))

/* UNF */

#define shift__AUD_PCMOUT_ITS__UNF(ip) 0
#define mask__AUD_PCMOUT_ITS__UNF(ip) 0x1
#define get__AUD_PCMOUT_ITS__UNF(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_ITS(ip)) >> shift__AUD_PCMOUT_ITS__UNF(ip)) & \
	mask__AUD_PCMOUT_ITS__UNF(ip))
#define set__AUD_PCMOUT_ITS__UNF(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMOUT_ITS(ip)) & ~(mask__AUD_PCMOUT_ITS__UNF(ip) << \
	shift__AUD_PCMOUT_ITS__UNF(ip))) | (((value) & \
	mask__AUD_PCMOUT_ITS__UNF(ip)) << shift__AUD_PCMOUT_ITS__UNF(ip)), \
	ip->base + offset__AUD_PCMOUT_ITS(ip))

#define value__AUD_PCMOUT_ITS__UNF__PENDING(ip) 0x1
#define mask__AUD_PCMOUT_ITS__UNF__PENDING(ip) \
	(value__AUD_PCMOUT_ITS__UNF__PENDING(ip) << \
	shift__AUD_PCMOUT_ITS__UNF(ip))
#define set__AUD_PCMOUT_ITS__UNF__PENDING(ip) \
	set__AUD_PCMOUT_ITS__UNF(ip, value__AUD_PCMOUT_ITS__UNF__PENDING(ip))

/* NSAMPLE */

#define shift__AUD_PCMOUT_ITS__NSAMPLE(ip) 1
#define mask__AUD_PCMOUT_ITS__NSAMPLE(ip) 0x1
#define get__AUD_PCMOUT_ITS__NSAMPLE(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_ITS(ip)) >> shift__AUD_PCMOUT_ITS__NSAMPLE(ip)) & \
	mask__AUD_PCMOUT_ITS__NSAMPLE(ip))
#define set__AUD_PCMOUT_ITS__NSAMPLE(ip, value) writel((readl(ip->base \
	+ offset__AUD_PCMOUT_ITS(ip)) & ~(mask__AUD_PCMOUT_ITS__NSAMPLE(ip) << \
	shift__AUD_PCMOUT_ITS__NSAMPLE(ip))) | (((value) & \
	mask__AUD_PCMOUT_ITS__NSAMPLE(ip)) << \
	shift__AUD_PCMOUT_ITS__NSAMPLE(ip)), ip->base + \
	offset__AUD_PCMOUT_ITS(ip))

#define value__AUD_PCMOUT_ITS__NSAMPLE__PENDING(ip) 0x1
#define mask__AUD_PCMOUT_ITS__NSAMPLE__PENDING(ip) \
	(value__AUD_PCMOUT_ITS__NSAMPLE__PENDING(ip) << \
	shift__AUD_PCMOUT_ITS__NSAMPLE(ip))
#define set__AUD_PCMOUT_ITS__NSAMPLE__PENDING(ip) \
	set__AUD_PCMOUT_ITS__NSAMPLE(ip, \
	value__AUD_PCMOUT_ITS__NSAMPLE__PENDING(ip))



/*
 * AUD_PCMOUT_ITS_CLR
 */

#define offset__AUD_PCMOUT_ITS_CLR(ip) 0x0c
#define get__AUD_PCMOUT_ITS_CLR(ip) readl(ip->base + \
	offset__AUD_PCMOUT_ITS_CLR(ip))
#define set__AUD_PCMOUT_ITS_CLR(ip, value) writel(value, ip->base + \
	offset__AUD_PCMOUT_ITS_CLR(ip))

/* UNF */

#define shift__AUD_PCMOUT_ITS_CLR__UNF(ip) 0
#define mask__AUD_PCMOUT_ITS_CLR__UNF(ip) 0x1
#define get__AUD_PCMOUT_ITS_CLR__UNF(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_ITS_CLR(ip)) >> shift__AUD_PCMOUT_ITS_CLR__UNF(ip)) \
	& mask__AUD_PCMOUT_ITS_CLR__UNF(ip))
#define set__AUD_PCMOUT_ITS_CLR__UNF(ip, value) writel((readl(ip->base \
	+ offset__AUD_PCMOUT_ITS_CLR(ip)) & \
	~(mask__AUD_PCMOUT_ITS_CLR__UNF(ip) << \
	shift__AUD_PCMOUT_ITS_CLR__UNF(ip))) | (((value) & \
	mask__AUD_PCMOUT_ITS_CLR__UNF(ip)) << \
	shift__AUD_PCMOUT_ITS_CLR__UNF(ip)), ip->base + \
	offset__AUD_PCMOUT_ITS_CLR(ip))

#define value__AUD_PCMOUT_ITS_CLR__UNF__CLEAR(ip) 0x1
#define mask__AUD_PCMOUT_ITS_CLR__UNF__CLEAR(ip) \
	(value__AUD_PCMOUT_ITS_CLR__UNF__CLEAR(ip) << \
	shift__AUD_PCMOUT_ITS_CLR__UNF(ip))
#define set__AUD_PCMOUT_ITS_CLR__UNF__CLEAR(ip) \
	set__AUD_PCMOUT_ITS_CLR__UNF(ip, \
	value__AUD_PCMOUT_ITS_CLR__UNF__CLEAR(ip))

/* NSAMPLE */

#define shift__AUD_PCMOUT_ITS_CLR__NSAMPLE(ip) 1
#define mask__AUD_PCMOUT_ITS_CLR__NSAMPLE(ip) 0x1
#define get__AUD_PCMOUT_ITS_CLR__NSAMPLE(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_ITS_CLR(ip)) >> \
	shift__AUD_PCMOUT_ITS_CLR__NSAMPLE(ip)) & \
	mask__AUD_PCMOUT_ITS_CLR__NSAMPLE(ip))
#define set__AUD_PCMOUT_ITS_CLR__NSAMPLE(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMOUT_ITS_CLR(ip)) & \
	~(mask__AUD_PCMOUT_ITS_CLR__NSAMPLE(ip) << \
	shift__AUD_PCMOUT_ITS_CLR__NSAMPLE(ip))) | (((value) & \
	mask__AUD_PCMOUT_ITS_CLR__NSAMPLE(ip)) << \
	shift__AUD_PCMOUT_ITS_CLR__NSAMPLE(ip)), ip->base + \
	offset__AUD_PCMOUT_ITS_CLR(ip))

#define value__AUD_PCMOUT_ITS_CLR__NSAMPLE__CLEAR(ip) 0x1
#define mask__AUD_PCMOUT_ITS_CLR__NSAMPLE__CLEAR(ip) \
	(value__AUD_PCMOUT_ITS_CLR__NSAMPLE__CLEAR(ip) << \
	shift__AUD_PCMOUT_ITS_CLR__NSAMPLE(ip))
#define set__AUD_PCMOUT_ITS_CLR__NSAMPLE__CLEAR(ip) \
	set__AUD_PCMOUT_ITS_CLR__NSAMPLE(ip, \
	value__AUD_PCMOUT_ITS_CLR__NSAMPLE__CLEAR(ip))



/*
 * AUD_PCMOUT_IT_EN
 */

#define offset__AUD_PCMOUT_IT_EN(ip) 0x10
#define get__AUD_PCMOUT_IT_EN(ip) readl(ip->base + \
	offset__AUD_PCMOUT_IT_EN(ip))
#define set__AUD_PCMOUT_IT_EN(ip, value) writel(value, ip->base + \
	offset__AUD_PCMOUT_IT_EN(ip))

/* UNF */

#define shift__AUD_PCMOUT_IT_EN__UNF(ip) 0
#define mask__AUD_PCMOUT_IT_EN__UNF(ip) 0x1
#define get__AUD_PCMOUT_IT_EN__UNF(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_IT_EN(ip)) >> shift__AUD_PCMOUT_IT_EN__UNF(ip)) & \
	mask__AUD_PCMOUT_IT_EN__UNF(ip))
#define set__AUD_PCMOUT_IT_EN__UNF(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMOUT_IT_EN(ip)) & ~(mask__AUD_PCMOUT_IT_EN__UNF(ip) << \
	shift__AUD_PCMOUT_IT_EN__UNF(ip))) | (((value) & \
	mask__AUD_PCMOUT_IT_EN__UNF(ip)) << shift__AUD_PCMOUT_IT_EN__UNF(ip)), \
	ip->base + offset__AUD_PCMOUT_IT_EN(ip))

#define value__AUD_PCMOUT_IT_EN__UNF__DISABLED(ip) 0x0
#define mask__AUD_PCMOUT_IT_EN__UNF__DISABLED(ip) \
	(value__AUD_PCMOUT_IT_EN__UNF__DISABLED(ip) << \
	shift__AUD_PCMOUT_IT_EN__UNF(ip))
#define set__AUD_PCMOUT_IT_EN__UNF__DISABLED(ip) \
	set__AUD_PCMOUT_IT_EN__UNF(ip, \
	value__AUD_PCMOUT_IT_EN__UNF__DISABLED(ip))

#define value__AUD_PCMOUT_IT_EN__UNF__ENABLED(ip) 0x1
#define mask__AUD_PCMOUT_IT_EN__UNF__ENABLED(ip) \
	(value__AUD_PCMOUT_IT_EN__UNF__ENABLED(ip) << \
	shift__AUD_PCMOUT_IT_EN__UNF(ip))
#define set__AUD_PCMOUT_IT_EN__UNF__ENABLED(ip) \
	set__AUD_PCMOUT_IT_EN__UNF(ip, \
	value__AUD_PCMOUT_IT_EN__UNF__ENABLED(ip))

/* NSAMPLE */

#define shift__AUD_PCMOUT_IT_EN__NSAMPLE(ip) 1
#define mask__AUD_PCMOUT_IT_EN__NSAMPLE(ip) 0x1
#define get__AUD_PCMOUT_IT_EN__NSAMPLE(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_IT_EN(ip)) >> shift__AUD_PCMOUT_IT_EN__NSAMPLE(ip)) \
	& mask__AUD_PCMOUT_IT_EN__NSAMPLE(ip))
#define set__AUD_PCMOUT_IT_EN__NSAMPLE(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMOUT_IT_EN(ip)) & \
	~(mask__AUD_PCMOUT_IT_EN__NSAMPLE(ip) << \
	shift__AUD_PCMOUT_IT_EN__NSAMPLE(ip))) | (((value) & \
	mask__AUD_PCMOUT_IT_EN__NSAMPLE(ip)) << \
	shift__AUD_PCMOUT_IT_EN__NSAMPLE(ip)), ip->base + \
	offset__AUD_PCMOUT_IT_EN(ip))

#define value__AUD_PCMOUT_IT_EN__NSAMPLE__DISABLED(ip) 0x0
#define mask__AUD_PCMOUT_IT_EN__NSAMPLE__DISABLED(ip) \
	(value__AUD_PCMOUT_IT_EN__NSAMPLE__DISABLED(ip) << \
	shift__AUD_PCMOUT_IT_EN__NSAMPLE(ip))
#define set__AUD_PCMOUT_IT_EN__NSAMPLE__DISABLED(ip) \
	set__AUD_PCMOUT_IT_EN__NSAMPLE(ip, \
	value__AUD_PCMOUT_IT_EN__NSAMPLE__DISABLED(ip))

#define value__AUD_PCMOUT_IT_EN__NSAMPLE__ENABLED(ip) 0x1
#define mask__AUD_PCMOUT_IT_EN__NSAMPLE__ENABLED(ip) \
	(value__AUD_PCMOUT_IT_EN__NSAMPLE__ENABLED(ip) << \
	shift__AUD_PCMOUT_IT_EN__NSAMPLE(ip))
#define set__AUD_PCMOUT_IT_EN__NSAMPLE__ENABLED(ip) \
	set__AUD_PCMOUT_IT_EN__NSAMPLE(ip, \
	value__AUD_PCMOUT_IT_EN__NSAMPLE__ENABLED(ip))



/*
 * AUD_PCMOUT_IT_EN_SET
 */

#define offset__AUD_PCMOUT_IT_EN_SET(ip) 0x14
#define get__AUD_PCMOUT_IT_EN_SET(ip) readl(ip->base + \
	offset__AUD_PCMOUT_IT_EN_SET(ip))
#define set__AUD_PCMOUT_IT_EN_SET(ip, value) writel(value, ip->base + \
	offset__AUD_PCMOUT_IT_EN_SET(ip))

/* UNF */

#define shift__AUD_PCMOUT_IT_EN_SET__UNF(ip) 0
#define mask__AUD_PCMOUT_IT_EN_SET__UNF(ip) 0x1
#define get__AUD_PCMOUT_IT_EN_SET__UNF(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_IT_EN_SET(ip)) >> \
	shift__AUD_PCMOUT_IT_EN_SET__UNF(ip)) & \
	mask__AUD_PCMOUT_IT_EN_SET__UNF(ip))
#define set__AUD_PCMOUT_IT_EN_SET__UNF(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMOUT_IT_EN_SET(ip)) & \
	~(mask__AUD_PCMOUT_IT_EN_SET__UNF(ip) << \
	shift__AUD_PCMOUT_IT_EN_SET__UNF(ip))) | (((value) & \
	mask__AUD_PCMOUT_IT_EN_SET__UNF(ip)) << \
	shift__AUD_PCMOUT_IT_EN_SET__UNF(ip)), ip->base + \
	offset__AUD_PCMOUT_IT_EN_SET(ip))

#define value__AUD_PCMOUT_IT_EN_SET__UNF__SET(ip) 0x1
#define mask__AUD_PCMOUT_IT_EN_SET__UNF__SET(ip) \
	(value__AUD_PCMOUT_IT_EN_SET__UNF__SET(ip) << \
	shift__AUD_PCMOUT_IT_EN_SET__UNF(ip))
#define set__AUD_PCMOUT_IT_EN_SET__UNF__SET(ip) \
	set__AUD_PCMOUT_IT_EN_SET__UNF(ip, \
	value__AUD_PCMOUT_IT_EN_SET__UNF__SET(ip))

/* NSAMPLE */

#define shift__AUD_PCMOUT_IT_EN_SET__NSAMPLE(ip) 1
#define mask__AUD_PCMOUT_IT_EN_SET__NSAMPLE(ip) 0x1
#define get__AUD_PCMOUT_IT_EN_SET__NSAMPLE(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_IT_EN_SET(ip)) >> \
	shift__AUD_PCMOUT_IT_EN_SET__NSAMPLE(ip)) & \
	mask__AUD_PCMOUT_IT_EN_SET__NSAMPLE(ip))
#define set__AUD_PCMOUT_IT_EN_SET__NSAMPLE(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMOUT_IT_EN_SET(ip)) & \
	~(mask__AUD_PCMOUT_IT_EN_SET__NSAMPLE(ip) << \
	shift__AUD_PCMOUT_IT_EN_SET__NSAMPLE(ip))) | (((value) & \
	mask__AUD_PCMOUT_IT_EN_SET__NSAMPLE(ip)) << \
	shift__AUD_PCMOUT_IT_EN_SET__NSAMPLE(ip)), ip->base + \
	offset__AUD_PCMOUT_IT_EN_SET(ip))

#define value__AUD_PCMOUT_IT_EN_SET__NSAMPLE__SET(ip) 0x1
#define mask__AUD_PCMOUT_IT_EN_SET__NSAMPLE__SET(ip) \
	(value__AUD_PCMOUT_IT_EN_SET__NSAMPLE__SET(ip) << \
	shift__AUD_PCMOUT_IT_EN_SET__NSAMPLE(ip))
#define set__AUD_PCMOUT_IT_EN_SET__NSAMPLE__SET(ip) \
	set__AUD_PCMOUT_IT_EN_SET__NSAMPLE(ip, \
	value__AUD_PCMOUT_IT_EN_SET__NSAMPLE__SET(ip))



/*
 * AUD_PCMOUT_IT_EN_CLR
 */

#define offset__AUD_PCMOUT_IT_EN_CLR(ip) 0x18
#define get__AUD_PCMOUT_IT_EN_CLR(ip) readl(ip->base + \
	offset__AUD_PCMOUT_IT_EN_CLR(ip))
#define set__AUD_PCMOUT_IT_EN_CLR(ip, value) writel(value, ip->base + \
	offset__AUD_PCMOUT_IT_EN_CLR(ip))

/* UNF */

#define shift__AUD_PCMOUT_IT_EN_CLR__UNF(ip) 0
#define mask__AUD_PCMOUT_IT_EN_CLR__UNF(ip) 0x1
#define get__AUD_PCMOUT_IT_EN_CLR__UNF(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_IT_EN_CLR(ip)) >> \
	shift__AUD_PCMOUT_IT_EN_CLR__UNF(ip)) & \
	mask__AUD_PCMOUT_IT_EN_CLR__UNF(ip))
#define set__AUD_PCMOUT_IT_EN_CLR__UNF(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMOUT_IT_EN_CLR(ip)) & \
	~(mask__AUD_PCMOUT_IT_EN_CLR__UNF(ip) << \
	shift__AUD_PCMOUT_IT_EN_CLR__UNF(ip))) | (((value) & \
	mask__AUD_PCMOUT_IT_EN_CLR__UNF(ip)) << \
	shift__AUD_PCMOUT_IT_EN_CLR__UNF(ip)), ip->base + \
	offset__AUD_PCMOUT_IT_EN_CLR(ip))

#define value__AUD_PCMOUT_IT_EN_CLR__UNF__CLEAR(ip) 0x1
#define mask__AUD_PCMOUT_IT_EN_CLR__UNF__CLEAR(ip) \
	(value__AUD_PCMOUT_IT_EN_CLR__UNF__CLEAR(ip) << \
	shift__AUD_PCMOUT_IT_EN_CLR__UNF(ip))
#define set__AUD_PCMOUT_IT_EN_CLR__UNF__CLEAR(ip) \
	set__AUD_PCMOUT_IT_EN_CLR__UNF(ip, \
	value__AUD_PCMOUT_IT_EN_CLR__UNF__CLEAR(ip))

/* NSAMPLE */

#define shift__AUD_PCMOUT_IT_EN_CLR__NSAMPLE(ip) 1
#define mask__AUD_PCMOUT_IT_EN_CLR__NSAMPLE(ip) 0x1
#define get__AUD_PCMOUT_IT_EN_CLR__NSAMPLE(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_IT_EN_CLR(ip)) >> \
	shift__AUD_PCMOUT_IT_EN_CLR__NSAMPLE(ip)) & \
	mask__AUD_PCMOUT_IT_EN_CLR__NSAMPLE(ip))
#define set__AUD_PCMOUT_IT_EN_CLR__NSAMPLE(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMOUT_IT_EN_CLR(ip)) & \
	~(mask__AUD_PCMOUT_IT_EN_CLR__NSAMPLE(ip) << \
	shift__AUD_PCMOUT_IT_EN_CLR__NSAMPLE(ip))) | (((value) & \
	mask__AUD_PCMOUT_IT_EN_CLR__NSAMPLE(ip)) << \
	shift__AUD_PCMOUT_IT_EN_CLR__NSAMPLE(ip)), ip->base + \
	offset__AUD_PCMOUT_IT_EN_CLR(ip))

#define value__AUD_PCMOUT_IT_EN_CLR__NSAMPLE__CLEAR(ip) 0x1
#define mask__AUD_PCMOUT_IT_EN_CLR__NSAMPLE__CLEAR(ip) \
	(value__AUD_PCMOUT_IT_EN_CLR__NSAMPLE__CLEAR(ip) << \
	shift__AUD_PCMOUT_IT_EN_CLR__NSAMPLE(ip))
#define set__AUD_PCMOUT_IT_EN_CLR__NSAMPLE__CLEAR(ip) \
	set__AUD_PCMOUT_IT_EN_CLR__NSAMPLE(ip, \
	value__AUD_PCMOUT_IT_EN_CLR__NSAMPLE__CLEAR(ip))



/*
 * AUD_PCMOUT_CTRL
 */

#define offset__AUD_PCMOUT_CTRL(ip) 0x1c
#define get__AUD_PCMOUT_CTRL(ip) readl(ip->base + \
	offset__AUD_PCMOUT_CTRL(ip))
#define set__AUD_PCMOUT_CTRL(ip, value) writel(value, ip->base + \
	offset__AUD_PCMOUT_CTRL(ip))

/* MODE */

#define shift__AUD_PCMOUT_CTRL__MODE(ip) 0
#define mask__AUD_PCMOUT_CTRL__MODE(ip) 0x3
#define get__AUD_PCMOUT_CTRL__MODE(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_CTRL(ip)) >> shift__AUD_PCMOUT_CTRL__MODE(ip)) & \
	mask__AUD_PCMOUT_CTRL__MODE(ip))
#define set__AUD_PCMOUT_CTRL__MODE(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMOUT_CTRL(ip)) & ~(mask__AUD_PCMOUT_CTRL__MODE(ip) << \
	shift__AUD_PCMOUT_CTRL__MODE(ip))) | (((value) & \
	mask__AUD_PCMOUT_CTRL__MODE(ip)) << shift__AUD_PCMOUT_CTRL__MODE(ip)), \
	ip->base + offset__AUD_PCMOUT_CTRL(ip))

#define value__AUD_PCMOUT_CTRL__MODE__OFF(ip) 0x0
#define mask__AUD_PCMOUT_CTRL__MODE__OFF(ip) \
	(value__AUD_PCMOUT_CTRL__MODE__OFF(ip) << \
	shift__AUD_PCMOUT_CTRL__MODE(ip))
#define set__AUD_PCMOUT_CTRL__MODE__OFF(ip) \
	set__AUD_PCMOUT_CTRL__MODE(ip, value__AUD_PCMOUT_CTRL__MODE__OFF(ip))

#define value__AUD_PCMOUT_CTRL__MODE__MUTE(ip) 0x1
#define mask__AUD_PCMOUT_CTRL__MODE__MUTE(ip) \
	(value__AUD_PCMOUT_CTRL__MODE__MUTE(ip) << \
	shift__AUD_PCMOUT_CTRL__MODE(ip))
#define set__AUD_PCMOUT_CTRL__MODE__MUTE(ip) \
	set__AUD_PCMOUT_CTRL__MODE(ip, value__AUD_PCMOUT_CTRL__MODE__MUTE(ip))

#define value__AUD_PCMOUT_CTRL__MODE__PCM(ip) 0x2
#define mask__AUD_PCMOUT_CTRL__MODE__PCM(ip) \
	(value__AUD_PCMOUT_CTRL__MODE__PCM(ip) << \
	shift__AUD_PCMOUT_CTRL__MODE(ip))
#define set__AUD_PCMOUT_CTRL__MODE__PCM(ip) \
	set__AUD_PCMOUT_CTRL__MODE(ip, value__AUD_PCMOUT_CTRL__MODE__PCM(ip))

#define value__AUD_PCMOUT_CTRL__MODE__CD(ip) 0x3
#define mask__AUD_PCMOUT_CTRL__MODE__CD(ip) \
	(value__AUD_PCMOUT_CTRL__MODE__CD(ip) << \
	shift__AUD_PCMOUT_CTRL__MODE(ip))
#define set__AUD_PCMOUT_CTRL__MODE__CD(ip) \
	set__AUD_PCMOUT_CTRL__MODE(ip, value__AUD_PCMOUT_CTRL__MODE__CD(ip))

/* MEM_FMT */

#define shift__AUD_PCMOUT_CTRL__MEM_FMT(ip) 2
#define mask__AUD_PCMOUT_CTRL__MEM_FMT(ip) 0x1
#define get__AUD_PCMOUT_CTRL__MEM_FMT(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_CTRL(ip)) >> shift__AUD_PCMOUT_CTRL__MEM_FMT(ip)) & \
	mask__AUD_PCMOUT_CTRL__MEM_FMT(ip))
#define set__AUD_PCMOUT_CTRL__MEM_FMT(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMOUT_CTRL(ip)) & \
	~(mask__AUD_PCMOUT_CTRL__MEM_FMT(ip) << \
	shift__AUD_PCMOUT_CTRL__MEM_FMT(ip))) | (((value) & \
	mask__AUD_PCMOUT_CTRL__MEM_FMT(ip)) << \
	shift__AUD_PCMOUT_CTRL__MEM_FMT(ip)), ip->base + \
	offset__AUD_PCMOUT_CTRL(ip))

#define value__AUD_PCMOUT_CTRL__MEM_FMT__16_BITS_0_BITS(ip) 0x0
#define mask__AUD_PCMOUT_CTRL__MEM_FMT__16_BITS_0_BITS(ip) \
	(value__AUD_PCMOUT_CTRL__MEM_FMT__16_BITS_0_BITS(ip) << \
	shift__AUD_PCMOUT_CTRL__MEM_FMT(ip))
#define set__AUD_PCMOUT_CTRL__MEM_FMT__16_BITS_0_BITS(ip) \
	set__AUD_PCMOUT_CTRL__MEM_FMT(ip, \
	value__AUD_PCMOUT_CTRL__MEM_FMT__16_BITS_0_BITS(ip))

#define value__AUD_PCMOUT_CTRL__MEM_FMT__16_BITS_16_BITS(ip) 0x1
#define mask__AUD_PCMOUT_CTRL__MEM_FMT__16_BITS_16_BITS(ip) \
	(value__AUD_PCMOUT_CTRL__MEM_FMT__16_BITS_16_BITS(ip) << \
	shift__AUD_PCMOUT_CTRL__MEM_FMT(ip))
#define set__AUD_PCMOUT_CTRL__MEM_FMT__16_BITS_16_BITS(ip) \
	set__AUD_PCMOUT_CTRL__MEM_FMT(ip, \
	value__AUD_PCMOUT_CTRL__MEM_FMT__16_BITS_16_BITS(ip))

/* RND */

#define shift__AUD_PCMOUT_CTRL__RND(ip) 3
#define mask__AUD_PCMOUT_CTRL__RND(ip) 0x1
#define get__AUD_PCMOUT_CTRL__RND(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_CTRL(ip)) >> shift__AUD_PCMOUT_CTRL__RND(ip)) & \
	mask__AUD_PCMOUT_CTRL__RND(ip))
#define set__AUD_PCMOUT_CTRL__RND(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMOUT_CTRL(ip)) & ~(mask__AUD_PCMOUT_CTRL__RND(ip) << \
	shift__AUD_PCMOUT_CTRL__RND(ip))) | (((value) & \
	mask__AUD_PCMOUT_CTRL__RND(ip)) << shift__AUD_PCMOUT_CTRL__RND(ip)), \
	ip->base + offset__AUD_PCMOUT_CTRL(ip))

#define value__AUD_PCMOUT_CTRL__RND__NO_ROUNDING(ip) 0x0
#define mask__AUD_PCMOUT_CTRL__RND__NO_ROUNDING(ip) \
	(value__AUD_PCMOUT_CTRL__RND__NO_ROUNDING(ip) << \
	shift__AUD_PCMOUT_CTRL__RND(ip))
#define set__AUD_PCMOUT_CTRL__RND__NO_ROUNDING(ip) \
	set__AUD_PCMOUT_CTRL__RND(ip, \
	value__AUD_PCMOUT_CTRL__RND__NO_ROUNDING(ip))

#define value__AUD_PCMOUT_CTRL__RND__16_BITS_ROUNDING(ip) 0x1
#define mask__AUD_PCMOUT_CTRL__RND__16_BITS_ROUNDING(ip) \
	(value__AUD_PCMOUT_CTRL__RND__16_BITS_ROUNDING(ip) << \
	shift__AUD_PCMOUT_CTRL__RND(ip))
#define set__AUD_PCMOUT_CTRL__RND__16_BITS_ROUNDING(ip) \
	set__AUD_PCMOUT_CTRL__RND(ip, \
	value__AUD_PCMOUT_CTRL__RND__16_BITS_ROUNDING(ip))

/* CLK_DIV */

#define shift__AUD_PCMOUT_CTRL__CLK_DIV(ip) 4
#define mask__AUD_PCMOUT_CTRL__CLK_DIV(ip) 0xff
#define get__AUD_PCMOUT_CTRL__CLK_DIV(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_CTRL(ip)) >> shift__AUD_PCMOUT_CTRL__CLK_DIV(ip)) & \
	mask__AUD_PCMOUT_CTRL__CLK_DIV(ip))
#define set__AUD_PCMOUT_CTRL__CLK_DIV(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMOUT_CTRL(ip)) & \
	~(mask__AUD_PCMOUT_CTRL__CLK_DIV(ip) << \
	shift__AUD_PCMOUT_CTRL__CLK_DIV(ip))) | (((value) & \
	mask__AUD_PCMOUT_CTRL__CLK_DIV(ip)) << \
	shift__AUD_PCMOUT_CTRL__CLK_DIV(ip)), ip->base + \
	offset__AUD_PCMOUT_CTRL(ip))

/* SPDIF_LAT */

#define shift__AUD_PCMOUT_CTRL__SPDIF_LAT(ip) 12
#define mask__AUD_PCMOUT_CTRL__SPDIF_LAT(ip) 0x1
#define get__AUD_PCMOUT_CTRL__SPDIF_LAT(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_CTRL(ip)) >> shift__AUD_PCMOUT_CTRL__SPDIF_LAT(ip)) \
	& mask__AUD_PCMOUT_CTRL__SPDIF_LAT(ip))
#define set__AUD_PCMOUT_CTRL__SPDIF_LAT(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMOUT_CTRL(ip)) & \
	~(mask__AUD_PCMOUT_CTRL__SPDIF_LAT(ip) << \
	shift__AUD_PCMOUT_CTRL__SPDIF_LAT(ip))) | (((value) & \
	mask__AUD_PCMOUT_CTRL__SPDIF_LAT(ip)) << \
	shift__AUD_PCMOUT_CTRL__SPDIF_LAT(ip)), ip->base + \
	offset__AUD_PCMOUT_CTRL(ip))

#define value__AUD_PCMOUT_CTRL__SPDIF_LAT__IGNORE_SPDIF(ip) 0x0
#define mask__AUD_PCMOUT_CTRL__SPDIF_LAT__IGNORE_SPDIF(ip) \
	(value__AUD_PCMOUT_CTRL__SPDIF_LAT__IGNORE_SPDIF(ip) << \
	shift__AUD_PCMOUT_CTRL__SPDIF_LAT(ip))
#define set__AUD_PCMOUT_CTRL__SPDIF_LAT__IGNORE_SPDIF(ip) \
	set__AUD_PCMOUT_CTRL__SPDIF_LAT(ip, \
	value__AUD_PCMOUT_CTRL__SPDIF_LAT__IGNORE_SPDIF(ip))

#define value__AUD_PCMOUT_CTRL__SPDIF_LAT__WAIT_FOR_SPDIF(ip) 0x1
#define mask__AUD_PCMOUT_CTRL__SPDIF_LAT__WAIT_FOR_SPDIF(ip) \
	(value__AUD_PCMOUT_CTRL__SPDIF_LAT__WAIT_FOR_SPDIF(ip) << \
	shift__AUD_PCMOUT_CTRL__SPDIF_LAT(ip))
#define set__AUD_PCMOUT_CTRL__SPDIF_LAT__WAIT_FOR_SPDIF(ip) \
	set__AUD_PCMOUT_CTRL__SPDIF_LAT(ip, \
	value__AUD_PCMOUT_CTRL__SPDIF_LAT__WAIT_FOR_SPDIF(ip))

/* NSAMPLE */

#define shift__AUD_PCMOUT_CTRL__NSAMPLE(ip) 13
#define mask__AUD_PCMOUT_CTRL__NSAMPLE(ip) 0x7ffff
#define get__AUD_PCMOUT_CTRL__NSAMPLE(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_CTRL(ip)) >> shift__AUD_PCMOUT_CTRL__NSAMPLE(ip)) & \
	mask__AUD_PCMOUT_CTRL__NSAMPLE(ip))
#define set__AUD_PCMOUT_CTRL__NSAMPLE(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMOUT_CTRL(ip)) & \
	~(mask__AUD_PCMOUT_CTRL__NSAMPLE(ip) << \
	shift__AUD_PCMOUT_CTRL__NSAMPLE(ip))) | (((value) & \
	mask__AUD_PCMOUT_CTRL__NSAMPLE(ip)) << \
	shift__AUD_PCMOUT_CTRL__NSAMPLE(ip)), ip->base + \
	offset__AUD_PCMOUT_CTRL(ip))



/*
 * AUD_PCMOUT_STA
 */

#define offset__AUD_PCMOUT_STA(ip) 0x20
#define get__AUD_PCMOUT_STA(ip) readl(ip->base + \
	offset__AUD_PCMOUT_STA(ip))
#define set__AUD_PCMOUT_STA(ip, value) writel(value, ip->base + \
	offset__AUD_PCMOUT_STA(ip))

/* RUN_STOP */

#define shift__AUD_PCMOUT_STA__RUN_STOP(ip) 0
#define mask__AUD_PCMOUT_STA__RUN_STOP(ip) 0x1
#define get__AUD_PCMOUT_STA__RUN_STOP(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_STA(ip)) >> shift__AUD_PCMOUT_STA__RUN_STOP(ip)) & \
	mask__AUD_PCMOUT_STA__RUN_STOP(ip))
#define set__AUD_PCMOUT_STA__RUN_STOP(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMOUT_STA(ip)) & \
	~(mask__AUD_PCMOUT_STA__RUN_STOP(ip) << \
	shift__AUD_PCMOUT_STA__RUN_STOP(ip))) | (((value) & \
	mask__AUD_PCMOUT_STA__RUN_STOP(ip)) << \
	shift__AUD_PCMOUT_STA__RUN_STOP(ip)), ip->base + \
	offset__AUD_PCMOUT_STA(ip))

#define value__AUD_PCMOUT_STA__RUN_STOP__STOPPED(ip) 0x0
#define mask__AUD_PCMOUT_STA__RUN_STOP__STOPPED(ip) \
	(value__AUD_PCMOUT_STA__RUN_STOP__STOPPED(ip) << \
	shift__AUD_PCMOUT_STA__RUN_STOP(ip))
#define set__AUD_PCMOUT_STA__RUN_STOP__STOPPED(ip) \
	set__AUD_PCMOUT_STA__RUN_STOP(ip, \
	value__AUD_PCMOUT_STA__RUN_STOP__STOPPED(ip))

#define value__AUD_PCMOUT_STA__RUN_STOP__RUNNING(ip) 0x1
#define mask__AUD_PCMOUT_STA__RUN_STOP__RUNNING(ip) \
	(value__AUD_PCMOUT_STA__RUN_STOP__RUNNING(ip) << \
	shift__AUD_PCMOUT_STA__RUN_STOP(ip))
#define set__AUD_PCMOUT_STA__RUN_STOP__RUNNING(ip) \
	set__AUD_PCMOUT_STA__RUN_STOP(ip, \
	value__AUD_PCMOUT_STA__RUN_STOP__RUNNING(ip))

/* UNF */

#define shift__AUD_PCMOUT_STA__UNF(ip) 1
#define mask__AUD_PCMOUT_STA__UNF(ip) 0x1
#define get__AUD_PCMOUT_STA__UNF(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_STA(ip)) >> shift__AUD_PCMOUT_STA__UNF(ip)) & \
	mask__AUD_PCMOUT_STA__UNF(ip))
#define set__AUD_PCMOUT_STA__UNF(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMOUT_STA(ip)) & ~(mask__AUD_PCMOUT_STA__UNF(ip) << \
	shift__AUD_PCMOUT_STA__UNF(ip))) | (((value) & \
	mask__AUD_PCMOUT_STA__UNF(ip)) << shift__AUD_PCMOUT_STA__UNF(ip)), \
	ip->base + offset__AUD_PCMOUT_STA(ip))

#define value__AUD_PCMOUT_STA__UNF__DETECTED(ip) 0x1
#define mask__AUD_PCMOUT_STA__UNF__DETECTED(ip) \
	(value__AUD_PCMOUT_STA__UNF__DETECTED(ip) << \
	shift__AUD_PCMOUT_STA__UNF(ip))
#define set__AUD_PCMOUT_STA__UNF__DETECTED(ip) \
	set__AUD_PCMOUT_STA__UNF(ip, value__AUD_PCMOUT_STA__UNF__DETECTED(ip))

/* NSAMPLE */

#define shift__AUD_PCMOUT_STA__NSAMPLE(ip) 2
#define mask__AUD_PCMOUT_STA__NSAMPLE(ip) 0x1
#define get__AUD_PCMOUT_STA__NSAMPLE(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_STA(ip)) >> shift__AUD_PCMOUT_STA__NSAMPLE(ip)) & \
	mask__AUD_PCMOUT_STA__NSAMPLE(ip))
#define set__AUD_PCMOUT_STA__NSAMPLE(ip, value) writel((readl(ip->base \
	+ offset__AUD_PCMOUT_STA(ip)) & ~(mask__AUD_PCMOUT_STA__NSAMPLE(ip) << \
	shift__AUD_PCMOUT_STA__NSAMPLE(ip))) | (((value) & \
	mask__AUD_PCMOUT_STA__NSAMPLE(ip)) << \
	shift__AUD_PCMOUT_STA__NSAMPLE(ip)), ip->base + \
	offset__AUD_PCMOUT_STA(ip))

#define value__AUD_PCMOUT_STA__NSAMPLE__DONE(ip) 0x1
#define mask__AUD_PCMOUT_STA__NSAMPLE__DONE(ip) \
	(value__AUD_PCMOUT_STA__NSAMPLE__DONE(ip) << \
	shift__AUD_PCMOUT_STA__NSAMPLE(ip))
#define set__AUD_PCMOUT_STA__NSAMPLE__DONE(ip) \
	set__AUD_PCMOUT_STA__NSAMPLE(ip, \
	value__AUD_PCMOUT_STA__NSAMPLE__DONE(ip))



/*
 * AUD_PCMOUT_FMT
 */

#define offset__AUD_PCMOUT_FMT(ip) 0x24
#define get__AUD_PCMOUT_FMT(ip) readl(ip->base + \
	offset__AUD_PCMOUT_FMT(ip))
#define set__AUD_PCMOUT_FMT(ip, value) writel(value, ip->base + \
	offset__AUD_PCMOUT_FMT(ip))

/* NBIT */

#define shift__AUD_PCMOUT_FMT__NBIT(ip) 0
#define mask__AUD_PCMOUT_FMT__NBIT(ip) 0x1
#define get__AUD_PCMOUT_FMT__NBIT(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_FMT(ip)) >> shift__AUD_PCMOUT_FMT__NBIT(ip)) & \
	mask__AUD_PCMOUT_FMT__NBIT(ip))
#define set__AUD_PCMOUT_FMT__NBIT(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMOUT_FMT(ip)) & ~(mask__AUD_PCMOUT_FMT__NBIT(ip) << \
	shift__AUD_PCMOUT_FMT__NBIT(ip))) | (((value) & \
	mask__AUD_PCMOUT_FMT__NBIT(ip)) << shift__AUD_PCMOUT_FMT__NBIT(ip)), \
	ip->base + offset__AUD_PCMOUT_FMT(ip))

#define value__AUD_PCMOUT_FMT__NBIT__32_BITS(ip) 0x0
#define mask__AUD_PCMOUT_FMT__NBIT__32_BITS(ip) \
	(value__AUD_PCMOUT_FMT__NBIT__32_BITS(ip) << \
	shift__AUD_PCMOUT_FMT__NBIT(ip))
#define set__AUD_PCMOUT_FMT__NBIT__32_BITS(ip) \
	set__AUD_PCMOUT_FMT__NBIT(ip, \
	value__AUD_PCMOUT_FMT__NBIT__32_BITS(ip))

#define value__AUD_PCMOUT_FMT__NBIT__16_BITS(ip) 0x1
#define mask__AUD_PCMOUT_FMT__NBIT__16_BITS(ip) \
	(value__AUD_PCMOUT_FMT__NBIT__16_BITS(ip) << \
	shift__AUD_PCMOUT_FMT__NBIT(ip))
#define set__AUD_PCMOUT_FMT__NBIT__16_BITS(ip) \
	set__AUD_PCMOUT_FMT__NBIT(ip, \
	value__AUD_PCMOUT_FMT__NBIT__16_BITS(ip))

/* DATA_SIZE */

#define shift__AUD_PCMOUT_FMT__DATA_SIZE(ip) 1
#define mask__AUD_PCMOUT_FMT__DATA_SIZE(ip) (ip->ver < \
	6 ? 0x3 : 0x7)
#define get__AUD_PCMOUT_FMT__DATA_SIZE(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_FMT(ip)) >> shift__AUD_PCMOUT_FMT__DATA_SIZE(ip)) & \
	mask__AUD_PCMOUT_FMT__DATA_SIZE(ip))
#define set__AUD_PCMOUT_FMT__DATA_SIZE(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMOUT_FMT(ip)) & \
	~(mask__AUD_PCMOUT_FMT__DATA_SIZE(ip) << \
	shift__AUD_PCMOUT_FMT__DATA_SIZE(ip))) | (((value) & \
	mask__AUD_PCMOUT_FMT__DATA_SIZE(ip)) << \
	shift__AUD_PCMOUT_FMT__DATA_SIZE(ip)), ip->base + \
	offset__AUD_PCMOUT_FMT(ip))

#define value__AUD_PCMOUT_FMT__DATA_SIZE__16_BITS(ip) (ip->ver < \
	6 ? 0x3 : 0x0)
#define mask__AUD_PCMOUT_FMT__DATA_SIZE__16_BITS(ip) \
	(value__AUD_PCMOUT_FMT__DATA_SIZE__16_BITS(ip) << \
	shift__AUD_PCMOUT_FMT__DATA_SIZE(ip))
#define set__AUD_PCMOUT_FMT__DATA_SIZE__16_BITS(ip) \
	set__AUD_PCMOUT_FMT__DATA_SIZE(ip, \
	value__AUD_PCMOUT_FMT__DATA_SIZE__16_BITS(ip))

#define value__AUD_PCMOUT_FMT__DATA_SIZE__18_BITS(ip) (ip->ver < \
	6 ? 0x2 : 0x1)
#define mask__AUD_PCMOUT_FMT__DATA_SIZE__18_BITS(ip) \
	(value__AUD_PCMOUT_FMT__DATA_SIZE__18_BITS(ip) << \
	shift__AUD_PCMOUT_FMT__DATA_SIZE(ip))
#define set__AUD_PCMOUT_FMT__DATA_SIZE__18_BITS(ip) \
	set__AUD_PCMOUT_FMT__DATA_SIZE(ip, \
	value__AUD_PCMOUT_FMT__DATA_SIZE__18_BITS(ip))

#define value__AUD_PCMOUT_FMT__DATA_SIZE__20_BITS(ip) (ip->ver < \
	6 ? 0x1 : 0x2)
#define mask__AUD_PCMOUT_FMT__DATA_SIZE__20_BITS(ip) \
	(value__AUD_PCMOUT_FMT__DATA_SIZE__20_BITS(ip) << \
	shift__AUD_PCMOUT_FMT__DATA_SIZE(ip))
#define set__AUD_PCMOUT_FMT__DATA_SIZE__20_BITS(ip) \
	set__AUD_PCMOUT_FMT__DATA_SIZE(ip, \
	value__AUD_PCMOUT_FMT__DATA_SIZE__20_BITS(ip))

#define value__AUD_PCMOUT_FMT__DATA_SIZE__24_BITS(ip) (ip->ver < \
	6 ? 0x0 : 0x3)
#define mask__AUD_PCMOUT_FMT__DATA_SIZE__24_BITS(ip) \
	(value__AUD_PCMOUT_FMT__DATA_SIZE__24_BITS(ip) << \
	shift__AUD_PCMOUT_FMT__DATA_SIZE(ip))
#define set__AUD_PCMOUT_FMT__DATA_SIZE__24_BITS(ip) \
	set__AUD_PCMOUT_FMT__DATA_SIZE(ip, \
	value__AUD_PCMOUT_FMT__DATA_SIZE__24_BITS(ip))

#define value__AUD_PCMOUT_FMT__DATA_SIZE__28_BITS(ip) (ip->ver < \
	6 ? -1 : 0x4)
#define mask__AUD_PCMOUT_FMT__DATA_SIZE__28_BITS(ip) \
	(value__AUD_PCMOUT_FMT__DATA_SIZE__28_BITS(ip) << \
	shift__AUD_PCMOUT_FMT__DATA_SIZE(ip))
#define set__AUD_PCMOUT_FMT__DATA_SIZE__28_BITS(ip) \
	set__AUD_PCMOUT_FMT__DATA_SIZE(ip, \
	value__AUD_PCMOUT_FMT__DATA_SIZE__28_BITS(ip))

#define value__AUD_PCMOUT_FMT__DATA_SIZE__32_BITS(ip) (ip->ver < \
	6 ? -1 : 0x5)
#define mask__AUD_PCMOUT_FMT__DATA_SIZE__32_BITS(ip) \
	(value__AUD_PCMOUT_FMT__DATA_SIZE__32_BITS(ip) << \
	shift__AUD_PCMOUT_FMT__DATA_SIZE(ip))
#define set__AUD_PCMOUT_FMT__DATA_SIZE__32_BITS(ip) \
	set__AUD_PCMOUT_FMT__DATA_SIZE(ip, \
	value__AUD_PCMOUT_FMT__DATA_SIZE__32_BITS(ip))

/* LR_POL */

#define shift__AUD_PCMOUT_FMT__LR_POL(ip) (ip->ver < \
	6 ? 3 : 4)
#define mask__AUD_PCMOUT_FMT__LR_POL(ip) 0x1
#define get__AUD_PCMOUT_FMT__LR_POL(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_FMT(ip)) >> shift__AUD_PCMOUT_FMT__LR_POL(ip)) & \
	mask__AUD_PCMOUT_FMT__LR_POL(ip))
#define set__AUD_PCMOUT_FMT__LR_POL(ip, value) writel((readl(ip->base \
	+ offset__AUD_PCMOUT_FMT(ip)) & ~(mask__AUD_PCMOUT_FMT__LR_POL(ip) << \
	shift__AUD_PCMOUT_FMT__LR_POL(ip))) | (((value) & \
	mask__AUD_PCMOUT_FMT__LR_POL(ip)) << \
	shift__AUD_PCMOUT_FMT__LR_POL(ip)), ip->base + \
	offset__AUD_PCMOUT_FMT(ip))

#define value__AUD_PCMOUT_FMT__LR_POL__LEFT_LOW(ip) 0x0
#define mask__AUD_PCMOUT_FMT__LR_POL__LEFT_LOW(ip) \
	(value__AUD_PCMOUT_FMT__LR_POL__LEFT_LOW(ip) << \
	shift__AUD_PCMOUT_FMT__LR_POL(ip))
#define set__AUD_PCMOUT_FMT__LR_POL__LEFT_LOW(ip) \
	set__AUD_PCMOUT_FMT__LR_POL(ip, \
	value__AUD_PCMOUT_FMT__LR_POL__LEFT_LOW(ip))

#define value__AUD_PCMOUT_FMT__LR_POL__LEFT_HIGH(ip) 0x1
#define mask__AUD_PCMOUT_FMT__LR_POL__LEFT_HIGH(ip) \
	(value__AUD_PCMOUT_FMT__LR_POL__LEFT_HIGH(ip) << \
	shift__AUD_PCMOUT_FMT__LR_POL(ip))
#define set__AUD_PCMOUT_FMT__LR_POL__LEFT_HIGH(ip) \
	set__AUD_PCMOUT_FMT__LR_POL(ip, \
	value__AUD_PCMOUT_FMT__LR_POL__LEFT_HIGH(ip))

/* SCLK_EDGE */

#define shift__AUD_PCMOUT_FMT__SCLK_EDGE(ip) (ip->ver < \
	6 ? 4 : 5)
#define mask__AUD_PCMOUT_FMT__SCLK_EDGE(ip) 0x1
#define get__AUD_PCMOUT_FMT__SCLK_EDGE(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_FMT(ip)) >> shift__AUD_PCMOUT_FMT__SCLK_EDGE(ip)) & \
	mask__AUD_PCMOUT_FMT__SCLK_EDGE(ip))
#define set__AUD_PCMOUT_FMT__SCLK_EDGE(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMOUT_FMT(ip)) & \
	~(mask__AUD_PCMOUT_FMT__SCLK_EDGE(ip) << \
	shift__AUD_PCMOUT_FMT__SCLK_EDGE(ip))) | (((value) & \
	mask__AUD_PCMOUT_FMT__SCLK_EDGE(ip)) << \
	shift__AUD_PCMOUT_FMT__SCLK_EDGE(ip)), ip->base + \
	offset__AUD_PCMOUT_FMT(ip))

#define value__AUD_PCMOUT_FMT__SCLK_EDGE__RISING(ip) (ip->ver < \
	4 ? 0x1 : 0x0)
#define mask__AUD_PCMOUT_FMT__SCLK_EDGE__RISING(ip) \
	(value__AUD_PCMOUT_FMT__SCLK_EDGE__RISING(ip) << \
	shift__AUD_PCMOUT_FMT__SCLK_EDGE(ip))
#define set__AUD_PCMOUT_FMT__SCLK_EDGE__RISING(ip) \
	set__AUD_PCMOUT_FMT__SCLK_EDGE(ip, \
	value__AUD_PCMOUT_FMT__SCLK_EDGE__RISING(ip))

#define value__AUD_PCMOUT_FMT__SCLK_EDGE__FALLING(ip) (ip->ver < \
	4 ? 0x0 : 0x1)
#define mask__AUD_PCMOUT_FMT__SCLK_EDGE__FALLING(ip) \
	(value__AUD_PCMOUT_FMT__SCLK_EDGE__FALLING(ip) << \
	shift__AUD_PCMOUT_FMT__SCLK_EDGE(ip))
#define set__AUD_PCMOUT_FMT__SCLK_EDGE__FALLING(ip) \
	set__AUD_PCMOUT_FMT__SCLK_EDGE(ip, \
	value__AUD_PCMOUT_FMT__SCLK_EDGE__FALLING(ip))

/* PADDING */

#define shift__AUD_PCMOUT_FMT__PADDING(ip) (ip->ver < \
	6 ? 5 : 6)
#define mask__AUD_PCMOUT_FMT__PADDING(ip) 0x1
#define get__AUD_PCMOUT_FMT__PADDING(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_FMT(ip)) >> shift__AUD_PCMOUT_FMT__PADDING(ip)) & \
	mask__AUD_PCMOUT_FMT__PADDING(ip))
#define set__AUD_PCMOUT_FMT__PADDING(ip, value) writel((readl(ip->base \
	+ offset__AUD_PCMOUT_FMT(ip)) & ~(mask__AUD_PCMOUT_FMT__PADDING(ip) << \
	shift__AUD_PCMOUT_FMT__PADDING(ip))) | (((value) & \
	mask__AUD_PCMOUT_FMT__PADDING(ip)) << \
	shift__AUD_PCMOUT_FMT__PADDING(ip)), ip->base + \
	offset__AUD_PCMOUT_FMT(ip))

#define value__AUD_PCMOUT_FMT__PADDING__1_CYCLE_DELAY(ip) 0x0
#define mask__AUD_PCMOUT_FMT__PADDING__1_CYCLE_DELAY(ip) \
	(value__AUD_PCMOUT_FMT__PADDING__1_CYCLE_DELAY(ip) << \
	shift__AUD_PCMOUT_FMT__PADDING(ip))
#define set__AUD_PCMOUT_FMT__PADDING__1_CYCLE_DELAY(ip) \
	set__AUD_PCMOUT_FMT__PADDING(ip, \
	value__AUD_PCMOUT_FMT__PADDING__1_CYCLE_DELAY(ip))

#define value__AUD_PCMOUT_FMT__PADDING__NO_DELAY(ip) 0x1
#define mask__AUD_PCMOUT_FMT__PADDING__NO_DELAY(ip) \
	(value__AUD_PCMOUT_FMT__PADDING__NO_DELAY(ip) << \
	shift__AUD_PCMOUT_FMT__PADDING(ip))
#define set__AUD_PCMOUT_FMT__PADDING__NO_DELAY(ip) \
	set__AUD_PCMOUT_FMT__PADDING(ip, \
	value__AUD_PCMOUT_FMT__PADDING__NO_DELAY(ip))

/* ALIGN */

#define shift__AUD_PCMOUT_FMT__ALIGN(ip) (ip->ver < \
	6 ? 6 : 7)
#define mask__AUD_PCMOUT_FMT__ALIGN(ip) 0x1
#define get__AUD_PCMOUT_FMT__ALIGN(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_FMT(ip)) >> shift__AUD_PCMOUT_FMT__ALIGN(ip)) & \
	mask__AUD_PCMOUT_FMT__ALIGN(ip))
#define set__AUD_PCMOUT_FMT__ALIGN(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMOUT_FMT(ip)) & ~(mask__AUD_PCMOUT_FMT__ALIGN(ip) << \
	shift__AUD_PCMOUT_FMT__ALIGN(ip))) | (((value) & \
	mask__AUD_PCMOUT_FMT__ALIGN(ip)) << shift__AUD_PCMOUT_FMT__ALIGN(ip)), \
	ip->base + offset__AUD_PCMOUT_FMT(ip))

#define value__AUD_PCMOUT_FMT__ALIGN__LEFT(ip) 0x0
#define mask__AUD_PCMOUT_FMT__ALIGN__LEFT(ip) \
	(value__AUD_PCMOUT_FMT__ALIGN__LEFT(ip) << \
	shift__AUD_PCMOUT_FMT__ALIGN(ip))
#define set__AUD_PCMOUT_FMT__ALIGN__LEFT(ip) \
	set__AUD_PCMOUT_FMT__ALIGN(ip, value__AUD_PCMOUT_FMT__ALIGN__LEFT(ip))

#define value__AUD_PCMOUT_FMT__ALIGN__RIGHT(ip) 0x1
#define mask__AUD_PCMOUT_FMT__ALIGN__RIGHT(ip) \
	(value__AUD_PCMOUT_FMT__ALIGN__RIGHT(ip) << \
	shift__AUD_PCMOUT_FMT__ALIGN(ip))
#define set__AUD_PCMOUT_FMT__ALIGN__RIGHT(ip) \
	set__AUD_PCMOUT_FMT__ALIGN(ip, \
	value__AUD_PCMOUT_FMT__ALIGN__RIGHT(ip))

/* ORDER */

#define shift__AUD_PCMOUT_FMT__ORDER(ip) (ip->ver < \
	6 ? 7 : 8)
#define mask__AUD_PCMOUT_FMT__ORDER(ip) 0x1
#define get__AUD_PCMOUT_FMT__ORDER(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_FMT(ip)) >> shift__AUD_PCMOUT_FMT__ORDER(ip)) & \
	mask__AUD_PCMOUT_FMT__ORDER(ip))
#define set__AUD_PCMOUT_FMT__ORDER(ip, value) writel((readl(ip->base + \
	offset__AUD_PCMOUT_FMT(ip)) & ~(mask__AUD_PCMOUT_FMT__ORDER(ip) << \
	shift__AUD_PCMOUT_FMT__ORDER(ip))) | (((value) & \
	mask__AUD_PCMOUT_FMT__ORDER(ip)) << shift__AUD_PCMOUT_FMT__ORDER(ip)), \
	ip->base + offset__AUD_PCMOUT_FMT(ip))

#define value__AUD_PCMOUT_FMT__ORDER__LSB_FIRST(ip) 0x0
#define mask__AUD_PCMOUT_FMT__ORDER__LSB_FIRST(ip) \
	(value__AUD_PCMOUT_FMT__ORDER__LSB_FIRST(ip) << \
	shift__AUD_PCMOUT_FMT__ORDER(ip))
#define set__AUD_PCMOUT_FMT__ORDER__LSB_FIRST(ip) \
	set__AUD_PCMOUT_FMT__ORDER(ip, \
	value__AUD_PCMOUT_FMT__ORDER__LSB_FIRST(ip))

#define value__AUD_PCMOUT_FMT__ORDER__MSB_FIRST(ip) 0x1
#define mask__AUD_PCMOUT_FMT__ORDER__MSB_FIRST(ip) \
	(value__AUD_PCMOUT_FMT__ORDER__MSB_FIRST(ip) << \
	shift__AUD_PCMOUT_FMT__ORDER(ip))
#define set__AUD_PCMOUT_FMT__ORDER__MSB_FIRST(ip) \
	set__AUD_PCMOUT_FMT__ORDER(ip, \
	value__AUD_PCMOUT_FMT__ORDER__MSB_FIRST(ip))

/* NUM_CH */

#define shift__AUD_PCMOUT_FMT__NUM_CH(ip) (ip->ver < \
	6 ? 8 : 9)
#define mask__AUD_PCMOUT_FMT__NUM_CH(ip) 0x7
#define get__AUD_PCMOUT_FMT__NUM_CH(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_FMT(ip)) >> shift__AUD_PCMOUT_FMT__NUM_CH(ip)) & \
	mask__AUD_PCMOUT_FMT__NUM_CH(ip))
#define set__AUD_PCMOUT_FMT__NUM_CH(ip, value) writel((readl(ip->base \
	+ offset__AUD_PCMOUT_FMT(ip)) & ~(mask__AUD_PCMOUT_FMT__NUM_CH(ip) << \
	shift__AUD_PCMOUT_FMT__NUM_CH(ip))) | (((value) & \
	mask__AUD_PCMOUT_FMT__NUM_CH(ip)) << \
	shift__AUD_PCMOUT_FMT__NUM_CH(ip)), ip->base + \
	offset__AUD_PCMOUT_FMT(ip))

#define value__AUD_PCMOUT_FMT__NUM_CH__1_CHANNEL(ip) (ip->ver < \
	2 ? -1 : 0x1)
#define mask__AUD_PCMOUT_FMT__NUM_CH__1_CHANNEL(ip) \
	(value__AUD_PCMOUT_FMT__NUM_CH__1_CHANNEL(ip) << \
	shift__AUD_PCMOUT_FMT__NUM_CH(ip))
#define set__AUD_PCMOUT_FMT__NUM_CH__1_CHANNEL(ip) \
	set__AUD_PCMOUT_FMT__NUM_CH(ip, \
	value__AUD_PCMOUT_FMT__NUM_CH__1_CHANNEL(ip))

#define value__AUD_PCMOUT_FMT__NUM_CH__2_CHANNELS(ip) (ip->ver < \
	2 ? -1 : 0x2)
#define mask__AUD_PCMOUT_FMT__NUM_CH__2_CHANNELS(ip) \
	(value__AUD_PCMOUT_FMT__NUM_CH__2_CHANNELS(ip) << \
	shift__AUD_PCMOUT_FMT__NUM_CH(ip))
#define set__AUD_PCMOUT_FMT__NUM_CH__2_CHANNELS(ip) \
	set__AUD_PCMOUT_FMT__NUM_CH(ip, \
	value__AUD_PCMOUT_FMT__NUM_CH__2_CHANNELS(ip))

#define value__AUD_PCMOUT_FMT__NUM_CH__3_CHANNELS(ip) (ip->ver < \
	2 ? -1 : 0x3)
#define mask__AUD_PCMOUT_FMT__NUM_CH__3_CHANNELS(ip) \
	(value__AUD_PCMOUT_FMT__NUM_CH__3_CHANNELS(ip) << \
	shift__AUD_PCMOUT_FMT__NUM_CH(ip))
#define set__AUD_PCMOUT_FMT__NUM_CH__3_CHANNELS(ip) \
	set__AUD_PCMOUT_FMT__NUM_CH(ip, \
	value__AUD_PCMOUT_FMT__NUM_CH__3_CHANNELS(ip))

#define value__AUD_PCMOUT_FMT__NUM_CH__4_CHANNELS(ip) (ip->ver < \
	2 ? -1 : 0x4)
#define mask__AUD_PCMOUT_FMT__NUM_CH__4_CHANNELS(ip) \
	(value__AUD_PCMOUT_FMT__NUM_CH__4_CHANNELS(ip) << \
	shift__AUD_PCMOUT_FMT__NUM_CH(ip))
#define set__AUD_PCMOUT_FMT__NUM_CH__4_CHANNELS(ip) \
	set__AUD_PCMOUT_FMT__NUM_CH(ip, \
	value__AUD_PCMOUT_FMT__NUM_CH__4_CHANNELS(ip))

#define value__AUD_PCMOUT_FMT__NUM_CH__5_CHANNELS(ip) (ip->ver < \
	2 ? 0x5 : 0x5)
#define mask__AUD_PCMOUT_FMT__NUM_CH__5_CHANNELS(ip) \
	(value__AUD_PCMOUT_FMT__NUM_CH__5_CHANNELS(ip) << \
	shift__AUD_PCMOUT_FMT__NUM_CH(ip))
#define set__AUD_PCMOUT_FMT__NUM_CH__5_CHANNELS(ip) \
	set__AUD_PCMOUT_FMT__NUM_CH(ip, \
	value__AUD_PCMOUT_FMT__NUM_CH__5_CHANNELS(ip))

/* BACK_STALLING */

#define shift__AUD_PCMOUT_FMT__BACK_STALLING(ip) (ip->ver < \
	6 ? -1 : 12)
#define mask__AUD_PCMOUT_FMT__BACK_STALLING(ip) (ip->ver < \
	6 ? -1 : 0x1)
#define get__AUD_PCMOUT_FMT__BACK_STALLING(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_FMT(ip)) >> \
	shift__AUD_PCMOUT_FMT__BACK_STALLING(ip)) & \
	mask__AUD_PCMOUT_FMT__BACK_STALLING(ip))
#define set__AUD_PCMOUT_FMT__BACK_STALLING(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMOUT_FMT(ip)) & \
	~(mask__AUD_PCMOUT_FMT__BACK_STALLING(ip) << \
	shift__AUD_PCMOUT_FMT__BACK_STALLING(ip))) | (((value) & \
	mask__AUD_PCMOUT_FMT__BACK_STALLING(ip)) << \
	shift__AUD_PCMOUT_FMT__BACK_STALLING(ip)), ip->base + \
	offset__AUD_PCMOUT_FMT(ip))

#define value__AUD_PCMOUT_FMT__BACK_STALLING__DISABLED(ip) (ip->ver < \
	6 ? -1 : 0x0)
#define mask__AUD_PCMOUT_FMT__BACK_STALLING__DISABLED(ip) \
	(value__AUD_PCMOUT_FMT__BACK_STALLING__DISABLED(ip) << \
	shift__AUD_PCMOUT_FMT__BACK_STALLING(ip))
#define set__AUD_PCMOUT_FMT__BACK_STALLING__DISABLED(ip) \
	set__AUD_PCMOUT_FMT__BACK_STALLING(ip, \
	value__AUD_PCMOUT_FMT__BACK_STALLING__DISABLED(ip))

#define value__AUD_PCMOUT_FMT__BACK_STALLING__ENABLED(ip) (ip->ver < \
	6 ? -1 : 0x1)
#define mask__AUD_PCMOUT_FMT__BACK_STALLING__ENABLED(ip) \
	(value__AUD_PCMOUT_FMT__BACK_STALLING__ENABLED(ip) << \
	shift__AUD_PCMOUT_FMT__BACK_STALLING(ip))
#define set__AUD_PCMOUT_FMT__BACK_STALLING__ENABLED(ip) \
	set__AUD_PCMOUT_FMT__BACK_STALLING(ip, \
	value__AUD_PCMOUT_FMT__BACK_STALLING__ENABLED(ip))

/* DMA_REQ_TRIG_LMT */

#define shift__AUD_PCMOUT_FMT__DMA_REQ_TRIG_LMT(ip) (ip->ver < \
	6 ? 11 : 13)
#define mask__AUD_PCMOUT_FMT__DMA_REQ_TRIG_LMT(ip) (ip->ver < \
	6 ? 0x1f : 0x7f)
#define get__AUD_PCMOUT_FMT__DMA_REQ_TRIG_LMT(ip) ((readl(ip->base + \
	offset__AUD_PCMOUT_FMT(ip)) >> \
	shift__AUD_PCMOUT_FMT__DMA_REQ_TRIG_LMT(ip)) & \
	mask__AUD_PCMOUT_FMT__DMA_REQ_TRIG_LMT(ip))
#define set__AUD_PCMOUT_FMT__DMA_REQ_TRIG_LMT(ip, value) \
	writel((readl(ip->base + offset__AUD_PCMOUT_FMT(ip)) & \
	~(mask__AUD_PCMOUT_FMT__DMA_REQ_TRIG_LMT(ip) << \
	shift__AUD_PCMOUT_FMT__DMA_REQ_TRIG_LMT(ip))) | (((value) & \
	mask__AUD_PCMOUT_FMT__DMA_REQ_TRIG_LMT(ip)) << \
	shift__AUD_PCMOUT_FMT__DMA_REQ_TRIG_LMT(ip)), ip->base + \
	offset__AUD_PCMOUT_FMT(ip))



#endif
