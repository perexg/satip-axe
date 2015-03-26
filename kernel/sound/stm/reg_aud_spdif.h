#ifndef __SND_STM_AUD_SPDIF_H
#define __SND_STM_AUD_SPDIF_H

/*
 * AUD_SPDIF_RST
 */

#define offset__AUD_SPDIF_RST(ip) 0x00
#define get__AUD_SPDIF_RST(ip) readl(ip->base + \
	offset__AUD_SPDIF_RST(ip))
#define set__AUD_SPDIF_RST(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_RST(ip))

/* SRSTP */

#define shift__AUD_SPDIF_RST__SRSTP(ip) 0
#define mask__AUD_SPDIF_RST__SRSTP(ip) 0x1
#define get__AUD_SPDIF_RST__SRSTP(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_RST(ip)) >> shift__AUD_SPDIF_RST__SRSTP(ip)) & \
	mask__AUD_SPDIF_RST__SRSTP(ip))
#define set__AUD_SPDIF_RST__SRSTP(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_RST(ip)) & ~(mask__AUD_SPDIF_RST__SRSTP(ip) << \
	shift__AUD_SPDIF_RST__SRSTP(ip))) | (((value) & \
	mask__AUD_SPDIF_RST__SRSTP(ip)) << shift__AUD_SPDIF_RST__SRSTP(ip)), \
	ip->base + offset__AUD_SPDIF_RST(ip))

#define value__AUD_SPDIF_RST__SRSTP__RUNNING(ip) 0x0
#define mask__AUD_SPDIF_RST__SRSTP__RUNNING(ip) \
	(value__AUD_SPDIF_RST__SRSTP__RUNNING(ip) << \
	shift__AUD_SPDIF_RST__SRSTP(ip))
#define set__AUD_SPDIF_RST__SRSTP__RUNNING(ip) \
	set__AUD_SPDIF_RST__SRSTP(ip, \
	value__AUD_SPDIF_RST__SRSTP__RUNNING(ip))

#define value__AUD_SPDIF_RST__SRSTP__RESET(ip) 0x1
#define mask__AUD_SPDIF_RST__SRSTP__RESET(ip) \
	(value__AUD_SPDIF_RST__SRSTP__RESET(ip) << \
	shift__AUD_SPDIF_RST__SRSTP(ip))
#define set__AUD_SPDIF_RST__SRSTP__RESET(ip) \
	set__AUD_SPDIF_RST__SRSTP(ip, value__AUD_SPDIF_RST__SRSTP__RESET(ip))



/*
 * AUD_SPDIF_DATA
 */

#define offset__AUD_SPDIF_DATA(ip) 0x04
#define get__AUD_SPDIF_DATA(ip) readl(ip->base + \
	offset__AUD_SPDIF_DATA(ip))
#define set__AUD_SPDIF_DATA(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_DATA(ip))

/* DATA */

#define shift__AUD_SPDIF_DATA__DATA(ip) 0
#define mask__AUD_SPDIF_DATA__DATA(ip) 0xffffffff
#define get__AUD_SPDIF_DATA__DATA(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_DATA(ip)) >> shift__AUD_SPDIF_DATA__DATA(ip)) & \
	mask__AUD_SPDIF_DATA__DATA(ip))
#define set__AUD_SPDIF_DATA__DATA(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_DATA(ip)) & ~(mask__AUD_SPDIF_DATA__DATA(ip) << \
	shift__AUD_SPDIF_DATA__DATA(ip))) | (((value) & \
	mask__AUD_SPDIF_DATA__DATA(ip)) << shift__AUD_SPDIF_DATA__DATA(ip)), \
	ip->base + offset__AUD_SPDIF_DATA(ip))



/*
 * AUD_SPDIF_ITS
 */

#define offset__AUD_SPDIF_ITS(ip) 0x08
#define get__AUD_SPDIF_ITS(ip) readl(ip->base + \
	offset__AUD_SPDIF_ITS(ip))
#define set__AUD_SPDIF_ITS(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_ITS(ip))

/* UNF */

#define shift__AUD_SPDIF_ITS__UNF(ip) 0
#define mask__AUD_SPDIF_ITS__UNF(ip) 0x1
#define get__AUD_SPDIF_ITS__UNF(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_ITS(ip)) >> shift__AUD_SPDIF_ITS__UNF(ip)) & \
	mask__AUD_SPDIF_ITS__UNF(ip))
#define set__AUD_SPDIF_ITS__UNF(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_ITS(ip)) & ~(mask__AUD_SPDIF_ITS__UNF(ip) << \
	shift__AUD_SPDIF_ITS__UNF(ip))) | (((value) & \
	mask__AUD_SPDIF_ITS__UNF(ip)) << shift__AUD_SPDIF_ITS__UNF(ip)), \
	ip->base + offset__AUD_SPDIF_ITS(ip))

#define value__AUD_SPDIF_ITS__UNF__PENDING(ip) 0x1
#define mask__AUD_SPDIF_ITS__UNF__PENDING(ip) \
	(value__AUD_SPDIF_ITS__UNF__PENDING(ip) << \
	shift__AUD_SPDIF_ITS__UNF(ip))
#define set__AUD_SPDIF_ITS__UNF__PENDING(ip) \
	set__AUD_SPDIF_ITS__UNF(ip, value__AUD_SPDIF_ITS__UNF__PENDING(ip))

/* EOBURST */

#define shift__AUD_SPDIF_ITS__EOBURST(ip) 1
#define mask__AUD_SPDIF_ITS__EOBURST(ip) 0x1
#define get__AUD_SPDIF_ITS__EOBURST(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_ITS(ip)) >> shift__AUD_SPDIF_ITS__EOBURST(ip)) & \
	mask__AUD_SPDIF_ITS__EOBURST(ip))
#define set__AUD_SPDIF_ITS__EOBURST(ip, value) writel((readl(ip->base \
	+ offset__AUD_SPDIF_ITS(ip)) & ~(mask__AUD_SPDIF_ITS__EOBURST(ip) << \
	shift__AUD_SPDIF_ITS__EOBURST(ip))) | (((value) & \
	mask__AUD_SPDIF_ITS__EOBURST(ip)) << \
	shift__AUD_SPDIF_ITS__EOBURST(ip)), ip->base + \
	offset__AUD_SPDIF_ITS(ip))

#define value__AUD_SPDIF_ITS__EOBURST__PENDING(ip) 0x1
#define mask__AUD_SPDIF_ITS__EOBURST__PENDING(ip) \
	(value__AUD_SPDIF_ITS__EOBURST__PENDING(ip) << \
	shift__AUD_SPDIF_ITS__EOBURST(ip))
#define set__AUD_SPDIF_ITS__EOBURST__PENDING(ip) \
	set__AUD_SPDIF_ITS__EOBURST(ip, \
	value__AUD_SPDIF_ITS__EOBURST__PENDING(ip))

/* EOBLOCK */

#define shift__AUD_SPDIF_ITS__EOBLOCK(ip) 2
#define mask__AUD_SPDIF_ITS__EOBLOCK(ip) 0x1
#define get__AUD_SPDIF_ITS__EOBLOCK(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_ITS(ip)) >> shift__AUD_SPDIF_ITS__EOBLOCK(ip)) & \
	mask__AUD_SPDIF_ITS__EOBLOCK(ip))
#define set__AUD_SPDIF_ITS__EOBLOCK(ip, value) writel((readl(ip->base \
	+ offset__AUD_SPDIF_ITS(ip)) & ~(mask__AUD_SPDIF_ITS__EOBLOCK(ip) << \
	shift__AUD_SPDIF_ITS__EOBLOCK(ip))) | (((value) & \
	mask__AUD_SPDIF_ITS__EOBLOCK(ip)) << \
	shift__AUD_SPDIF_ITS__EOBLOCK(ip)), ip->base + \
	offset__AUD_SPDIF_ITS(ip))

#define value__AUD_SPDIF_ITS__EOBLOCK__PENDING(ip) 0x1
#define mask__AUD_SPDIF_ITS__EOBLOCK__PENDING(ip) \
	(value__AUD_SPDIF_ITS__EOBLOCK__PENDING(ip) << \
	shift__AUD_SPDIF_ITS__EOBLOCK(ip))
#define set__AUD_SPDIF_ITS__EOBLOCK__PENDING(ip) \
	set__AUD_SPDIF_ITS__EOBLOCK(ip, \
	value__AUD_SPDIF_ITS__EOBLOCK__PENDING(ip))

/* EOLATENCY */

#define shift__AUD_SPDIF_ITS__EOLATENCY(ip) 3
#define mask__AUD_SPDIF_ITS__EOLATENCY(ip) 0x1
#define get__AUD_SPDIF_ITS__EOLATENCY(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_ITS(ip)) >> shift__AUD_SPDIF_ITS__EOLATENCY(ip)) & \
	mask__AUD_SPDIF_ITS__EOLATENCY(ip))
#define set__AUD_SPDIF_ITS__EOLATENCY(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_ITS(ip)) & \
	~(mask__AUD_SPDIF_ITS__EOLATENCY(ip) << \
	shift__AUD_SPDIF_ITS__EOLATENCY(ip))) | (((value) & \
	mask__AUD_SPDIF_ITS__EOLATENCY(ip)) << \
	shift__AUD_SPDIF_ITS__EOLATENCY(ip)), ip->base + \
	offset__AUD_SPDIF_ITS(ip))

#define value__AUD_SPDIF_ITS__EOLATENCY__PENDING(ip) 0x1
#define mask__AUD_SPDIF_ITS__EOLATENCY__PENDING(ip) \
	(value__AUD_SPDIF_ITS__EOLATENCY__PENDING(ip) << \
	shift__AUD_SPDIF_ITS__EOLATENCY(ip))
#define set__AUD_SPDIF_ITS__EOLATENCY__PENDING(ip) \
	set__AUD_SPDIF_ITS__EOLATENCY(ip, \
	value__AUD_SPDIF_ITS__EOLATENCY__PENDING(ip))

/* EOPD */

#define shift__AUD_SPDIF_ITS__EOPD(ip) 4
#define mask__AUD_SPDIF_ITS__EOPD(ip) 0x1
#define get__AUD_SPDIF_ITS__EOPD(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_ITS(ip)) >> shift__AUD_SPDIF_ITS__EOPD(ip)) & \
	mask__AUD_SPDIF_ITS__EOPD(ip))
#define set__AUD_SPDIF_ITS__EOPD(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_ITS(ip)) & ~(mask__AUD_SPDIF_ITS__EOPD(ip) << \
	shift__AUD_SPDIF_ITS__EOPD(ip))) | (((value) & \
	mask__AUD_SPDIF_ITS__EOPD(ip)) << shift__AUD_SPDIF_ITS__EOPD(ip)), \
	ip->base + offset__AUD_SPDIF_ITS(ip))

#define value__AUD_SPDIF_ITS__EOPD__PENDING(ip) 0x1
#define mask__AUD_SPDIF_ITS__EOPD__PENDING(ip) \
	(value__AUD_SPDIF_ITS__EOPD__PENDING(ip) << \
	shift__AUD_SPDIF_ITS__EOPD(ip))
#define set__AUD_SPDIF_ITS__EOPD__PENDING(ip) \
	set__AUD_SPDIF_ITS__EOPD(ip, value__AUD_SPDIF_ITS__EOPD__PENDING(ip))

/* NSAMPLE */

#define shift__AUD_SPDIF_ITS__NSAMPLE(ip) 5
#define mask__AUD_SPDIF_ITS__NSAMPLE(ip) 0x1
#define get__AUD_SPDIF_ITS__NSAMPLE(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_ITS(ip)) >> shift__AUD_SPDIF_ITS__NSAMPLE(ip)) & \
	mask__AUD_SPDIF_ITS__NSAMPLE(ip))
#define set__AUD_SPDIF_ITS__NSAMPLE(ip, value) writel((readl(ip->base \
	+ offset__AUD_SPDIF_ITS(ip)) & ~(mask__AUD_SPDIF_ITS__NSAMPLE(ip) << \
	shift__AUD_SPDIF_ITS__NSAMPLE(ip))) | (((value) & \
	mask__AUD_SPDIF_ITS__NSAMPLE(ip)) << \
	shift__AUD_SPDIF_ITS__NSAMPLE(ip)), ip->base + \
	offset__AUD_SPDIF_ITS(ip))

#define value__AUD_SPDIF_ITS__NSAMPLE__PENDING(ip) 0x1
#define mask__AUD_SPDIF_ITS__NSAMPLE__PENDING(ip) \
	(value__AUD_SPDIF_ITS__NSAMPLE__PENDING(ip) << \
	shift__AUD_SPDIF_ITS__NSAMPLE(ip))
#define set__AUD_SPDIF_ITS__NSAMPLE__PENDING(ip) \
	set__AUD_SPDIF_ITS__NSAMPLE(ip, \
	value__AUD_SPDIF_ITS__NSAMPLE__PENDING(ip))



/*
 * AUD_SPDIF_ITS_CLR
 */

#define offset__AUD_SPDIF_ITS_CLR(ip) 0x0c
#define get__AUD_SPDIF_ITS_CLR(ip) readl(ip->base + \
	offset__AUD_SPDIF_ITS_CLR(ip))
#define set__AUD_SPDIF_ITS_CLR(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_ITS_CLR(ip))

/* UNF */

#define shift__AUD_SPDIF_ITS_CLR__UNF(ip) 0
#define mask__AUD_SPDIF_ITS_CLR__UNF(ip) 0x1
#define get__AUD_SPDIF_ITS_CLR__UNF(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_ITS_CLR(ip)) >> shift__AUD_SPDIF_ITS_CLR__UNF(ip)) & \
	mask__AUD_SPDIF_ITS_CLR__UNF(ip))
#define set__AUD_SPDIF_ITS_CLR__UNF(ip, value) writel((readl(ip->base \
	+ offset__AUD_SPDIF_ITS_CLR(ip)) & ~(mask__AUD_SPDIF_ITS_CLR__UNF(ip) \
	<< shift__AUD_SPDIF_ITS_CLR__UNF(ip))) | (((value) & \
	mask__AUD_SPDIF_ITS_CLR__UNF(ip)) << \
	shift__AUD_SPDIF_ITS_CLR__UNF(ip)), ip->base + \
	offset__AUD_SPDIF_ITS_CLR(ip))

#define value__AUD_SPDIF_ITS_CLR__UNF__CLEAR(ip) 0x1
#define mask__AUD_SPDIF_ITS_CLR__UNF__CLEAR(ip) \
	(value__AUD_SPDIF_ITS_CLR__UNF__CLEAR(ip) << \
	shift__AUD_SPDIF_ITS_CLR__UNF(ip))
#define set__AUD_SPDIF_ITS_CLR__UNF__CLEAR(ip) \
	set__AUD_SPDIF_ITS_CLR__UNF(ip, \
	value__AUD_SPDIF_ITS_CLR__UNF__CLEAR(ip))

/* EOBURST */

#define shift__AUD_SPDIF_ITS_CLR__EOBURST(ip) 1
#define mask__AUD_SPDIF_ITS_CLR__EOBURST(ip) 0x1
#define get__AUD_SPDIF_ITS_CLR__EOBURST(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_ITS_CLR(ip)) >> \
	shift__AUD_SPDIF_ITS_CLR__EOBURST(ip)) & \
	mask__AUD_SPDIF_ITS_CLR__EOBURST(ip))
#define set__AUD_SPDIF_ITS_CLR__EOBURST(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_ITS_CLR(ip)) & \
	~(mask__AUD_SPDIF_ITS_CLR__EOBURST(ip) << \
	shift__AUD_SPDIF_ITS_CLR__EOBURST(ip))) | (((value) & \
	mask__AUD_SPDIF_ITS_CLR__EOBURST(ip)) << \
	shift__AUD_SPDIF_ITS_CLR__EOBURST(ip)), ip->base + \
	offset__AUD_SPDIF_ITS_CLR(ip))

#define value__AUD_SPDIF_ITS_CLR__EOBURST__CLEAR(ip) 0x1
#define mask__AUD_SPDIF_ITS_CLR__EOBURST__CLEAR(ip) \
	(value__AUD_SPDIF_ITS_CLR__EOBURST__CLEAR(ip) << \
	shift__AUD_SPDIF_ITS_CLR__EOBURST(ip))
#define set__AUD_SPDIF_ITS_CLR__EOBURST__CLEAR(ip) \
	set__AUD_SPDIF_ITS_CLR__EOBURST(ip, \
	value__AUD_SPDIF_ITS_CLR__EOBURST__CLEAR(ip))

/* EOBLOCK */

#define shift__AUD_SPDIF_ITS_CLR__EOBLOCK(ip) 2
#define mask__AUD_SPDIF_ITS_CLR__EOBLOCK(ip) 0x1
#define get__AUD_SPDIF_ITS_CLR__EOBLOCK(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_ITS_CLR(ip)) >> \
	shift__AUD_SPDIF_ITS_CLR__EOBLOCK(ip)) & \
	mask__AUD_SPDIF_ITS_CLR__EOBLOCK(ip))
#define set__AUD_SPDIF_ITS_CLR__EOBLOCK(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_ITS_CLR(ip)) & \
	~(mask__AUD_SPDIF_ITS_CLR__EOBLOCK(ip) << \
	shift__AUD_SPDIF_ITS_CLR__EOBLOCK(ip))) | (((value) & \
	mask__AUD_SPDIF_ITS_CLR__EOBLOCK(ip)) << \
	shift__AUD_SPDIF_ITS_CLR__EOBLOCK(ip)), ip->base + \
	offset__AUD_SPDIF_ITS_CLR(ip))

#define value__AUD_SPDIF_ITS_CLR__EOBLOCK__CLEAR(ip) 0x1
#define mask__AUD_SPDIF_ITS_CLR__EOBLOCK__CLEAR(ip) \
	(value__AUD_SPDIF_ITS_CLR__EOBLOCK__CLEAR(ip) << \
	shift__AUD_SPDIF_ITS_CLR__EOBLOCK(ip))
#define set__AUD_SPDIF_ITS_CLR__EOBLOCK__CLEAR(ip) \
	set__AUD_SPDIF_ITS_CLR__EOBLOCK(ip, \
	value__AUD_SPDIF_ITS_CLR__EOBLOCK__CLEAR(ip))

/* EOLATENCY */

#define shift__AUD_SPDIF_ITS_CLR__EOLATENCY(ip) 3
#define mask__AUD_SPDIF_ITS_CLR__EOLATENCY(ip) 0x1
#define get__AUD_SPDIF_ITS_CLR__EOLATENCY(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_ITS_CLR(ip)) >> \
	shift__AUD_SPDIF_ITS_CLR__EOLATENCY(ip)) & \
	mask__AUD_SPDIF_ITS_CLR__EOLATENCY(ip))
#define set__AUD_SPDIF_ITS_CLR__EOLATENCY(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_ITS_CLR(ip)) & \
	~(mask__AUD_SPDIF_ITS_CLR__EOLATENCY(ip) << \
	shift__AUD_SPDIF_ITS_CLR__EOLATENCY(ip))) | (((value) & \
	mask__AUD_SPDIF_ITS_CLR__EOLATENCY(ip)) << \
	shift__AUD_SPDIF_ITS_CLR__EOLATENCY(ip)), ip->base + \
	offset__AUD_SPDIF_ITS_CLR(ip))

#define value__AUD_SPDIF_ITS_CLR__EOLATENCY__CLEAR(ip) 0x1
#define mask__AUD_SPDIF_ITS_CLR__EOLATENCY__CLEAR(ip) \
	(value__AUD_SPDIF_ITS_CLR__EOLATENCY__CLEAR(ip) << \
	shift__AUD_SPDIF_ITS_CLR__EOLATENCY(ip))
#define set__AUD_SPDIF_ITS_CLR__EOLATENCY__CLEAR(ip) \
	set__AUD_SPDIF_ITS_CLR__EOLATENCY(ip, \
	value__AUD_SPDIF_ITS_CLR__EOLATENCY__CLEAR(ip))

/* EOPD */

#define shift__AUD_SPDIF_ITS_CLR__EOPD(ip) 4
#define mask__AUD_SPDIF_ITS_CLR__EOPD(ip) 0x1
#define get__AUD_SPDIF_ITS_CLR__EOPD(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_ITS_CLR(ip)) >> shift__AUD_SPDIF_ITS_CLR__EOPD(ip)) \
	& mask__AUD_SPDIF_ITS_CLR__EOPD(ip))
#define set__AUD_SPDIF_ITS_CLR__EOPD(ip, value) writel((readl(ip->base \
	+ offset__AUD_SPDIF_ITS_CLR(ip)) & ~(mask__AUD_SPDIF_ITS_CLR__EOPD(ip) \
	<< shift__AUD_SPDIF_ITS_CLR__EOPD(ip))) | (((value) & \
	mask__AUD_SPDIF_ITS_CLR__EOPD(ip)) << \
	shift__AUD_SPDIF_ITS_CLR__EOPD(ip)), ip->base + \
	offset__AUD_SPDIF_ITS_CLR(ip))

#define value__AUD_SPDIF_ITS_CLR__EOPD__CLEAR(ip) 0x1
#define mask__AUD_SPDIF_ITS_CLR__EOPD__CLEAR(ip) \
	(value__AUD_SPDIF_ITS_CLR__EOPD__CLEAR(ip) << \
	shift__AUD_SPDIF_ITS_CLR__EOPD(ip))
#define set__AUD_SPDIF_ITS_CLR__EOPD__CLEAR(ip) \
	set__AUD_SPDIF_ITS_CLR__EOPD(ip, \
	value__AUD_SPDIF_ITS_CLR__EOPD__CLEAR(ip))

/* NSAMPLE */

#define shift__AUD_SPDIF_ITS_CLR__NSAMPLE(ip) 5
#define mask__AUD_SPDIF_ITS_CLR__NSAMPLE(ip) 0x1
#define get__AUD_SPDIF_ITS_CLR__NSAMPLE(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_ITS_CLR(ip)) >> \
	shift__AUD_SPDIF_ITS_CLR__NSAMPLE(ip)) & \
	mask__AUD_SPDIF_ITS_CLR__NSAMPLE(ip))
#define set__AUD_SPDIF_ITS_CLR__NSAMPLE(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_ITS_CLR(ip)) & \
	~(mask__AUD_SPDIF_ITS_CLR__NSAMPLE(ip) << \
	shift__AUD_SPDIF_ITS_CLR__NSAMPLE(ip))) | (((value) & \
	mask__AUD_SPDIF_ITS_CLR__NSAMPLE(ip)) << \
	shift__AUD_SPDIF_ITS_CLR__NSAMPLE(ip)), ip->base + \
	offset__AUD_SPDIF_ITS_CLR(ip))

#define value__AUD_SPDIF_ITS_CLR__NSAMPLE__CLEAR(ip) 0x1
#define mask__AUD_SPDIF_ITS_CLR__NSAMPLE__CLEAR(ip) \
	(value__AUD_SPDIF_ITS_CLR__NSAMPLE__CLEAR(ip) << \
	shift__AUD_SPDIF_ITS_CLR__NSAMPLE(ip))
#define set__AUD_SPDIF_ITS_CLR__NSAMPLE__CLEAR(ip) \
	set__AUD_SPDIF_ITS_CLR__NSAMPLE(ip, \
	value__AUD_SPDIF_ITS_CLR__NSAMPLE__CLEAR(ip))



/*
 * AUD_SPDIF_IT_EN
 */

#define offset__AUD_SPDIF_IT_EN(ip) 0x10
#define get__AUD_SPDIF_IT_EN(ip) readl(ip->base + \
	offset__AUD_SPDIF_IT_EN(ip))
#define set__AUD_SPDIF_IT_EN(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_IT_EN(ip))

/* UNF */

#define shift__AUD_SPDIF_IT_EN__UNF(ip) 0
#define mask__AUD_SPDIF_IT_EN__UNF(ip) 0x1
#define get__AUD_SPDIF_IT_EN__UNF(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN(ip)) >> shift__AUD_SPDIF_IT_EN__UNF(ip)) & \
	mask__AUD_SPDIF_IT_EN__UNF(ip))
#define set__AUD_SPDIF_IT_EN__UNF(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN(ip)) & ~(mask__AUD_SPDIF_IT_EN__UNF(ip) << \
	shift__AUD_SPDIF_IT_EN__UNF(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN__UNF(ip)) << shift__AUD_SPDIF_IT_EN__UNF(ip)), \
	ip->base + offset__AUD_SPDIF_IT_EN(ip))

#define value__AUD_SPDIF_IT_EN__UNF__DISABLED(ip) 0x0
#define mask__AUD_SPDIF_IT_EN__UNF__DISABLED(ip) \
	(value__AUD_SPDIF_IT_EN__UNF__DISABLED(ip) << \
	shift__AUD_SPDIF_IT_EN__UNF(ip))
#define set__AUD_SPDIF_IT_EN__UNF__DISABLED(ip) \
	set__AUD_SPDIF_IT_EN__UNF(ip, \
	value__AUD_SPDIF_IT_EN__UNF__DISABLED(ip))

#define value__AUD_SPDIF_IT_EN__UNF__ENABLED(ip) 0x1
#define mask__AUD_SPDIF_IT_EN__UNF__ENABLED(ip) \
	(value__AUD_SPDIF_IT_EN__UNF__ENABLED(ip) << \
	shift__AUD_SPDIF_IT_EN__UNF(ip))
#define set__AUD_SPDIF_IT_EN__UNF__ENABLED(ip) \
	set__AUD_SPDIF_IT_EN__UNF(ip, \
	value__AUD_SPDIF_IT_EN__UNF__ENABLED(ip))

/* EOBURST */

#define shift__AUD_SPDIF_IT_EN__EOBURST(ip) 1
#define mask__AUD_SPDIF_IT_EN__EOBURST(ip) 0x1
#define get__AUD_SPDIF_IT_EN__EOBURST(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN(ip)) >> shift__AUD_SPDIF_IT_EN__EOBURST(ip)) & \
	mask__AUD_SPDIF_IT_EN__EOBURST(ip))
#define set__AUD_SPDIF_IT_EN__EOBURST(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN(ip)) & \
	~(mask__AUD_SPDIF_IT_EN__EOBURST(ip) << \
	shift__AUD_SPDIF_IT_EN__EOBURST(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN__EOBURST(ip)) << \
	shift__AUD_SPDIF_IT_EN__EOBURST(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN(ip))

#define value__AUD_SPDIF_IT_EN__EOBURST__DISABLED(ip) 0x0
#define mask__AUD_SPDIF_IT_EN__EOBURST__DISABLED(ip) \
	(value__AUD_SPDIF_IT_EN__EOBURST__DISABLED(ip) << \
	shift__AUD_SPDIF_IT_EN__EOBURST(ip))
#define set__AUD_SPDIF_IT_EN__EOBURST__DISABLED(ip) \
	set__AUD_SPDIF_IT_EN__EOBURST(ip, \
	value__AUD_SPDIF_IT_EN__EOBURST__DISABLED(ip))

#define value__AUD_SPDIF_IT_EN__EOBURST__ENABLED(ip) 0x1
#define mask__AUD_SPDIF_IT_EN__EOBURST__ENABLED(ip) \
	(value__AUD_SPDIF_IT_EN__EOBURST__ENABLED(ip) << \
	shift__AUD_SPDIF_IT_EN__EOBURST(ip))
#define set__AUD_SPDIF_IT_EN__EOBURST__ENABLED(ip) \
	set__AUD_SPDIF_IT_EN__EOBURST(ip, \
	value__AUD_SPDIF_IT_EN__EOBURST__ENABLED(ip))

/* EOBLOCK */

#define shift__AUD_SPDIF_IT_EN__EOBLOCK(ip) 2
#define mask__AUD_SPDIF_IT_EN__EOBLOCK(ip) 0x1
#define get__AUD_SPDIF_IT_EN__EOBLOCK(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN(ip)) >> shift__AUD_SPDIF_IT_EN__EOBLOCK(ip)) & \
	mask__AUD_SPDIF_IT_EN__EOBLOCK(ip))
#define set__AUD_SPDIF_IT_EN__EOBLOCK(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN(ip)) & \
	~(mask__AUD_SPDIF_IT_EN__EOBLOCK(ip) << \
	shift__AUD_SPDIF_IT_EN__EOBLOCK(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN__EOBLOCK(ip)) << \
	shift__AUD_SPDIF_IT_EN__EOBLOCK(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN(ip))

#define value__AUD_SPDIF_IT_EN__EOBLOCK__DISABLED(ip) 0x0
#define mask__AUD_SPDIF_IT_EN__EOBLOCK__DISABLED(ip) \
	(value__AUD_SPDIF_IT_EN__EOBLOCK__DISABLED(ip) << \
	shift__AUD_SPDIF_IT_EN__EOBLOCK(ip))
#define set__AUD_SPDIF_IT_EN__EOBLOCK__DISABLED(ip) \
	set__AUD_SPDIF_IT_EN__EOBLOCK(ip, \
	value__AUD_SPDIF_IT_EN__EOBLOCK__DISABLED(ip))

#define value__AUD_SPDIF_IT_EN__EOBLOCK__ENABLED(ip) 0x1
#define mask__AUD_SPDIF_IT_EN__EOBLOCK__ENABLED(ip) \
	(value__AUD_SPDIF_IT_EN__EOBLOCK__ENABLED(ip) << \
	shift__AUD_SPDIF_IT_EN__EOBLOCK(ip))
#define set__AUD_SPDIF_IT_EN__EOBLOCK__ENABLED(ip) \
	set__AUD_SPDIF_IT_EN__EOBLOCK(ip, \
	value__AUD_SPDIF_IT_EN__EOBLOCK__ENABLED(ip))

/* EOLATENCY */

#define shift__AUD_SPDIF_IT_EN__EOLATENCY(ip) 3
#define mask__AUD_SPDIF_IT_EN__EOLATENCY(ip) 0x1
#define get__AUD_SPDIF_IT_EN__EOLATENCY(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN(ip)) >> shift__AUD_SPDIF_IT_EN__EOLATENCY(ip)) \
	& mask__AUD_SPDIF_IT_EN__EOLATENCY(ip))
#define set__AUD_SPDIF_IT_EN__EOLATENCY(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN(ip)) & \
	~(mask__AUD_SPDIF_IT_EN__EOLATENCY(ip) << \
	shift__AUD_SPDIF_IT_EN__EOLATENCY(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN__EOLATENCY(ip)) << \
	shift__AUD_SPDIF_IT_EN__EOLATENCY(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN(ip))

#define value__AUD_SPDIF_IT_EN__EOLATENCY__DISABLED(ip) 0x0
#define mask__AUD_SPDIF_IT_EN__EOLATENCY__DISABLED(ip) \
	(value__AUD_SPDIF_IT_EN__EOLATENCY__DISABLED(ip) << \
	shift__AUD_SPDIF_IT_EN__EOLATENCY(ip))
#define set__AUD_SPDIF_IT_EN__EOLATENCY__DISABLED(ip) \
	set__AUD_SPDIF_IT_EN__EOLATENCY(ip, \
	value__AUD_SPDIF_IT_EN__EOLATENCY__DISABLED(ip))

#define value__AUD_SPDIF_IT_EN__EOLATENCY__ENABLED(ip) 0x1
#define mask__AUD_SPDIF_IT_EN__EOLATENCY__ENABLED(ip) \
	(value__AUD_SPDIF_IT_EN__EOLATENCY__ENABLED(ip) << \
	shift__AUD_SPDIF_IT_EN__EOLATENCY(ip))
#define set__AUD_SPDIF_IT_EN__EOLATENCY__ENABLED(ip) \
	set__AUD_SPDIF_IT_EN__EOLATENCY(ip, \
	value__AUD_SPDIF_IT_EN__EOLATENCY__ENABLED(ip))

/* EOPD */

#define shift__AUD_SPDIF_IT_EN__EOPD(ip) 4
#define mask__AUD_SPDIF_IT_EN__EOPD(ip) 0x1
#define get__AUD_SPDIF_IT_EN__EOPD(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN(ip)) >> shift__AUD_SPDIF_IT_EN__EOPD(ip)) & \
	mask__AUD_SPDIF_IT_EN__EOPD(ip))
#define set__AUD_SPDIF_IT_EN__EOPD(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN(ip)) & ~(mask__AUD_SPDIF_IT_EN__EOPD(ip) << \
	shift__AUD_SPDIF_IT_EN__EOPD(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN__EOPD(ip)) << shift__AUD_SPDIF_IT_EN__EOPD(ip)), \
	ip->base + offset__AUD_SPDIF_IT_EN(ip))

#define value__AUD_SPDIF_IT_EN__EOPD__DISABLED(ip) 0x0
#define mask__AUD_SPDIF_IT_EN__EOPD__DISABLED(ip) \
	(value__AUD_SPDIF_IT_EN__EOPD__DISABLED(ip) << \
	shift__AUD_SPDIF_IT_EN__EOPD(ip))
#define set__AUD_SPDIF_IT_EN__EOPD__DISABLED(ip) \
	set__AUD_SPDIF_IT_EN__EOPD(ip, \
	value__AUD_SPDIF_IT_EN__EOPD__DISABLED(ip))

#define value__AUD_SPDIF_IT_EN__EOPD__ENABLED(ip) 0x1
#define mask__AUD_SPDIF_IT_EN__EOPD__ENABLED(ip) \
	(value__AUD_SPDIF_IT_EN__EOPD__ENABLED(ip) << \
	shift__AUD_SPDIF_IT_EN__EOPD(ip))
#define set__AUD_SPDIF_IT_EN__EOPD__ENABLED(ip) \
	set__AUD_SPDIF_IT_EN__EOPD(ip, \
	value__AUD_SPDIF_IT_EN__EOPD__ENABLED(ip))

/* NSAMPLE */

#define shift__AUD_SPDIF_IT_EN__NSAMPLE(ip) 5
#define mask__AUD_SPDIF_IT_EN__NSAMPLE(ip) 0x1
#define get__AUD_SPDIF_IT_EN__NSAMPLE(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN(ip)) >> shift__AUD_SPDIF_IT_EN__NSAMPLE(ip)) & \
	mask__AUD_SPDIF_IT_EN__NSAMPLE(ip))
#define set__AUD_SPDIF_IT_EN__NSAMPLE(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN(ip)) & \
	~(mask__AUD_SPDIF_IT_EN__NSAMPLE(ip) << \
	shift__AUD_SPDIF_IT_EN__NSAMPLE(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN__NSAMPLE(ip)) << \
	shift__AUD_SPDIF_IT_EN__NSAMPLE(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN(ip))

#define value__AUD_SPDIF_IT_EN__NSAMPLE__DISABLED(ip) 0x0
#define mask__AUD_SPDIF_IT_EN__NSAMPLE__DISABLED(ip) \
	(value__AUD_SPDIF_IT_EN__NSAMPLE__DISABLED(ip) << \
	shift__AUD_SPDIF_IT_EN__NSAMPLE(ip))
#define set__AUD_SPDIF_IT_EN__NSAMPLE__DISABLED(ip) \
	set__AUD_SPDIF_IT_EN__NSAMPLE(ip, \
	value__AUD_SPDIF_IT_EN__NSAMPLE__DISABLED(ip))

#define value__AUD_SPDIF_IT_EN__NSAMPLE__ENABLED(ip) 0x1
#define mask__AUD_SPDIF_IT_EN__NSAMPLE__ENABLED(ip) \
	(value__AUD_SPDIF_IT_EN__NSAMPLE__ENABLED(ip) << \
	shift__AUD_SPDIF_IT_EN__NSAMPLE(ip))
#define set__AUD_SPDIF_IT_EN__NSAMPLE__ENABLED(ip) \
	set__AUD_SPDIF_IT_EN__NSAMPLE(ip, \
	value__AUD_SPDIF_IT_EN__NSAMPLE__ENABLED(ip))



/*
 * AUD_SPDIF_IT_EN_SET
 */

#define offset__AUD_SPDIF_IT_EN_SET(ip) 0x14
#define get__AUD_SPDIF_IT_EN_SET(ip) readl(ip->base + \
	offset__AUD_SPDIF_IT_EN_SET(ip))
#define set__AUD_SPDIF_IT_EN_SET(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_IT_EN_SET(ip))

/* UNF */

#define shift__AUD_SPDIF_IT_EN_SET__UNF(ip) 0
#define mask__AUD_SPDIF_IT_EN_SET__UNF(ip) 0x1
#define get__AUD_SPDIF_IT_EN_SET__UNF(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN_SET(ip)) >> \
	shift__AUD_SPDIF_IT_EN_SET__UNF(ip)) & \
	mask__AUD_SPDIF_IT_EN_SET__UNF(ip))
#define set__AUD_SPDIF_IT_EN_SET__UNF(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN_SET(ip)) & \
	~(mask__AUD_SPDIF_IT_EN_SET__UNF(ip) << \
	shift__AUD_SPDIF_IT_EN_SET__UNF(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN_SET__UNF(ip)) << \
	shift__AUD_SPDIF_IT_EN_SET__UNF(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN_SET(ip))

#define value__AUD_SPDIF_IT_EN_SET__UNF__SET(ip) 0x1
#define mask__AUD_SPDIF_IT_EN_SET__UNF__SET(ip) \
	(value__AUD_SPDIF_IT_EN_SET__UNF__SET(ip) << \
	shift__AUD_SPDIF_IT_EN_SET__UNF(ip))
#define set__AUD_SPDIF_IT_EN_SET__UNF__SET(ip) \
	set__AUD_SPDIF_IT_EN_SET__UNF(ip, \
	value__AUD_SPDIF_IT_EN_SET__UNF__SET(ip))

/* EOBURST */

#define shift__AUD_SPDIF_IT_EN_SET__EOBURST(ip) 1
#define mask__AUD_SPDIF_IT_EN_SET__EOBURST(ip) 0x1
#define get__AUD_SPDIF_IT_EN_SET__EOBURST(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN_SET(ip)) >> \
	shift__AUD_SPDIF_IT_EN_SET__EOBURST(ip)) & \
	mask__AUD_SPDIF_IT_EN_SET__EOBURST(ip))
#define set__AUD_SPDIF_IT_EN_SET__EOBURST(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN_SET(ip)) & \
	~(mask__AUD_SPDIF_IT_EN_SET__EOBURST(ip) << \
	shift__AUD_SPDIF_IT_EN_SET__EOBURST(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN_SET__EOBURST(ip)) << \
	shift__AUD_SPDIF_IT_EN_SET__EOBURST(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN_SET(ip))

#define value__AUD_SPDIF_IT_EN_SET__EOBURST__SET(ip) 0x1
#define mask__AUD_SPDIF_IT_EN_SET__EOBURST__SET(ip) \
	(value__AUD_SPDIF_IT_EN_SET__EOBURST__SET(ip) << \
	shift__AUD_SPDIF_IT_EN_SET__EOBURST(ip))
#define set__AUD_SPDIF_IT_EN_SET__EOBURST__SET(ip) \
	set__AUD_SPDIF_IT_EN_SET__EOBURST(ip, \
	value__AUD_SPDIF_IT_EN_SET__EOBURST__SET(ip))

/* EOBLOCK */

#define shift__AUD_SPDIF_IT_EN_SET__EOBLOCK(ip) 2
#define mask__AUD_SPDIF_IT_EN_SET__EOBLOCK(ip) 0x1
#define get__AUD_SPDIF_IT_EN_SET__EOBLOCK(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN_SET(ip)) >> \
	shift__AUD_SPDIF_IT_EN_SET__EOBLOCK(ip)) & \
	mask__AUD_SPDIF_IT_EN_SET__EOBLOCK(ip))
#define set__AUD_SPDIF_IT_EN_SET__EOBLOCK(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN_SET(ip)) & \
	~(mask__AUD_SPDIF_IT_EN_SET__EOBLOCK(ip) << \
	shift__AUD_SPDIF_IT_EN_SET__EOBLOCK(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN_SET__EOBLOCK(ip)) << \
	shift__AUD_SPDIF_IT_EN_SET__EOBLOCK(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN_SET(ip))

#define value__AUD_SPDIF_IT_EN_SET__EOBLOCK__SET(ip) 0x1
#define mask__AUD_SPDIF_IT_EN_SET__EOBLOCK__SET(ip) \
	(value__AUD_SPDIF_IT_EN_SET__EOBLOCK__SET(ip) << \
	shift__AUD_SPDIF_IT_EN_SET__EOBLOCK(ip))
#define set__AUD_SPDIF_IT_EN_SET__EOBLOCK__SET(ip) \
	set__AUD_SPDIF_IT_EN_SET__EOBLOCK(ip, \
	value__AUD_SPDIF_IT_EN_SET__EOBLOCK__SET(ip))

/* EOLATENCY */

#define shift__AUD_SPDIF_IT_EN_SET__EOLATENCY(ip) 3
#define mask__AUD_SPDIF_IT_EN_SET__EOLATENCY(ip) 0x1
#define get__AUD_SPDIF_IT_EN_SET__EOLATENCY(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN_SET(ip)) >> \
	shift__AUD_SPDIF_IT_EN_SET__EOLATENCY(ip)) & \
	mask__AUD_SPDIF_IT_EN_SET__EOLATENCY(ip))
#define set__AUD_SPDIF_IT_EN_SET__EOLATENCY(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN_SET(ip)) & \
	~(mask__AUD_SPDIF_IT_EN_SET__EOLATENCY(ip) << \
	shift__AUD_SPDIF_IT_EN_SET__EOLATENCY(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN_SET__EOLATENCY(ip)) << \
	shift__AUD_SPDIF_IT_EN_SET__EOLATENCY(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN_SET(ip))

#define value__AUD_SPDIF_IT_EN_SET__EOLATENCY__SET(ip) 0x1
#define mask__AUD_SPDIF_IT_EN_SET__EOLATENCY__SET(ip) \
	(value__AUD_SPDIF_IT_EN_SET__EOLATENCY__SET(ip) << \
	shift__AUD_SPDIF_IT_EN_SET__EOLATENCY(ip))
#define set__AUD_SPDIF_IT_EN_SET__EOLATENCY__SET(ip) \
	set__AUD_SPDIF_IT_EN_SET__EOLATENCY(ip, \
	value__AUD_SPDIF_IT_EN_SET__EOLATENCY__SET(ip))

/* EOPD */

#define shift__AUD_SPDIF_IT_EN_SET__EOPD(ip) 4
#define mask__AUD_SPDIF_IT_EN_SET__EOPD(ip) 0x1
#define get__AUD_SPDIF_IT_EN_SET__EOPD(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN_SET(ip)) >> \
	shift__AUD_SPDIF_IT_EN_SET__EOPD(ip)) & \
	mask__AUD_SPDIF_IT_EN_SET__EOPD(ip))
#define set__AUD_SPDIF_IT_EN_SET__EOPD(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN_SET(ip)) & \
	~(mask__AUD_SPDIF_IT_EN_SET__EOPD(ip) << \
	shift__AUD_SPDIF_IT_EN_SET__EOPD(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN_SET__EOPD(ip)) << \
	shift__AUD_SPDIF_IT_EN_SET__EOPD(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN_SET(ip))

#define value__AUD_SPDIF_IT_EN_SET__EOPD__SET(ip) 0x1
#define mask__AUD_SPDIF_IT_EN_SET__EOPD__SET(ip) \
	(value__AUD_SPDIF_IT_EN_SET__EOPD__SET(ip) << \
	shift__AUD_SPDIF_IT_EN_SET__EOPD(ip))
#define set__AUD_SPDIF_IT_EN_SET__EOPD__SET(ip) \
	set__AUD_SPDIF_IT_EN_SET__EOPD(ip, \
	value__AUD_SPDIF_IT_EN_SET__EOPD__SET(ip))

/* NSAMPLE */

#define shift__AUD_SPDIF_IT_EN_SET__NSAMPLE(ip) 5
#define mask__AUD_SPDIF_IT_EN_SET__NSAMPLE(ip) 0x1
#define get__AUD_SPDIF_IT_EN_SET__NSAMPLE(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN_SET(ip)) >> \
	shift__AUD_SPDIF_IT_EN_SET__NSAMPLE(ip)) & \
	mask__AUD_SPDIF_IT_EN_SET__NSAMPLE(ip))
#define set__AUD_SPDIF_IT_EN_SET__NSAMPLE(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN_SET(ip)) & \
	~(mask__AUD_SPDIF_IT_EN_SET__NSAMPLE(ip) << \
	shift__AUD_SPDIF_IT_EN_SET__NSAMPLE(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN_SET__NSAMPLE(ip)) << \
	shift__AUD_SPDIF_IT_EN_SET__NSAMPLE(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN_SET(ip))

#define value__AUD_SPDIF_IT_EN_SET__NSAMPLE__SET(ip) 0x1
#define mask__AUD_SPDIF_IT_EN_SET__NSAMPLE__SET(ip) \
	(value__AUD_SPDIF_IT_EN_SET__NSAMPLE__SET(ip) << \
	shift__AUD_SPDIF_IT_EN_SET__NSAMPLE(ip))
#define set__AUD_SPDIF_IT_EN_SET__NSAMPLE__SET(ip) \
	set__AUD_SPDIF_IT_EN_SET__NSAMPLE(ip, \
	value__AUD_SPDIF_IT_EN_SET__NSAMPLE__SET(ip))



/*
 * AUD_SPDIF_IT_EN_CLR
 */

#define offset__AUD_SPDIF_IT_EN_CLR(ip) 0x18
#define get__AUD_SPDIF_IT_EN_CLR(ip) readl(ip->base + \
	offset__AUD_SPDIF_IT_EN_CLR(ip))
#define set__AUD_SPDIF_IT_EN_CLR(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_IT_EN_CLR(ip))

/* UNF */

#define shift__AUD_SPDIF_IT_EN_CLR__UNF(ip) 0
#define mask__AUD_SPDIF_IT_EN_CLR__UNF(ip) 0x1
#define get__AUD_SPDIF_IT_EN_CLR__UNF(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN_CLR(ip)) >> \
	shift__AUD_SPDIF_IT_EN_CLR__UNF(ip)) & \
	mask__AUD_SPDIF_IT_EN_CLR__UNF(ip))
#define set__AUD_SPDIF_IT_EN_CLR__UNF(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN_CLR(ip)) & \
	~(mask__AUD_SPDIF_IT_EN_CLR__UNF(ip) << \
	shift__AUD_SPDIF_IT_EN_CLR__UNF(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN_CLR__UNF(ip)) << \
	shift__AUD_SPDIF_IT_EN_CLR__UNF(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN_CLR(ip))

#define value__AUD_SPDIF_IT_EN_CLR__UNF__CLEAR(ip) 0x1
#define mask__AUD_SPDIF_IT_EN_CLR__UNF__CLEAR(ip) \
	(value__AUD_SPDIF_IT_EN_CLR__UNF__CLEAR(ip) << \
	shift__AUD_SPDIF_IT_EN_CLR__UNF(ip))
#define set__AUD_SPDIF_IT_EN_CLR__UNF__CLEAR(ip) \
	set__AUD_SPDIF_IT_EN_CLR__UNF(ip, \
	value__AUD_SPDIF_IT_EN_CLR__UNF__CLEAR(ip))

/* EOBURST */

#define shift__AUD_SPDIF_IT_EN_CLR__EOBURST(ip) 1
#define mask__AUD_SPDIF_IT_EN_CLR__EOBURST(ip) 0x1
#define get__AUD_SPDIF_IT_EN_CLR__EOBURST(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN_CLR(ip)) >> \
	shift__AUD_SPDIF_IT_EN_CLR__EOBURST(ip)) & \
	mask__AUD_SPDIF_IT_EN_CLR__EOBURST(ip))
#define set__AUD_SPDIF_IT_EN_CLR__EOBURST(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN_CLR(ip)) & \
	~(mask__AUD_SPDIF_IT_EN_CLR__EOBURST(ip) << \
	shift__AUD_SPDIF_IT_EN_CLR__EOBURST(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN_CLR__EOBURST(ip)) << \
	shift__AUD_SPDIF_IT_EN_CLR__EOBURST(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN_CLR(ip))

#define value__AUD_SPDIF_IT_EN_CLR__EOBURST__CLEAR(ip) 0x1
#define mask__AUD_SPDIF_IT_EN_CLR__EOBURST__CLEAR(ip) \
	(value__AUD_SPDIF_IT_EN_CLR__EOBURST__CLEAR(ip) << \
	shift__AUD_SPDIF_IT_EN_CLR__EOBURST(ip))
#define set__AUD_SPDIF_IT_EN_CLR__EOBURST__CLEAR(ip) \
	set__AUD_SPDIF_IT_EN_CLR__EOBURST(ip, \
	value__AUD_SPDIF_IT_EN_CLR__EOBURST__CLEAR(ip))

/* EOBLOCK */

#define shift__AUD_SPDIF_IT_EN_CLR__EOBLOCK(ip) 2
#define mask__AUD_SPDIF_IT_EN_CLR__EOBLOCK(ip) 0x1
#define get__AUD_SPDIF_IT_EN_CLR__EOBLOCK(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN_CLR(ip)) >> \
	shift__AUD_SPDIF_IT_EN_CLR__EOBLOCK(ip)) & \
	mask__AUD_SPDIF_IT_EN_CLR__EOBLOCK(ip))
#define set__AUD_SPDIF_IT_EN_CLR__EOBLOCK(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN_CLR(ip)) & \
	~(mask__AUD_SPDIF_IT_EN_CLR__EOBLOCK(ip) << \
	shift__AUD_SPDIF_IT_EN_CLR__EOBLOCK(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN_CLR__EOBLOCK(ip)) << \
	shift__AUD_SPDIF_IT_EN_CLR__EOBLOCK(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN_CLR(ip))

#define value__AUD_SPDIF_IT_EN_CLR__EOBLOCK__CLEAR(ip) 0x1
#define mask__AUD_SPDIF_IT_EN_CLR__EOBLOCK__CLEAR(ip) \
	(value__AUD_SPDIF_IT_EN_CLR__EOBLOCK__CLEAR(ip) << \
	shift__AUD_SPDIF_IT_EN_CLR__EOBLOCK(ip))
#define set__AUD_SPDIF_IT_EN_CLR__EOBLOCK__CLEAR(ip) \
	set__AUD_SPDIF_IT_EN_CLR__EOBLOCK(ip, \
	value__AUD_SPDIF_IT_EN_CLR__EOBLOCK__CLEAR(ip))

/* EOLATENCY */

#define shift__AUD_SPDIF_IT_EN_CLR__EOLATENCY(ip) 3
#define mask__AUD_SPDIF_IT_EN_CLR__EOLATENCY(ip) 0x1
#define get__AUD_SPDIF_IT_EN_CLR__EOLATENCY(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN_CLR(ip)) >> \
	shift__AUD_SPDIF_IT_EN_CLR__EOLATENCY(ip)) & \
	mask__AUD_SPDIF_IT_EN_CLR__EOLATENCY(ip))
#define set__AUD_SPDIF_IT_EN_CLR__EOLATENCY(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN_CLR(ip)) & \
	~(mask__AUD_SPDIF_IT_EN_CLR__EOLATENCY(ip) << \
	shift__AUD_SPDIF_IT_EN_CLR__EOLATENCY(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN_CLR__EOLATENCY(ip)) << \
	shift__AUD_SPDIF_IT_EN_CLR__EOLATENCY(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN_CLR(ip))

#define value__AUD_SPDIF_IT_EN_CLR__EOLATENCY__CLEAR(ip) 0x1
#define mask__AUD_SPDIF_IT_EN_CLR__EOLATENCY__CLEAR(ip) \
	(value__AUD_SPDIF_IT_EN_CLR__EOLATENCY__CLEAR(ip) << \
	shift__AUD_SPDIF_IT_EN_CLR__EOLATENCY(ip))
#define set__AUD_SPDIF_IT_EN_CLR__EOLATENCY__CLEAR(ip) \
	set__AUD_SPDIF_IT_EN_CLR__EOLATENCY(ip, \
	value__AUD_SPDIF_IT_EN_CLR__EOLATENCY__CLEAR(ip))

/* EOPD */

#define shift__AUD_SPDIF_IT_EN_CLR__EOPD(ip) 4
#define mask__AUD_SPDIF_IT_EN_CLR__EOPD(ip) 0x1
#define get__AUD_SPDIF_IT_EN_CLR__EOPD(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN_CLR(ip)) >> \
	shift__AUD_SPDIF_IT_EN_CLR__EOPD(ip)) & \
	mask__AUD_SPDIF_IT_EN_CLR__EOPD(ip))
#define set__AUD_SPDIF_IT_EN_CLR__EOPD(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN_CLR(ip)) & \
	~(mask__AUD_SPDIF_IT_EN_CLR__EOPD(ip) << \
	shift__AUD_SPDIF_IT_EN_CLR__EOPD(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN_CLR__EOPD(ip)) << \
	shift__AUD_SPDIF_IT_EN_CLR__EOPD(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN_CLR(ip))

#define value__AUD_SPDIF_IT_EN_CLR__EOPD__CLEAR(ip) 0x1
#define mask__AUD_SPDIF_IT_EN_CLR__EOPD__CLEAR(ip) \
	(value__AUD_SPDIF_IT_EN_CLR__EOPD__CLEAR(ip) << \
	shift__AUD_SPDIF_IT_EN_CLR__EOPD(ip))
#define set__AUD_SPDIF_IT_EN_CLR__EOPD__CLEAR(ip) \
	set__AUD_SPDIF_IT_EN_CLR__EOPD(ip, \
	value__AUD_SPDIF_IT_EN_CLR__EOPD__CLEAR(ip))

/* NSAMPLE */

#define shift__AUD_SPDIF_IT_EN_CLR__NSAMPLE(ip) 5
#define mask__AUD_SPDIF_IT_EN_CLR__NSAMPLE(ip) 0x1
#define get__AUD_SPDIF_IT_EN_CLR__NSAMPLE(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_IT_EN_CLR(ip)) >> \
	shift__AUD_SPDIF_IT_EN_CLR__NSAMPLE(ip)) & \
	mask__AUD_SPDIF_IT_EN_CLR__NSAMPLE(ip))
#define set__AUD_SPDIF_IT_EN_CLR__NSAMPLE(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_IT_EN_CLR(ip)) & \
	~(mask__AUD_SPDIF_IT_EN_CLR__NSAMPLE(ip) << \
	shift__AUD_SPDIF_IT_EN_CLR__NSAMPLE(ip))) | (((value) & \
	mask__AUD_SPDIF_IT_EN_CLR__NSAMPLE(ip)) << \
	shift__AUD_SPDIF_IT_EN_CLR__NSAMPLE(ip)), ip->base + \
	offset__AUD_SPDIF_IT_EN_CLR(ip))

#define value__AUD_SPDIF_IT_EN_CLR__NSAMPLE__CLEAR(ip) 0x1
#define mask__AUD_SPDIF_IT_EN_CLR__NSAMPLE__CLEAR(ip) \
	(value__AUD_SPDIF_IT_EN_CLR__NSAMPLE__CLEAR(ip) << \
	shift__AUD_SPDIF_IT_EN_CLR__NSAMPLE(ip))
#define set__AUD_SPDIF_IT_EN_CLR__NSAMPLE__CLEAR(ip) \
	set__AUD_SPDIF_IT_EN_CLR__NSAMPLE(ip, \
	value__AUD_SPDIF_IT_EN_CLR__NSAMPLE__CLEAR(ip))



/*
 * AUD_SPDIF_CTRL
 */

#define offset__AUD_SPDIF_CTRL(ip) 0x1c
#define get__AUD_SPDIF_CTRL(ip) readl(ip->base + \
	offset__AUD_SPDIF_CTRL(ip))
#define set__AUD_SPDIF_CTRL(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_CTRL(ip))

/* MODE */

#define shift__AUD_SPDIF_CTRL__MODE(ip) 0
#define mask__AUD_SPDIF_CTRL__MODE(ip) 0x7
#define get__AUD_SPDIF_CTRL__MODE(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CTRL(ip)) >> shift__AUD_SPDIF_CTRL__MODE(ip)) & \
	mask__AUD_SPDIF_CTRL__MODE(ip))
#define set__AUD_SPDIF_CTRL__MODE(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_CTRL(ip)) & ~(mask__AUD_SPDIF_CTRL__MODE(ip) << \
	shift__AUD_SPDIF_CTRL__MODE(ip))) | (((value) & \
	mask__AUD_SPDIF_CTRL__MODE(ip)) << shift__AUD_SPDIF_CTRL__MODE(ip)), \
	ip->base + offset__AUD_SPDIF_CTRL(ip))

#define value__AUD_SPDIF_CTRL__MODE__OFF(ip) 0x0
#define mask__AUD_SPDIF_CTRL__MODE__OFF(ip) \
	(value__AUD_SPDIF_CTRL__MODE__OFF(ip) << \
	shift__AUD_SPDIF_CTRL__MODE(ip))
#define set__AUD_SPDIF_CTRL__MODE__OFF(ip) \
	set__AUD_SPDIF_CTRL__MODE(ip, value__AUD_SPDIF_CTRL__MODE__OFF(ip))

#define value__AUD_SPDIF_CTRL__MODE__MUTE_PCM_NULL(ip) 0x1
#define mask__AUD_SPDIF_CTRL__MODE__MUTE_PCM_NULL(ip) \
	(value__AUD_SPDIF_CTRL__MODE__MUTE_PCM_NULL(ip) << \
	shift__AUD_SPDIF_CTRL__MODE(ip))
#define set__AUD_SPDIF_CTRL__MODE__MUTE_PCM_NULL(ip) \
	set__AUD_SPDIF_CTRL__MODE(ip, \
	value__AUD_SPDIF_CTRL__MODE__MUTE_PCM_NULL(ip))

#define value__AUD_SPDIF_CTRL__MODE__MUTE_PAUSE_BURSTS(ip) 0x2
#define mask__AUD_SPDIF_CTRL__MODE__MUTE_PAUSE_BURSTS(ip) \
	(value__AUD_SPDIF_CTRL__MODE__MUTE_PAUSE_BURSTS(ip) << \
	shift__AUD_SPDIF_CTRL__MODE(ip))
#define set__AUD_SPDIF_CTRL__MODE__MUTE_PAUSE_BURSTS(ip) \
	set__AUD_SPDIF_CTRL__MODE(ip, \
	value__AUD_SPDIF_CTRL__MODE__MUTE_PAUSE_BURSTS(ip))

#define value__AUD_SPDIF_CTRL__MODE__PCM(ip) 0x3
#define mask__AUD_SPDIF_CTRL__MODE__PCM(ip) \
	(value__AUD_SPDIF_CTRL__MODE__PCM(ip) << \
	shift__AUD_SPDIF_CTRL__MODE(ip))
#define set__AUD_SPDIF_CTRL__MODE__PCM(ip) \
	set__AUD_SPDIF_CTRL__MODE(ip, value__AUD_SPDIF_CTRL__MODE__PCM(ip))

#define value__AUD_SPDIF_CTRL__MODE__ENCODED(ip) 0x4
#define mask__AUD_SPDIF_CTRL__MODE__ENCODED(ip) \
	(value__AUD_SPDIF_CTRL__MODE__ENCODED(ip) << \
	shift__AUD_SPDIF_CTRL__MODE(ip))
#define set__AUD_SPDIF_CTRL__MODE__ENCODED(ip) \
	set__AUD_SPDIF_CTRL__MODE(ip, \
	value__AUD_SPDIF_CTRL__MODE__ENCODED(ip))

/* IDLE */

#define shift__AUD_SPDIF_CTRL__IDLE(ip) 3
#define mask__AUD_SPDIF_CTRL__IDLE(ip) 0x1
#define get__AUD_SPDIF_CTRL__IDLE(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CTRL(ip)) >> shift__AUD_SPDIF_CTRL__IDLE(ip)) & \
	mask__AUD_SPDIF_CTRL__IDLE(ip))
#define set__AUD_SPDIF_CTRL__IDLE(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_CTRL(ip)) & ~(mask__AUD_SPDIF_CTRL__IDLE(ip) << \
	shift__AUD_SPDIF_CTRL__IDLE(ip))) | (((value) & \
	mask__AUD_SPDIF_CTRL__IDLE(ip)) << shift__AUD_SPDIF_CTRL__IDLE(ip)), \
	ip->base + offset__AUD_SPDIF_CTRL(ip))

#define value__AUD_SPDIF_CTRL__IDLE__NORMAL(ip) 0x0
#define mask__AUD_SPDIF_CTRL__IDLE__NORMAL(ip) \
	(value__AUD_SPDIF_CTRL__IDLE__NORMAL(ip) << \
	shift__AUD_SPDIF_CTRL__IDLE(ip))
#define set__AUD_SPDIF_CTRL__IDLE__NORMAL(ip) \
	set__AUD_SPDIF_CTRL__IDLE(ip, value__AUD_SPDIF_CTRL__IDLE__NORMAL(ip))

#define value__AUD_SPDIF_CTRL__IDLE__IDLE(ip) 0x1
#define mask__AUD_SPDIF_CTRL__IDLE__IDLE(ip) \
	(value__AUD_SPDIF_CTRL__IDLE__IDLE(ip) << \
	shift__AUD_SPDIF_CTRL__IDLE(ip))
#define set__AUD_SPDIF_CTRL__IDLE__IDLE(ip) \
	set__AUD_SPDIF_CTRL__IDLE(ip, value__AUD_SPDIF_CTRL__IDLE__IDLE(ip))

/* RND */

#define shift__AUD_SPDIF_CTRL__RND(ip) 4
#define mask__AUD_SPDIF_CTRL__RND(ip) 0x1
#define get__AUD_SPDIF_CTRL__RND(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CTRL(ip)) >> shift__AUD_SPDIF_CTRL__RND(ip)) & \
	mask__AUD_SPDIF_CTRL__RND(ip))
#define set__AUD_SPDIF_CTRL__RND(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_CTRL(ip)) & ~(mask__AUD_SPDIF_CTRL__RND(ip) << \
	shift__AUD_SPDIF_CTRL__RND(ip))) | (((value) & \
	mask__AUD_SPDIF_CTRL__RND(ip)) << shift__AUD_SPDIF_CTRL__RND(ip)), \
	ip->base + offset__AUD_SPDIF_CTRL(ip))

#define value__AUD_SPDIF_CTRL__RND__NO_ROUNDING(ip) 0x0
#define mask__AUD_SPDIF_CTRL__RND__NO_ROUNDING(ip) \
	(value__AUD_SPDIF_CTRL__RND__NO_ROUNDING(ip) << \
	shift__AUD_SPDIF_CTRL__RND(ip))
#define set__AUD_SPDIF_CTRL__RND__NO_ROUNDING(ip) \
	set__AUD_SPDIF_CTRL__RND(ip, \
	value__AUD_SPDIF_CTRL__RND__NO_ROUNDING(ip))

#define value__AUD_SPDIF_CTRL__RND__16_BITS_ROUNDING(ip) 0x1
#define mask__AUD_SPDIF_CTRL__RND__16_BITS_ROUNDING(ip) \
	(value__AUD_SPDIF_CTRL__RND__16_BITS_ROUNDING(ip) << \
	shift__AUD_SPDIF_CTRL__RND(ip))
#define set__AUD_SPDIF_CTRL__RND__16_BITS_ROUNDING(ip) \
	set__AUD_SPDIF_CTRL__RND(ip, \
	value__AUD_SPDIF_CTRL__RND__16_BITS_ROUNDING(ip))

/* CLK_DIV */

#define shift__AUD_SPDIF_CTRL__CLK_DIV(ip) 5
#define mask__AUD_SPDIF_CTRL__CLK_DIV(ip) 0xff
#define get__AUD_SPDIF_CTRL__CLK_DIV(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CTRL(ip)) >> shift__AUD_SPDIF_CTRL__CLK_DIV(ip)) & \
	mask__AUD_SPDIF_CTRL__CLK_DIV(ip))
#define set__AUD_SPDIF_CTRL__CLK_DIV(ip, value) writel((readl(ip->base \
	+ offset__AUD_SPDIF_CTRL(ip)) & ~(mask__AUD_SPDIF_CTRL__CLK_DIV(ip) << \
	shift__AUD_SPDIF_CTRL__CLK_DIV(ip))) | (((value) & \
	mask__AUD_SPDIF_CTRL__CLK_DIV(ip)) << \
	shift__AUD_SPDIF_CTRL__CLK_DIV(ip)), ip->base + \
	offset__AUD_SPDIF_CTRL(ip))

/* STUFFING */

#define shift__AUD_SPDIF_CTRL__STUFFING(ip) 14
#define mask__AUD_SPDIF_CTRL__STUFFING(ip) 0x1
#define get__AUD_SPDIF_CTRL__STUFFING(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CTRL(ip)) >> shift__AUD_SPDIF_CTRL__STUFFING(ip)) & \
	mask__AUD_SPDIF_CTRL__STUFFING(ip))
#define set__AUD_SPDIF_CTRL__STUFFING(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_CTRL(ip)) & \
	~(mask__AUD_SPDIF_CTRL__STUFFING(ip) << \
	shift__AUD_SPDIF_CTRL__STUFFING(ip))) | (((value) & \
	mask__AUD_SPDIF_CTRL__STUFFING(ip)) << \
	shift__AUD_SPDIF_CTRL__STUFFING(ip)), ip->base + \
	offset__AUD_SPDIF_CTRL(ip))

#define value__AUD_SPDIF_CTRL__STUFFING__SOFTWARE(ip) 0x0
#define mask__AUD_SPDIF_CTRL__STUFFING__SOFTWARE(ip) \
	(value__AUD_SPDIF_CTRL__STUFFING__SOFTWARE(ip) << \
	shift__AUD_SPDIF_CTRL__STUFFING(ip))
#define set__AUD_SPDIF_CTRL__STUFFING__SOFTWARE(ip) \
	set__AUD_SPDIF_CTRL__STUFFING(ip, \
	value__AUD_SPDIF_CTRL__STUFFING__SOFTWARE(ip))

#define value__AUD_SPDIF_CTRL__STUFFING__HARDWARE(ip) 0x1
#define mask__AUD_SPDIF_CTRL__STUFFING__HARDWARE(ip) \
	(value__AUD_SPDIF_CTRL__STUFFING__HARDWARE(ip) << \
	shift__AUD_SPDIF_CTRL__STUFFING(ip))
#define set__AUD_SPDIF_CTRL__STUFFING__HARDWARE(ip) \
	set__AUD_SPDIF_CTRL__STUFFING(ip, \
	value__AUD_SPDIF_CTRL__STUFFING__HARDWARE(ip))

/* MEMREAD */

#define shift__AUD_SPDIF_CTRL__MEMREAD(ip) 15
#define mask__AUD_SPDIF_CTRL__MEMREAD(ip) 0x1ffff
#define get__AUD_SPDIF_CTRL__MEMREAD(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CTRL(ip)) >> shift__AUD_SPDIF_CTRL__MEMREAD(ip)) & \
	mask__AUD_SPDIF_CTRL__MEMREAD(ip))
#define set__AUD_SPDIF_CTRL__MEMREAD(ip, value) writel((readl(ip->base \
	+ offset__AUD_SPDIF_CTRL(ip)) & ~(mask__AUD_SPDIF_CTRL__MEMREAD(ip) << \
	shift__AUD_SPDIF_CTRL__MEMREAD(ip))) | (((value) & \
	mask__AUD_SPDIF_CTRL__MEMREAD(ip)) << \
	shift__AUD_SPDIF_CTRL__MEMREAD(ip)), ip->base + \
	offset__AUD_SPDIF_CTRL(ip))



/*
 * AUD_SPDIF_STA
 */

#define offset__AUD_SPDIF_STA(ip) 0x20
#define get__AUD_SPDIF_STA(ip) readl(ip->base + \
	offset__AUD_SPDIF_STA(ip))
#define set__AUD_SPDIF_STA(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_STA(ip))

/* RUN_STOP */

#define shift__AUD_SPDIF_STA__RUN_STOP(ip) 0
#define mask__AUD_SPDIF_STA__RUN_STOP(ip) 0x1
#define get__AUD_SPDIF_STA__RUN_STOP(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_STA(ip)) >> shift__AUD_SPDIF_STA__RUN_STOP(ip)) & \
	mask__AUD_SPDIF_STA__RUN_STOP(ip))
#define set__AUD_SPDIF_STA__RUN_STOP(ip, value) writel((readl(ip->base \
	+ offset__AUD_SPDIF_STA(ip)) & ~(mask__AUD_SPDIF_STA__RUN_STOP(ip) << \
	shift__AUD_SPDIF_STA__RUN_STOP(ip))) | (((value) & \
	mask__AUD_SPDIF_STA__RUN_STOP(ip)) << \
	shift__AUD_SPDIF_STA__RUN_STOP(ip)), ip->base + \
	offset__AUD_SPDIF_STA(ip))

#define value__AUD_SPDIF_STA__RUN_STOP__STOPPED(ip) 0x0
#define mask__AUD_SPDIF_STA__RUN_STOP__STOPPED(ip) \
	(value__AUD_SPDIF_STA__RUN_STOP__STOPPED(ip) << \
	shift__AUD_SPDIF_STA__RUN_STOP(ip))
#define set__AUD_SPDIF_STA__RUN_STOP__STOPPED(ip) \
	set__AUD_SPDIF_STA__RUN_STOP(ip, \
	value__AUD_SPDIF_STA__RUN_STOP__STOPPED(ip))

#define value__AUD_SPDIF_STA__RUN_STOP__RUNNING(ip) 0x1
#define mask__AUD_SPDIF_STA__RUN_STOP__RUNNING(ip) \
	(value__AUD_SPDIF_STA__RUN_STOP__RUNNING(ip) << \
	shift__AUD_SPDIF_STA__RUN_STOP(ip))
#define set__AUD_SPDIF_STA__RUN_STOP__RUNNING(ip) \
	set__AUD_SPDIF_STA__RUN_STOP(ip, \
	value__AUD_SPDIF_STA__RUN_STOP__RUNNING(ip))

/* UNF */

#define shift__AUD_SPDIF_STA__UNF(ip) 1
#define mask__AUD_SPDIF_STA__UNF(ip) 0x1
#define get__AUD_SPDIF_STA__UNF(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_STA(ip)) >> shift__AUD_SPDIF_STA__UNF(ip)) & \
	mask__AUD_SPDIF_STA__UNF(ip))
#define set__AUD_SPDIF_STA__UNF(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_STA(ip)) & ~(mask__AUD_SPDIF_STA__UNF(ip) << \
	shift__AUD_SPDIF_STA__UNF(ip))) | (((value) & \
	mask__AUD_SPDIF_STA__UNF(ip)) << shift__AUD_SPDIF_STA__UNF(ip)), \
	ip->base + offset__AUD_SPDIF_STA(ip))

#define value__AUD_SPDIF_STA__UNF__DETECTED(ip) 0x1
#define mask__AUD_SPDIF_STA__UNF__DETECTED(ip) \
	(value__AUD_SPDIF_STA__UNF__DETECTED(ip) << \
	shift__AUD_SPDIF_STA__UNF(ip))
#define set__AUD_SPDIF_STA__UNF__DETECTED(ip) \
	set__AUD_SPDIF_STA__UNF(ip, value__AUD_SPDIF_STA__UNF__DETECTED(ip))

/* EOBURST */

#define shift__AUD_SPDIF_STA__EOBURST(ip) 2
#define mask__AUD_SPDIF_STA__EOBURST(ip) 0x1
#define get__AUD_SPDIF_STA__EOBURST(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_STA(ip)) >> shift__AUD_SPDIF_STA__EOBURST(ip)) & \
	mask__AUD_SPDIF_STA__EOBURST(ip))
#define set__AUD_SPDIF_STA__EOBURST(ip, value) writel((readl(ip->base \
	+ offset__AUD_SPDIF_STA(ip)) & ~(mask__AUD_SPDIF_STA__EOBURST(ip) << \
	shift__AUD_SPDIF_STA__EOBURST(ip))) | (((value) & \
	mask__AUD_SPDIF_STA__EOBURST(ip)) << \
	shift__AUD_SPDIF_STA__EOBURST(ip)), ip->base + \
	offset__AUD_SPDIF_STA(ip))

#define value__AUD_SPDIF_STA__EOBURST__END(ip) 0x1
#define mask__AUD_SPDIF_STA__EOBURST__END(ip) \
	(value__AUD_SPDIF_STA__EOBURST__END(ip) << \
	shift__AUD_SPDIF_STA__EOBURST(ip))
#define set__AUD_SPDIF_STA__EOBURST__END(ip) \
	set__AUD_SPDIF_STA__EOBURST(ip, \
	value__AUD_SPDIF_STA__EOBURST__END(ip))

/* EOBLOCK */

#define shift__AUD_SPDIF_STA__EOBLOCK(ip) 3
#define mask__AUD_SPDIF_STA__EOBLOCK(ip) 0x1
#define get__AUD_SPDIF_STA__EOBLOCK(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_STA(ip)) >> shift__AUD_SPDIF_STA__EOBLOCK(ip)) & \
	mask__AUD_SPDIF_STA__EOBLOCK(ip))
#define set__AUD_SPDIF_STA__EOBLOCK(ip, value) writel((readl(ip->base \
	+ offset__AUD_SPDIF_STA(ip)) & ~(mask__AUD_SPDIF_STA__EOBLOCK(ip) << \
	shift__AUD_SPDIF_STA__EOBLOCK(ip))) | (((value) & \
	mask__AUD_SPDIF_STA__EOBLOCK(ip)) << \
	shift__AUD_SPDIF_STA__EOBLOCK(ip)), ip->base + \
	offset__AUD_SPDIF_STA(ip))

#define value__AUD_SPDIF_STA__EOBLOCK__END(ip) 0x1
#define mask__AUD_SPDIF_STA__EOBLOCK__END(ip) \
	(value__AUD_SPDIF_STA__EOBLOCK__END(ip) << \
	shift__AUD_SPDIF_STA__EOBLOCK(ip))
#define set__AUD_SPDIF_STA__EOBLOCK__END(ip) \
	set__AUD_SPDIF_STA__EOBLOCK(ip, \
	value__AUD_SPDIF_STA__EOBLOCK__END(ip))

/* EOLATENCY */

#define shift__AUD_SPDIF_STA__EOLATENCY(ip) 4
#define mask__AUD_SPDIF_STA__EOLATENCY(ip) 0x1
#define get__AUD_SPDIF_STA__EOLATENCY(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_STA(ip)) >> shift__AUD_SPDIF_STA__EOLATENCY(ip)) & \
	mask__AUD_SPDIF_STA__EOLATENCY(ip))
#define set__AUD_SPDIF_STA__EOLATENCY(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_STA(ip)) & \
	~(mask__AUD_SPDIF_STA__EOLATENCY(ip) << \
	shift__AUD_SPDIF_STA__EOLATENCY(ip))) | (((value) & \
	mask__AUD_SPDIF_STA__EOLATENCY(ip)) << \
	shift__AUD_SPDIF_STA__EOLATENCY(ip)), ip->base + \
	offset__AUD_SPDIF_STA(ip))

#define value__AUD_SPDIF_STA__EOLATENCY__END_OF_LATENCY_COUNTER(ip) \
	0x1
#define mask__AUD_SPDIF_STA__EOLATENCY__END_OF_LATENCY_COUNTER(ip) \
	(value__AUD_SPDIF_STA__EOLATENCY__END_OF_LATENCY_COUNTER(ip) << \
	shift__AUD_SPDIF_STA__EOLATENCY(ip))
#define set__AUD_SPDIF_STA__EOLATENCY__END_OF_LATENCY_COUNTER(ip) \
	set__AUD_SPDIF_STA__EOLATENCY(ip, \
	value__AUD_SPDIF_STA__EOLATENCY__END_OF_LATENCY_COUNTER(ip))

/* PDDATA */

#define shift__AUD_SPDIF_STA__PDDATA(ip) 5
#define mask__AUD_SPDIF_STA__PDDATA(ip) 0x1
#define get__AUD_SPDIF_STA__PDDATA(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_STA(ip)) >> shift__AUD_SPDIF_STA__PDDATA(ip)) & \
	mask__AUD_SPDIF_STA__PDDATA(ip))
#define set__AUD_SPDIF_STA__PDDATA(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_STA(ip)) & ~(mask__AUD_SPDIF_STA__PDDATA(ip) << \
	shift__AUD_SPDIF_STA__PDDATA(ip))) | (((value) & \
	mask__AUD_SPDIF_STA__PDDATA(ip)) << shift__AUD_SPDIF_STA__PDDATA(ip)), \
	ip->base + offset__AUD_SPDIF_STA(ip))

#define value__AUD_SPDIF_STA__PDDATA__SENT(ip) 0x1
#define mask__AUD_SPDIF_STA__PDDATA__SENT(ip) \
	(value__AUD_SPDIF_STA__PDDATA__SENT(ip) << \
	shift__AUD_SPDIF_STA__PDDATA(ip))
#define set__AUD_SPDIF_STA__PDDATA__SENT(ip) \
	set__AUD_SPDIF_STA__PDDATA(ip, value__AUD_SPDIF_STA__PDDATA__SENT(ip))

/* NSAMPLE */

#define shift__AUD_SPDIF_STA__NSAMPLE(ip) 6
#define mask__AUD_SPDIF_STA__NSAMPLE(ip) 0x1
#define get__AUD_SPDIF_STA__NSAMPLE(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_STA(ip)) >> shift__AUD_SPDIF_STA__NSAMPLE(ip)) & \
	mask__AUD_SPDIF_STA__NSAMPLE(ip))
#define set__AUD_SPDIF_STA__NSAMPLE(ip, value) writel((readl(ip->base \
	+ offset__AUD_SPDIF_STA(ip)) & ~(mask__AUD_SPDIF_STA__NSAMPLE(ip) << \
	shift__AUD_SPDIF_STA__NSAMPLE(ip))) | (((value) & \
	mask__AUD_SPDIF_STA__NSAMPLE(ip)) << \
	shift__AUD_SPDIF_STA__NSAMPLE(ip)), ip->base + \
	offset__AUD_SPDIF_STA(ip))

#define value__AUD_SPDIF_STA__NSAMPLE__DONE(ip) 0x1
#define mask__AUD_SPDIF_STA__NSAMPLE__DONE(ip) \
	(value__AUD_SPDIF_STA__NSAMPLE__DONE(ip) << \
	shift__AUD_SPDIF_STA__NSAMPLE(ip))
#define set__AUD_SPDIF_STA__NSAMPLE__DONE(ip) \
	set__AUD_SPDIF_STA__NSAMPLE(ip, \
	value__AUD_SPDIF_STA__NSAMPLE__DONE(ip))

/* PABIT */

#define shift__AUD_SPDIF_STA__PABIT(ip) 7
#define mask__AUD_SPDIF_STA__PABIT(ip) 0xff
#define get__AUD_SPDIF_STA__PABIT(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_STA(ip)) >> shift__AUD_SPDIF_STA__PABIT(ip)) & \
	mask__AUD_SPDIF_STA__PABIT(ip))
#define set__AUD_SPDIF_STA__PABIT(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_STA(ip)) & ~(mask__AUD_SPDIF_STA__PABIT(ip) << \
	shift__AUD_SPDIF_STA__PABIT(ip))) | (((value) & \
	mask__AUD_SPDIF_STA__PABIT(ip)) << shift__AUD_SPDIF_STA__PABIT(ip)), \
	ip->base + offset__AUD_SPDIF_STA(ip))

/* PDPAUSE */

#define shift__AUD_SPDIF_STA__PDPAUSE(ip) 15
#define mask__AUD_SPDIF_STA__PDPAUSE(ip) 0x1
#define get__AUD_SPDIF_STA__PDPAUSE(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_STA(ip)) >> shift__AUD_SPDIF_STA__PDPAUSE(ip)) & \
	mask__AUD_SPDIF_STA__PDPAUSE(ip))
#define set__AUD_SPDIF_STA__PDPAUSE(ip, value) writel((readl(ip->base \
	+ offset__AUD_SPDIF_STA(ip)) & ~(mask__AUD_SPDIF_STA__PDPAUSE(ip) << \
	shift__AUD_SPDIF_STA__PDPAUSE(ip))) | (((value) & \
	mask__AUD_SPDIF_STA__PDPAUSE(ip)) << \
	shift__AUD_SPDIF_STA__PDPAUSE(ip)), ip->base + \
	offset__AUD_SPDIF_STA(ip))

#define value__AUD_SPDIF_STA__PDPAUSE__SENT(ip) 0x1
#define mask__AUD_SPDIF_STA__PDPAUSE__SENT(ip) \
	(value__AUD_SPDIF_STA__PDPAUSE__SENT(ip) << \
	shift__AUD_SPDIF_STA__PDPAUSE(ip))
#define set__AUD_SPDIF_STA__PDPAUSE__SENT(ip) \
	set__AUD_SPDIF_STA__PDPAUSE(ip, \
	value__AUD_SPDIF_STA__PDPAUSE__SENT(ip))

/* SAMPLES_IN_FIFO */

#define shift__AUD_SPDIF_STA__SAMPLES_IN_FIFO(ip) (ip->ver < \
	4 ? -1 : 16)
#define mask__AUD_SPDIF_STA__SAMPLES_IN_FIFO(ip) (ip->ver < \
	4 ? -1 : 0x1f)
#define get__AUD_SPDIF_STA__SAMPLES_IN_FIFO(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_STA(ip)) >> \
	shift__AUD_SPDIF_STA__SAMPLES_IN_FIFO(ip)) & \
	mask__AUD_SPDIF_STA__SAMPLES_IN_FIFO(ip))
#define set__AUD_SPDIF_STA__SAMPLES_IN_FIFO(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_STA(ip)) & \
	~(mask__AUD_SPDIF_STA__SAMPLES_IN_FIFO(ip) << \
	shift__AUD_SPDIF_STA__SAMPLES_IN_FIFO(ip))) | (((value) & \
	mask__AUD_SPDIF_STA__SAMPLES_IN_FIFO(ip)) << \
	shift__AUD_SPDIF_STA__SAMPLES_IN_FIFO(ip)), ip->base + \
	offset__AUD_SPDIF_STA(ip))



/*
 * AUD_SPDIF_PA_PB
 */

#define offset__AUD_SPDIF_PA_PB(ip) 0x24
#define get__AUD_SPDIF_PA_PB(ip) readl(ip->base + \
	offset__AUD_SPDIF_PA_PB(ip))
#define set__AUD_SPDIF_PA_PB(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_PA_PB(ip))

/* PB */

#define shift__AUD_SPDIF_PA_PB__PB(ip) 0
#define mask__AUD_SPDIF_PA_PB__PB(ip) 0xffff
#define get__AUD_SPDIF_PA_PB__PB(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_PA_PB(ip)) >> shift__AUD_SPDIF_PA_PB__PB(ip)) & \
	mask__AUD_SPDIF_PA_PB__PB(ip))
#define set__AUD_SPDIF_PA_PB__PB(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_PA_PB(ip)) & ~(mask__AUD_SPDIF_PA_PB__PB(ip) << \
	shift__AUD_SPDIF_PA_PB__PB(ip))) | (((value) & \
	mask__AUD_SPDIF_PA_PB__PB(ip)) << shift__AUD_SPDIF_PA_PB__PB(ip)), \
	ip->base + offset__AUD_SPDIF_PA_PB(ip))

/* PA */

#define shift__AUD_SPDIF_PA_PB__PA(ip) 16
#define mask__AUD_SPDIF_PA_PB__PA(ip) 0xffff
#define get__AUD_SPDIF_PA_PB__PA(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_PA_PB(ip)) >> shift__AUD_SPDIF_PA_PB__PA(ip)) & \
	mask__AUD_SPDIF_PA_PB__PA(ip))
#define set__AUD_SPDIF_PA_PB__PA(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_PA_PB(ip)) & ~(mask__AUD_SPDIF_PA_PB__PA(ip) << \
	shift__AUD_SPDIF_PA_PB__PA(ip))) | (((value) & \
	mask__AUD_SPDIF_PA_PB__PA(ip)) << shift__AUD_SPDIF_PA_PB__PA(ip)), \
	ip->base + offset__AUD_SPDIF_PA_PB(ip))



/*
 * AUD_SPDIF_PC_PD
 */

#define offset__AUD_SPDIF_PC_PD(ip) 0x28
#define get__AUD_SPDIF_PC_PD(ip) readl(ip->base + \
	offset__AUD_SPDIF_PC_PD(ip))
#define set__AUD_SPDIF_PC_PD(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_PC_PD(ip))

/* PD */

#define shift__AUD_SPDIF_PC_PD__PD(ip) 0
#define mask__AUD_SPDIF_PC_PD__PD(ip) 0xffff
#define get__AUD_SPDIF_PC_PD__PD(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_PC_PD(ip)) >> shift__AUD_SPDIF_PC_PD__PD(ip)) & \
	mask__AUD_SPDIF_PC_PD__PD(ip))
#define set__AUD_SPDIF_PC_PD__PD(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_PC_PD(ip)) & ~(mask__AUD_SPDIF_PC_PD__PD(ip) << \
	shift__AUD_SPDIF_PC_PD__PD(ip))) | (((value) & \
	mask__AUD_SPDIF_PC_PD__PD(ip)) << shift__AUD_SPDIF_PC_PD__PD(ip)), \
	ip->base + offset__AUD_SPDIF_PC_PD(ip))

/* PC */

#define shift__AUD_SPDIF_PC_PD__PC(ip) 16
#define mask__AUD_SPDIF_PC_PD__PC(ip) 0xffff
#define get__AUD_SPDIF_PC_PD__PC(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_PC_PD(ip)) >> shift__AUD_SPDIF_PC_PD__PC(ip)) & \
	mask__AUD_SPDIF_PC_PD__PC(ip))
#define set__AUD_SPDIF_PC_PD__PC(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_PC_PD(ip)) & ~(mask__AUD_SPDIF_PC_PD__PC(ip) << \
	shift__AUD_SPDIF_PC_PD__PC(ip))) | (((value) & \
	mask__AUD_SPDIF_PC_PD__PC(ip)) << shift__AUD_SPDIF_PC_PD__PC(ip)), \
	ip->base + offset__AUD_SPDIF_PC_PD(ip))



/*
 * AUD_SPDIF_CL1
 */

#define offset__AUD_SPDIF_CL1(ip) 0x2c
#define get__AUD_SPDIF_CL1(ip) readl(ip->base + \
	offset__AUD_SPDIF_CL1(ip))
#define set__AUD_SPDIF_CL1(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_CL1(ip))

/* CL1 */

#define shift__AUD_SPDIF_CL1__CL1(ip) 0
#define mask__AUD_SPDIF_CL1__CL1(ip) 0xffffffff
#define get__AUD_SPDIF_CL1__CL1(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CL1(ip)) >> shift__AUD_SPDIF_CL1__CL1(ip)) & \
	mask__AUD_SPDIF_CL1__CL1(ip))
#define set__AUD_SPDIF_CL1__CL1(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_CL1(ip)) & ~(mask__AUD_SPDIF_CL1__CL1(ip) << \
	shift__AUD_SPDIF_CL1__CL1(ip))) | (((value) & \
	mask__AUD_SPDIF_CL1__CL1(ip)) << shift__AUD_SPDIF_CL1__CL1(ip)), \
	ip->base + offset__AUD_SPDIF_CL1(ip))



/*
 * AUD_SPDIF_CR1
 */

#define offset__AUD_SPDIF_CR1(ip) 0x30
#define get__AUD_SPDIF_CR1(ip) readl(ip->base + \
	offset__AUD_SPDIF_CR1(ip))
#define set__AUD_SPDIF_CR1(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_CR1(ip))

/* CR1 */

#define shift__AUD_SPDIF_CR1__CR1(ip) 0
#define mask__AUD_SPDIF_CR1__CR1(ip) 0xffffffff
#define get__AUD_SPDIF_CR1__CR1(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CR1(ip)) >> shift__AUD_SPDIF_CR1__CR1(ip)) & \
	mask__AUD_SPDIF_CR1__CR1(ip))
#define set__AUD_SPDIF_CR1__CR1(ip, value) writel((readl(ip->base + \
	offset__AUD_SPDIF_CR1(ip)) & ~(mask__AUD_SPDIF_CR1__CR1(ip) << \
	shift__AUD_SPDIF_CR1__CR1(ip))) | (((value) & \
	mask__AUD_SPDIF_CR1__CR1(ip)) << shift__AUD_SPDIF_CR1__CR1(ip)), \
	ip->base + offset__AUD_SPDIF_CR1(ip))



/*
 * AUD_SPDIF_CL2_CR2_UV
 */

#define offset__AUD_SPDIF_CL2_CR2_UV(ip) 0x34
#define get__AUD_SPDIF_CL2_CR2_UV(ip) readl(ip->base + \
	offset__AUD_SPDIF_CL2_CR2_UV(ip))
#define set__AUD_SPDIF_CL2_CR2_UV(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_CL2_CR2_UV(ip))

/* CL2 */

#define shift__AUD_SPDIF_CL2_CR2_UV__CL2(ip) 0
#define mask__AUD_SPDIF_CL2_CR2_UV__CL2(ip) 0xf
#define get__AUD_SPDIF_CL2_CR2_UV__CL2(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CL2_CR2_UV(ip)) >> \
	shift__AUD_SPDIF_CL2_CR2_UV__CL2(ip)) & \
	mask__AUD_SPDIF_CL2_CR2_UV__CL2(ip))
#define set__AUD_SPDIF_CL2_CR2_UV__CL2(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_CL2_CR2_UV(ip)) & \
	~(mask__AUD_SPDIF_CL2_CR2_UV__CL2(ip) << \
	shift__AUD_SPDIF_CL2_CR2_UV__CL2(ip))) | (((value) & \
	mask__AUD_SPDIF_CL2_CR2_UV__CL2(ip)) << \
	shift__AUD_SPDIF_CL2_CR2_UV__CL2(ip)), ip->base + \
	offset__AUD_SPDIF_CL2_CR2_UV(ip))

/* CR2 */

#define shift__AUD_SPDIF_CL2_CR2_UV__CR2(ip) 8
#define mask__AUD_SPDIF_CL2_CR2_UV__CR2(ip) 0xf
#define get__AUD_SPDIF_CL2_CR2_UV__CR2(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CL2_CR2_UV(ip)) >> \
	shift__AUD_SPDIF_CL2_CR2_UV__CR2(ip)) & \
	mask__AUD_SPDIF_CL2_CR2_UV__CR2(ip))
#define set__AUD_SPDIF_CL2_CR2_UV__CR2(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_CL2_CR2_UV(ip)) & \
	~(mask__AUD_SPDIF_CL2_CR2_UV__CR2(ip) << \
	shift__AUD_SPDIF_CL2_CR2_UV__CR2(ip))) | (((value) & \
	mask__AUD_SPDIF_CL2_CR2_UV__CR2(ip)) << \
	shift__AUD_SPDIF_CL2_CR2_UV__CR2(ip)), ip->base + \
	offset__AUD_SPDIF_CL2_CR2_UV(ip))

/* LU */

#define shift__AUD_SPDIF_CL2_CR2_UV__LU(ip) 16
#define mask__AUD_SPDIF_CL2_CR2_UV__LU(ip) 0x1
#define get__AUD_SPDIF_CL2_CR2_UV__LU(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CL2_CR2_UV(ip)) >> \
	shift__AUD_SPDIF_CL2_CR2_UV__LU(ip)) & \
	mask__AUD_SPDIF_CL2_CR2_UV__LU(ip))
#define set__AUD_SPDIF_CL2_CR2_UV__LU(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_CL2_CR2_UV(ip)) & \
	~(mask__AUD_SPDIF_CL2_CR2_UV__LU(ip) << \
	shift__AUD_SPDIF_CL2_CR2_UV__LU(ip))) | (((value) & \
	mask__AUD_SPDIF_CL2_CR2_UV__LU(ip)) << \
	shift__AUD_SPDIF_CL2_CR2_UV__LU(ip)), ip->base + \
	offset__AUD_SPDIF_CL2_CR2_UV(ip))

/* RU */

#define shift__AUD_SPDIF_CL2_CR2_UV__RU(ip) 17
#define mask__AUD_SPDIF_CL2_CR2_UV__RU(ip) 0x1
#define get__AUD_SPDIF_CL2_CR2_UV__RU(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CL2_CR2_UV(ip)) >> \
	shift__AUD_SPDIF_CL2_CR2_UV__RU(ip)) & \
	mask__AUD_SPDIF_CL2_CR2_UV__RU(ip))
#define set__AUD_SPDIF_CL2_CR2_UV__RU(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_CL2_CR2_UV(ip)) & \
	~(mask__AUD_SPDIF_CL2_CR2_UV__RU(ip) << \
	shift__AUD_SPDIF_CL2_CR2_UV__RU(ip))) | (((value) & \
	mask__AUD_SPDIF_CL2_CR2_UV__RU(ip)) << \
	shift__AUD_SPDIF_CL2_CR2_UV__RU(ip)), ip->base + \
	offset__AUD_SPDIF_CL2_CR2_UV(ip))

/* LV */

#define shift__AUD_SPDIF_CL2_CR2_UV__LV(ip) 18
#define mask__AUD_SPDIF_CL2_CR2_UV__LV(ip) 0x1
#define get__AUD_SPDIF_CL2_CR2_UV__LV(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CL2_CR2_UV(ip)) >> \
	shift__AUD_SPDIF_CL2_CR2_UV__LV(ip)) & \
	mask__AUD_SPDIF_CL2_CR2_UV__LV(ip))
#define set__AUD_SPDIF_CL2_CR2_UV__LV(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_CL2_CR2_UV(ip)) & \
	~(mask__AUD_SPDIF_CL2_CR2_UV__LV(ip) << \
	shift__AUD_SPDIF_CL2_CR2_UV__LV(ip))) | (((value) & \
	mask__AUD_SPDIF_CL2_CR2_UV__LV(ip)) << \
	shift__AUD_SPDIF_CL2_CR2_UV__LV(ip)), ip->base + \
	offset__AUD_SPDIF_CL2_CR2_UV(ip))

/* RV */

#define shift__AUD_SPDIF_CL2_CR2_UV__RV(ip) 19
#define mask__AUD_SPDIF_CL2_CR2_UV__RV(ip) 0x1
#define get__AUD_SPDIF_CL2_CR2_UV__RV(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CL2_CR2_UV(ip)) >> \
	shift__AUD_SPDIF_CL2_CR2_UV__RV(ip)) & \
	mask__AUD_SPDIF_CL2_CR2_UV__RV(ip))
#define set__AUD_SPDIF_CL2_CR2_UV__RV(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_CL2_CR2_UV(ip)) & \
	~(mask__AUD_SPDIF_CL2_CR2_UV__RV(ip) << \
	shift__AUD_SPDIF_CL2_CR2_UV__RV(ip))) | (((value) & \
	mask__AUD_SPDIF_CL2_CR2_UV__RV(ip)) << \
	shift__AUD_SPDIF_CL2_CR2_UV__RV(ip)), ip->base + \
	offset__AUD_SPDIF_CL2_CR2_UV(ip))



/*
 * AUD_SPDIF_PAU_LAT
 */

#define offset__AUD_SPDIF_PAU_LAT(ip) 0x38
#define get__AUD_SPDIF_PAU_LAT(ip) readl(ip->base + \
	offset__AUD_SPDIF_PAU_LAT(ip))
#define set__AUD_SPDIF_PAU_LAT(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_PAU_LAT(ip))

/* LAT */

#define shift__AUD_SPDIF_PAU_LAT__LAT(ip) 0
#define mask__AUD_SPDIF_PAU_LAT__LAT(ip) 0xffff
#define get__AUD_SPDIF_PAU_LAT__LAT(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_PAU_LAT(ip)) >> shift__AUD_SPDIF_PAU_LAT__LAT(ip)) & \
	mask__AUD_SPDIF_PAU_LAT__LAT(ip))
#define set__AUD_SPDIF_PAU_LAT__LAT(ip, value) writel((readl(ip->base \
	+ offset__AUD_SPDIF_PAU_LAT(ip)) & ~(mask__AUD_SPDIF_PAU_LAT__LAT(ip) \
	<< shift__AUD_SPDIF_PAU_LAT__LAT(ip))) | (((value) & \
	mask__AUD_SPDIF_PAU_LAT__LAT(ip)) << \
	shift__AUD_SPDIF_PAU_LAT__LAT(ip)), ip->base + \
	offset__AUD_SPDIF_PAU_LAT(ip))

/* NPD_BURST */

#define shift__AUD_SPDIF_PAU_LAT__NPD_BURST(ip) 16
#define mask__AUD_SPDIF_PAU_LAT__NPD_BURST(ip) 0xffff
#define get__AUD_SPDIF_PAU_LAT__NPD_BURST(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_PAU_LAT(ip)) >> \
	shift__AUD_SPDIF_PAU_LAT__NPD_BURST(ip)) & \
	mask__AUD_SPDIF_PAU_LAT__NPD_BURST(ip))
#define set__AUD_SPDIF_PAU_LAT__NPD_BURST(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_PAU_LAT(ip)) & \
	~(mask__AUD_SPDIF_PAU_LAT__NPD_BURST(ip) << \
	shift__AUD_SPDIF_PAU_LAT__NPD_BURST(ip))) | (((value) & \
	mask__AUD_SPDIF_PAU_LAT__NPD_BURST(ip)) << \
	shift__AUD_SPDIF_PAU_LAT__NPD_BURST(ip)), ip->base + \
	offset__AUD_SPDIF_PAU_LAT(ip))



/*
 * AUD_SPDIF_BST_FL
 */

#define offset__AUD_SPDIF_BST_FL(ip) 0x3c
#define get__AUD_SPDIF_BST_FL(ip) readl(ip->base + \
	offset__AUD_SPDIF_BST_FL(ip))
#define set__AUD_SPDIF_BST_FL(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_BST_FL(ip))

/* PDBURST */

#define shift__AUD_SPDIF_BST_FL__PDBURST(ip) 0
#define mask__AUD_SPDIF_BST_FL__PDBURST(ip) 0xffff
#define get__AUD_SPDIF_BST_FL__PDBURST(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_BST_FL(ip)) >> shift__AUD_SPDIF_BST_FL__PDBURST(ip)) \
	& mask__AUD_SPDIF_BST_FL__PDBURST(ip))
#define set__AUD_SPDIF_BST_FL__PDBURST(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_BST_FL(ip)) & \
	~(mask__AUD_SPDIF_BST_FL__PDBURST(ip) << \
	shift__AUD_SPDIF_BST_FL__PDBURST(ip))) | (((value) & \
	mask__AUD_SPDIF_BST_FL__PDBURST(ip)) << \
	shift__AUD_SPDIF_BST_FL__PDBURST(ip)), ip->base + \
	offset__AUD_SPDIF_BST_FL(ip))

/* DBURST */

#define shift__AUD_SPDIF_BST_FL__DBURST(ip) 16
#define mask__AUD_SPDIF_BST_FL__DBURST(ip) 0xffff
#define get__AUD_SPDIF_BST_FL__DBURST(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_BST_FL(ip)) >> shift__AUD_SPDIF_BST_FL__DBURST(ip)) \
	& mask__AUD_SPDIF_BST_FL__DBURST(ip))
#define set__AUD_SPDIF_BST_FL__DBURST(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_BST_FL(ip)) & \
	~(mask__AUD_SPDIF_BST_FL__DBURST(ip) << \
	shift__AUD_SPDIF_BST_FL__DBURST(ip))) | (((value) & \
	mask__AUD_SPDIF_BST_FL__DBURST(ip)) << \
	shift__AUD_SPDIF_BST_FL__DBURST(ip)), ip->base + \
	offset__AUD_SPDIF_BST_FL(ip))



/*
 * AUD_SPDIF_CONFIG
 */

#define offset__AUD_SPDIF_CONFIG(ip) (ip->ver < 4 \
	? -1 : 0x40)
#define get__AUD_SPDIF_CONFIG(ip) readl(ip->base + \
	offset__AUD_SPDIF_CONFIG(ip))
#define set__AUD_SPDIF_CONFIG(ip, value) writel(value, ip->base + \
	offset__AUD_SPDIF_CONFIG(ip))

/* P_BIT */

#define shift__AUD_SPDIF_CONFIG__P_BIT(ip) (ip->ver < \
	4 ? -1 : 0)

#define value__AUD_SPDIF_CONFIG__P_BIT__MASK(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define mask__AUD_SPDIF_CONFIG__P_BIT__MASK(ip) \
	(value__AUD_SPDIF_CONFIG__P_BIT__MASK(ip) << \
	shift__AUD_SPDIF_CONFIG__P_BIT(ip))
#define set__AUD_SPDIF_CONFIG__P_BIT__MASK(ip) \
	set__AUD_SPDIF_CONFIG__P_BIT(ip, \
	value__AUD_SPDIF_CONFIG__P_BIT__MASK(ip))

#define value__AUD_SPDIF_CONFIG__P_BIT__HW(ip) (ip->ver < \
	4 ? -1 : 0x0)
#define mask__AUD_SPDIF_CONFIG__P_BIT__HW(ip) \
	(value__AUD_SPDIF_CONFIG__P_BIT__HW(ip) << \
	shift__AUD_SPDIF_CONFIG__P_BIT(ip))
#define set__AUD_SPDIF_CONFIG__P_BIT__HW(ip) \
	set__AUD_SPDIF_CONFIG__P_BIT(ip, \
	value__AUD_SPDIF_CONFIG__P_BIT__HW(ip))

#define value__AUD_SPDIF_CONFIG__P_BIT__FDMA(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define mask__AUD_SPDIF_CONFIG__P_BIT__FDMA(ip) \
	(value__AUD_SPDIF_CONFIG__P_BIT__FDMA(ip) << \
	shift__AUD_SPDIF_CONFIG__P_BIT(ip))
#define set__AUD_SPDIF_CONFIG__P_BIT__FDMA(ip) \
	set__AUD_SPDIF_CONFIG__P_BIT(ip, \
	value__AUD_SPDIF_CONFIG__P_BIT__FDMA(ip))

/* C_BIT */

#define shift__AUD_SPDIF_CONFIG__C_BIT(ip) (ip->ver < \
	4 ? -1 : 1)

#define value__AUD_SPDIF_CONFIG__C_BIT__MASK(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define mask__AUD_SPDIF_CONFIG__C_BIT__MASK(ip) \
	(value__AUD_SPDIF_CONFIG__C_BIT__MASK(ip) << \
	shift__AUD_SPDIF_CONFIG__C_BIT(ip))
#define set__AUD_SPDIF_CONFIG__C_BIT__MASK(ip) \
	set__AUD_SPDIF_CONFIG__C_BIT(ip, \
	value__AUD_SPDIF_CONFIG__C_BIT__MASK(ip))

#define value__AUD_SPDIF_CONFIG__C_BIT__FDMA(ip) (ip->ver < \
	4 ? -1 : 0x0)
#define mask__AUD_SPDIF_CONFIG__C_BIT__FDMA(ip) \
	(value__AUD_SPDIF_CONFIG__C_BIT__FDMA(ip) << \
	shift__AUD_SPDIF_CONFIG__C_BIT(ip))
#define set__AUD_SPDIF_CONFIG__C_BIT__FDMA(ip) \
	set__AUD_SPDIF_CONFIG__C_BIT(ip, \
	value__AUD_SPDIF_CONFIG__C_BIT__FDMA(ip))

#define value__AUD_SPDIF_CONFIG__C_BIT__HW(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define mask__AUD_SPDIF_CONFIG__C_BIT__HW(ip) \
	(value__AUD_SPDIF_CONFIG__C_BIT__HW(ip) << \
	shift__AUD_SPDIF_CONFIG__C_BIT(ip))
#define set__AUD_SPDIF_CONFIG__C_BIT__HW(ip) \
	set__AUD_SPDIF_CONFIG__C_BIT(ip, \
	value__AUD_SPDIF_CONFIG__C_BIT__HW(ip))

/* U_BIT */

#define shift__AUD_SPDIF_CONFIG__U_BIT(ip) (ip->ver < \
	4 ? -1 : 2)

#define value__AUD_SPDIF_CONFIG__U_BIT__MASK(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define mask__AUD_SPDIF_CONFIG__U_BIT__MASK(ip) \
	(value__AUD_SPDIF_CONFIG__U_BIT__MASK(ip) << \
	shift__AUD_SPDIF_CONFIG__U_BIT(ip))
#define set__AUD_SPDIF_CONFIG__U_BIT__MASK(ip) \
	set__AUD_SPDIF_CONFIG__U_BIT(ip, \
	value__AUD_SPDIF_CONFIG__U_BIT__MASK(ip))

#define value__AUD_SPDIF_CONFIG__U_BIT__FDMA(ip) (ip->ver < \
	4 ? -1 : 0x0)
#define mask__AUD_SPDIF_CONFIG__U_BIT__FDMA(ip) \
	(value__AUD_SPDIF_CONFIG__U_BIT__FDMA(ip) << \
	shift__AUD_SPDIF_CONFIG__U_BIT(ip))
#define set__AUD_SPDIF_CONFIG__U_BIT__FDMA(ip) \
	set__AUD_SPDIF_CONFIG__U_BIT(ip, \
	value__AUD_SPDIF_CONFIG__U_BIT__FDMA(ip))

#define value__AUD_SPDIF_CONFIG__U_BIT__HW(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define mask__AUD_SPDIF_CONFIG__U_BIT__HW(ip) \
	(value__AUD_SPDIF_CONFIG__U_BIT__HW(ip) << \
	shift__AUD_SPDIF_CONFIG__U_BIT(ip))
#define set__AUD_SPDIF_CONFIG__U_BIT__HW(ip) \
	set__AUD_SPDIF_CONFIG__U_BIT(ip, \
	value__AUD_SPDIF_CONFIG__U_BIT__HW(ip))

/* V_BIT */

#define shift__AUD_SPDIF_CONFIG__V_BIT(ip) (ip->ver < \
	4 ? -1 : 2)

#define value__AUD_SPDIF_CONFIG__V_BIT__MASK(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define mask__AUD_SPDIF_CONFIG__V_BIT__MASK(ip) \
	(value__AUD_SPDIF_CONFIG__V_BIT__MASK(ip) << \
	shift__AUD_SPDIF_CONFIG__V_BIT(ip))
#define set__AUD_SPDIF_CONFIG__V_BIT__MASK(ip) \
	set__AUD_SPDIF_CONFIG__V_BIT(ip, \
	value__AUD_SPDIF_CONFIG__V_BIT__MASK(ip))

#define value__AUD_SPDIF_CONFIG__V_BIT__FDMA(ip) (ip->ver < \
	4 ? -1 : 0x0)
#define mask__AUD_SPDIF_CONFIG__V_BIT__FDMA(ip) \
	(value__AUD_SPDIF_CONFIG__V_BIT__FDMA(ip) << \
	shift__AUD_SPDIF_CONFIG__V_BIT(ip))
#define set__AUD_SPDIF_CONFIG__V_BIT__FDMA(ip) \
	set__AUD_SPDIF_CONFIG__V_BIT(ip, \
	value__AUD_SPDIF_CONFIG__V_BIT__FDMA(ip))

#define value__AUD_SPDIF_CONFIG__V_BIT__HW(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define mask__AUD_SPDIF_CONFIG__V_BIT__HW(ip) \
	(value__AUD_SPDIF_CONFIG__V_BIT__HW(ip) << \
	shift__AUD_SPDIF_CONFIG__V_BIT(ip))
#define set__AUD_SPDIF_CONFIG__V_BIT__HW(ip) \
	set__AUD_SPDIF_CONFIG__V_BIT(ip, \
	value__AUD_SPDIF_CONFIG__V_BIT__HW(ip))

/* ONE_BIT_AUDIO */

#define shift__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO(ip) (ip->ver < \
	4 ? -1 : 4)
#define mask__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define get__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CONFIG(ip)) >> \
	shift__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO(ip)) & \
	mask__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO(ip))
#define set__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_CONFIG(ip)) & \
	~(mask__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO(ip) << \
	shift__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO(ip))) | (((value) & \
	mask__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO(ip)) << \
	shift__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO(ip)), ip->base + \
	offset__AUD_SPDIF_CONFIG(ip))

#define value__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO__DISABLED(ip) (ip->ver \
	< 4 ? -1 : 0x0)
#define mask__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO__DISABLED(ip) \
	(value__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO__DISABLED(ip) << \
	shift__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO(ip))
#define set__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO__DISABLED(ip) \
	set__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO(ip, \
	value__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO__DISABLED(ip))

#define value__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO__ENABLED(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define mask__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO__ENABLED(ip) \
	(value__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO__ENABLED(ip) << \
	shift__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO(ip))
#define set__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO__ENABLED(ip) \
	set__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO(ip, \
	value__AUD_SPDIF_CONFIG__ONE_BIT_AUDIO__ENABLED(ip))

/* MEM_FMT */

#define shift__AUD_SPDIF_CONFIG__MEM_FMT(ip) (ip->ver < \
	4 ? -1 : 5)
#define mask__AUD_SPDIF_CONFIG__MEM_FMT(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define get__AUD_SPDIF_CONFIG__MEM_FMT(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CONFIG(ip)) >> shift__AUD_SPDIF_CONFIG__MEM_FMT(ip)) \
	& mask__AUD_SPDIF_CONFIG__MEM_FMT(ip))
#define set__AUD_SPDIF_CONFIG__MEM_FMT(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_CONFIG(ip)) & \
	~(mask__AUD_SPDIF_CONFIG__MEM_FMT(ip) << \
	shift__AUD_SPDIF_CONFIG__MEM_FMT(ip))) | (((value) & \
	mask__AUD_SPDIF_CONFIG__MEM_FMT(ip)) << \
	shift__AUD_SPDIF_CONFIG__MEM_FMT(ip)), ip->base + \
	offset__AUD_SPDIF_CONFIG(ip))

#define value__AUD_SPDIF_CONFIG__MEM_FMT__16_BITS_0_BITS(ip) (ip->ver \
	< 4 ? -1 : 0x0)
#define mask__AUD_SPDIF_CONFIG__MEM_FMT__16_BITS_0_BITS(ip) \
	(value__AUD_SPDIF_CONFIG__MEM_FMT__16_BITS_0_BITS(ip) << \
	shift__AUD_SPDIF_CONFIG__MEM_FMT(ip))
#define set__AUD_SPDIF_CONFIG__MEM_FMT__16_BITS_0_BITS(ip) \
	set__AUD_SPDIF_CONFIG__MEM_FMT(ip, \
	value__AUD_SPDIF_CONFIG__MEM_FMT__16_BITS_0_BITS(ip))

#define value__AUD_SPDIF_CONFIG__MEM_FMT__16_BITS_16_BITS(ip) (ip->ver \
	< 4 ? -1 : 0x1)
#define mask__AUD_SPDIF_CONFIG__MEM_FMT__16_BITS_16_BITS(ip) \
	(value__AUD_SPDIF_CONFIG__MEM_FMT__16_BITS_16_BITS(ip) << \
	shift__AUD_SPDIF_CONFIG__MEM_FMT(ip))
#define set__AUD_SPDIF_CONFIG__MEM_FMT__16_BITS_16_BITS(ip) \
	set__AUD_SPDIF_CONFIG__MEM_FMT(ip, \
	value__AUD_SPDIF_CONFIG__MEM_FMT__16_BITS_16_BITS(ip))

/* DTS_HD */

#define shift__AUD_SPDIF_CONFIG__DTS_HD(ip) (ip->ver < \
	4 ? -1 : 6)
#define mask__AUD_SPDIF_CONFIG__DTS_HD(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define get__AUD_SPDIF_CONFIG__DTS_HD(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CONFIG(ip)) >> shift__AUD_SPDIF_CONFIG__DTS_HD(ip)) \
	& mask__AUD_SPDIF_CONFIG__DTS_HD(ip))
#define set__AUD_SPDIF_CONFIG__DTS_HD(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_CONFIG(ip)) & \
	~(mask__AUD_SPDIF_CONFIG__DTS_HD(ip) << \
	shift__AUD_SPDIF_CONFIG__DTS_HD(ip))) | (((value) & \
	mask__AUD_SPDIF_CONFIG__DTS_HD(ip)) << \
	shift__AUD_SPDIF_CONFIG__DTS_HD(ip)), ip->base + \
	offset__AUD_SPDIF_CONFIG(ip))

#define value__AUD_SPDIF_CONFIG__DTS_HD__DISABLED(ip) (ip->ver < \
	4 ? -1 : 0x0)
#define mask__AUD_SPDIF_CONFIG__DTS_HD__DISABLED(ip) \
	(value__AUD_SPDIF_CONFIG__DTS_HD__DISABLED(ip) << \
	shift__AUD_SPDIF_CONFIG__DTS_HD(ip))
#define set__AUD_SPDIF_CONFIG__DTS_HD__DISABLED(ip) \
	set__AUD_SPDIF_CONFIG__DTS_HD(ip, \
	value__AUD_SPDIF_CONFIG__DTS_HD__DISABLED(ip))

#define value__AUD_SPDIF_CONFIG__DTS_HD__ENABLED(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define mask__AUD_SPDIF_CONFIG__DTS_HD__ENABLED(ip) \
	(value__AUD_SPDIF_CONFIG__DTS_HD__ENABLED(ip) << \
	shift__AUD_SPDIF_CONFIG__DTS_HD(ip))
#define set__AUD_SPDIF_CONFIG__DTS_HD__ENABLED(ip) \
	set__AUD_SPDIF_CONFIG__DTS_HD(ip, \
	value__AUD_SPDIF_CONFIG__DTS_HD__ENABLED(ip))

/* BACK_STALLING */

#define shift__AUD_SPDIF_CONFIG__BACK_STALLING(ip) (ip->ver < \
	4 ? -1 : 7)
#define mask__AUD_SPDIF_CONFIG__BACK_STALLING(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define get__AUD_SPDIF_CONFIG__BACK_STALLING(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CONFIG(ip)) >> \
	shift__AUD_SPDIF_CONFIG__BACK_STALLING(ip)) & \
	mask__AUD_SPDIF_CONFIG__BACK_STALLING(ip))
#define set__AUD_SPDIF_CONFIG__BACK_STALLING(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_CONFIG(ip)) & \
	~(mask__AUD_SPDIF_CONFIG__BACK_STALLING(ip) << \
	shift__AUD_SPDIF_CONFIG__BACK_STALLING(ip))) | (((value) & \
	mask__AUD_SPDIF_CONFIG__BACK_STALLING(ip)) << \
	shift__AUD_SPDIF_CONFIG__BACK_STALLING(ip)), ip->base + \
	offset__AUD_SPDIF_CONFIG(ip))

#define value__AUD_SPDIF_CONFIG__BACK_STALLING__DISABLED(ip) (ip->ver \
	< 4 ? -1 : 0x0)
#define mask__AUD_SPDIF_CONFIG__BACK_STALLING__DISABLED(ip) \
	(value__AUD_SPDIF_CONFIG__BACK_STALLING__DISABLED(ip) << \
	shift__AUD_SPDIF_CONFIG__BACK_STALLING(ip))
#define set__AUD_SPDIF_CONFIG__BACK_STALLING__DISABLED(ip) \
	set__AUD_SPDIF_CONFIG__BACK_STALLING(ip, \
	value__AUD_SPDIF_CONFIG__BACK_STALLING__DISABLED(ip))

#define value__AUD_SPDIF_CONFIG__BACK_STALLING__ENABLED(ip) (ip->ver < \
	4 ? -1 : 0x1)
#define mask__AUD_SPDIF_CONFIG__BACK_STALLING__ENABLED(ip) \
	(value__AUD_SPDIF_CONFIG__BACK_STALLING__ENABLED(ip) << \
	shift__AUD_SPDIF_CONFIG__BACK_STALLING(ip))
#define set__AUD_SPDIF_CONFIG__BACK_STALLING__ENABLED(ip) \
	set__AUD_SPDIF_CONFIG__BACK_STALLING(ip, \
	value__AUD_SPDIF_CONFIG__BACK_STALLING__ENABLED(ip))

/* DMA_REQ_TRIG_LMT */

#define shift__AUD_SPDIF_CONFIG__DMA_REQ_TRIG_LMT(ip) (ip->ver < \
	4 ? -1 : 8)
#define mask__AUD_SPDIF_CONFIG__DMA_REQ_TRIG_LMT(ip) (ip->ver < \
	4 ? -1 : 0x1f)
#define get__AUD_SPDIF_CONFIG__DMA_REQ_TRIG_LMT(ip) ((readl(ip->base + \
	offset__AUD_SPDIF_CONFIG(ip)) >> \
	shift__AUD_SPDIF_CONFIG__DMA_REQ_TRIG_LMT(ip)) & \
	mask__AUD_SPDIF_CONFIG__DMA_REQ_TRIG_LMT(ip))
#define set__AUD_SPDIF_CONFIG__DMA_REQ_TRIG_LMT(ip, value) \
	writel((readl(ip->base + offset__AUD_SPDIF_CONFIG(ip)) & \
	~(mask__AUD_SPDIF_CONFIG__DMA_REQ_TRIG_LMT(ip) << \
	shift__AUD_SPDIF_CONFIG__DMA_REQ_TRIG_LMT(ip))) | (((value) & \
	mask__AUD_SPDIF_CONFIG__DMA_REQ_TRIG_LMT(ip)) << \
	shift__AUD_SPDIF_CONFIG__DMA_REQ_TRIG_LMT(ip)), ip->base + \
	offset__AUD_SPDIF_CONFIG(ip))



#endif
