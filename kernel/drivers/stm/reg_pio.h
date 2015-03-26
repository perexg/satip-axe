#ifndef __STM_PIO_H
#define __STM_PIO_H



/*
 * PIO_POUT
 */

#define offset__PIO_POUT() 0x00
#define get__PIO_POUT(base) readl(base + offset__PIO_POUT())
#define set__PIO_POUT(base, value) writel(value, base + \
	offset__PIO_POUT())

/* POUT */

#define shift__PIO_POUT__POUT(pin) pin
#define mask__PIO_POUT__POUT(pin) 0x1
#define get__PIO_POUT__POUT(base, pin) ((readl(base + \
	offset__PIO_POUT()) >> shift__PIO_POUT__POUT(pin)) & \
	mask__PIO_POUT__POUT(pin))
#define set__PIO_POUT__POUT(base, pin, value) writel((readl(base + \
	offset__PIO_POUT()) & ~(mask__PIO_POUT__POUT(pin) << \
	shift__PIO_POUT__POUT(pin))) | (((value) & mask__PIO_POUT__POUT(pin)) \
	<< shift__PIO_POUT__POUT(pin)), base + offset__PIO_POUT())



/*
 * PIO_SET_POUT
 */

#define offset__PIO_SET_POUT() 0x04
#define set__PIO_SET_POUT(base, value) writel(value, base + \
	offset__PIO_SET_POUT())

/* SET_POUT */

#define shift__PIO_SET_POUT__SET_POUT(pin) pin
#define mask__PIO_SET_POUT__SET_POUT(pin) 0x1
#define set__PIO_SET_POUT__SET_POUT(base, pin, value) writel(((value) \
	& mask__PIO_SET_POUT__SET_POUT(pin)) << \
	shift__PIO_SET_POUT__SET_POUT(pin), base + offset__PIO_SET_POUT())

#define value__PIO_SET_POUT__SET_POUT__SET() 0x1
#define mask__PIO_SET_POUT__SET_POUT__SET(pin) \
	(value__PIO_SET_POUT__SET_POUT__SET() << \
	shift__PIO_SET_POUT__SET_POUT(pin))
#define set__PIO_SET_POUT__SET_POUT__SET(base, pin) \
	set__PIO_SET_POUT__SET_POUT(base, pin, \
	value__PIO_SET_POUT__SET_POUT__SET())



/*
 * PIO_CLR_POUT
 */

#define offset__PIO_CLR_POUT() 0x08
#define set__PIO_CLR_POUT(base, value) writel(value, base + \
	offset__PIO_CLR_POUT())

/* CLR_POUT */

#define shift__PIO_CLR_POUT__CLR_POUT(pin) pin
#define mask__PIO_CLR_POUT__CLR_POUT(pin) 0x1
#define set__PIO_CLR_POUT__CLR_POUT(base, pin, value) writel(((value) \
	& mask__PIO_CLR_POUT__CLR_POUT(pin)) << \
	shift__PIO_CLR_POUT__CLR_POUT(pin), base + offset__PIO_CLR_POUT())

#define value__PIO_CLR_POUT__CLR_POUT__CLEAR() 0x1
#define mask__PIO_CLR_POUT__CLR_POUT__CLEAR(pin) \
	(value__PIO_CLR_POUT__CLR_POUT__CLEAR() << \
	shift__PIO_CLR_POUT__CLR_POUT(pin))
#define set__PIO_CLR_POUT__CLR_POUT__CLEAR(base, pin) \
	set__PIO_CLR_POUT__CLR_POUT(base, pin, \
	value__PIO_CLR_POUT__CLR_POUT__CLEAR())



/*
 * PIO_PIN
 */

#define offset__PIO_PIN() 0x10
#define get__PIO_PIN(base) readl(base + offset__PIO_PIN())
#define set__PIO_PIN(base, value) writel(value, base + \
	offset__PIO_PIN())

/* PIN */

#define shift__PIO_PIN__PIN(pin) pin
#define mask__PIO_PIN__PIN(pin) 0x1
#define get__PIO_PIN__PIN(base, pin) ((readl(base + offset__PIO_PIN()) \
	>> shift__PIO_PIN__PIN(pin)) & mask__PIO_PIN__PIN(pin))
#define set__PIO_PIN__PIN(base, pin, value) writel((readl(base + \
	offset__PIO_PIN()) & ~(mask__PIO_PIN__PIN(pin) << \
	shift__PIO_PIN__PIN(pin))) | (((value) & mask__PIO_PIN__PIN(pin)) << \
	shift__PIO_PIN__PIN(pin)), base + offset__PIO_PIN())



/*
 * PIO_PC
 */

#define offset__PIO_PC(n) (0x20 + (n) * 0x10)
#define get__PIO_PC(base, n) readl(base + offset__PIO_PC(n))
#define set__PIO_PC(base, n, value) writel(value, base + \
	offset__PIO_PC(n))

/* CONFIGDATA */

#define shift__PIO_PC__CONFIGDATA(pin) pin
#define mask__PIO_PC__CONFIGDATA(pin) 0x1
#define get__PIO_PC__CONFIGDATA(base, n, pin) ((readl(base + \
	offset__PIO_PC(n)) >> shift__PIO_PC__CONFIGDATA(pin)) & \
	mask__PIO_PC__CONFIGDATA(pin))
#define set__PIO_PC__CONFIGDATA(base, n, pin, value) \
	writel((readl(base + offset__PIO_PC(n)) & \
	~(mask__PIO_PC__CONFIGDATA(pin) << shift__PIO_PC__CONFIGDATA(pin))) | \
	(((value) & mask__PIO_PC__CONFIGDATA(pin)) << \
	shift__PIO_PC__CONFIGDATA(pin)), base + offset__PIO_PC(n))



/*
 * PIO_SET_PC
 */

#define offset__PIO_SET_PC(n) (0x24 + (n) * 0x10)
#define set__PIO_SET_PC(base, n, value) writel(value, base + \
	offset__PIO_SET_PC(n))

/* SET_PC */

#define shift__PIO_SET_PC__SET_PC(pin) pin
#define mask__PIO_SET_PC__SET_PC(pin) 0x1
#define set__PIO_SET_PC__SET_PC(base, n, pin, value) writel(((value) & \
	mask__PIO_SET_PC__SET_PC(pin)) << shift__PIO_SET_PC__SET_PC(pin), base \
	+ offset__PIO_SET_PC(n))

#define value__PIO_SET_PC__SET_PC__SET() 0x1
#define mask__PIO_SET_PC__SET_PC__SET(pin) \
	(value__PIO_SET_PC__SET_PC__SET() << shift__PIO_SET_PC__SET_PC(pin))
#define set__PIO_SET_PC__SET_PC__SET(base, n, pin) \
	set__PIO_SET_PC__SET_PC(base, n, pin, \
	value__PIO_SET_PC__SET_PC__SET())



/*
 * PIO_CLR_PC
 */

#define offset__PIO_CLR_PC(n) (0x28 + (n) * 0x10)
#define set__PIO_CLR_PC(base, n, value) writel(value, base + \
	offset__PIO_CLR_PC(n))

/* CLR_PC */

#define shift__PIO_CLR_PC__CLR_PC(pin) pin
#define mask__PIO_CLR_PC__CLR_PC(pin) 0x1
#define set__PIO_CLR_PC__CLR_PC(base, n, pin, value) writel(((value) & \
	mask__PIO_CLR_PC__CLR_PC(pin)) << shift__PIO_CLR_PC__CLR_PC(pin), base \
	+ offset__PIO_CLR_PC(n))

#define value__PIO_CLR_PC__CLR_PC__CLEAR() 0x1
#define mask__PIO_CLR_PC__CLR_PC__CLEAR(pin) \
	(value__PIO_CLR_PC__CLR_PC__CLEAR() << shift__PIO_CLR_PC__CLR_PC(pin))
#define set__PIO_CLR_PC__CLR_PC__CLEAR(base, n, pin) \
	set__PIO_CLR_PC__CLR_PC(base, n, pin, \
	value__PIO_CLR_PC__CLR_PC__CLEAR())



/*
 * PIO_PCx (concurrent PC0..2 access)
 */

static inline unsigned long get__PIO_PCx(void *base, int pin)
{
	unsigned long result = 0;
	int i;

	for (i = 0; i <= 2; i++)
		result |= get__PIO_PC__CONFIGDATA(base, i, pin) << i;

	return result;
}

static inline void set__PIO_PCx(void *base, int pin, unsigned long value)
{
	int i;

	for (i = 0; i <= 2; i++)
		if (value & (1 << i))
			set__PIO_SET_PC__SET_PC__SET(base, i, pin);
		else
			set__PIO_CLR_PC__CLR_PC__CLEAR(base, i, pin);
}

#define value__PIO_PCx__INPUT_WEAK_PULL_UP() 0x0
static inline void set__PIO_PCx__INPUT_WEAK_PULL_UP(void *base, int pin)
{
	set__PIO_CLR_PC__CLR_PC__CLEAR(base, 2, pin);
	set__PIO_CLR_PC__CLR_PC__CLEAR(base, 1, pin);
	set__PIO_CLR_PC__CLR_PC__CLEAR(base, 0, pin);
}

#define value__PIO_PCx__BIDIR_OPEN_DRAIN() 0x1
#define value__PIO_PCx__BIDIR_OPEN_DRAIN__alt() 0x3
static inline void set__PIO_PCx__BIDIR_OPEN_DRAIN(void *base, int pin)
{
	set__PIO_CLR_PC__CLR_PC__CLEAR(base, 2, pin);
	set__PIO_CLR_PC__CLR_PC__CLEAR(base, 1, pin);
	set__PIO_SET_PC__SET_PC__SET(base, 0, pin);
}

#define value__PIO_PCx__OUTPUT_PUSH_PULL() 0x2
static inline void set__PIO_PCx__OUTPUT_PUSH_PULL(void *base, int pin)
{
	set__PIO_CLR_PC__CLR_PC__CLEAR(base, 2, pin);
	set__PIO_SET_PC__SET_PC__SET(base, 1, pin);
	set__PIO_CLR_PC__CLR_PC__CLEAR(base, 0, pin);
}

#define value__PIO_PCx__INPUT_HIGH_IMPEDANCE() 0x4
#define value__PIO_PCx__INPUT_HIGH_IMPEDANCE__alt() 0x5
static inline void set__PIO_PCx__INPUT_HIGH_IMPEDANCE(void *base, int pin)
{
	set__PIO_SET_PC__SET_PC__SET(base, 2, pin);
	set__PIO_CLR_PC__CLR_PC__CLEAR(base, 1, pin);
	set__PIO_CLR_PC__CLR_PC__CLEAR(base, 0, pin);
}

#define value__PIO_PCx__ALTERNATIVE_OUTPUT_PUSH_PULL() 0x6
static inline void set__PIO_PCx__ALTERNATIVE_OUTPUT_PUSH_PULL(void *base,
		int pin)
{
	set__PIO_SET_PC__SET_PC__SET(base, 2, pin);
	set__PIO_SET_PC__SET_PC__SET(base, 1, pin);
	set__PIO_CLR_PC__CLR_PC__CLEAR(base, 0, pin);
}

#define value__PIO_PCx__ALTERNATIVE_BIDIR_OPEN_DRAIN() 0x7
static inline void set__PIO_PCx__ALTERNATIVE_BIDIR_OPEN_DRAIN(void *base,
		int pin)
{
	set__PIO_SET_PC__SET_PC__SET(base, 2, pin);
	set__PIO_SET_PC__SET_PC__SET(base, 1, pin);
	set__PIO_SET_PC__SET_PC__SET(base, 0, pin);
}



/*
 * PIO_PCOMP
 */

#define offset__PIO_PCOMP() 0x50
#define get__PIO_PCOMP(base) readl(base + offset__PIO_PCOMP())
#define set__PIO_PCOMP(base, value) writel(value, base + \
	offset__PIO_PCOMP())

/* PCOMP */

#define shift__PIO_PCOMP__PCOMP(pin) pin
#define mask__PIO_PCOMP__PCOMP(pin) 0x1
#define get__PIO_PCOMP__PCOMP(base, pin) ((readl(base + \
	offset__PIO_PCOMP()) >> shift__PIO_PCOMP__PCOMP(pin)) & \
	mask__PIO_PCOMP__PCOMP(pin))
#define set__PIO_PCOMP__PCOMP(base, pin, value) writel((readl(base + \
	offset__PIO_PCOMP()) & ~(mask__PIO_PCOMP__PCOMP(pin) << \
	shift__PIO_PCOMP__PCOMP(pin))) | (((value) & \
	mask__PIO_PCOMP__PCOMP(pin)) << shift__PIO_PCOMP__PCOMP(pin)), base + \
	offset__PIO_PCOMP())



/*
 * PIO_SET_PCOMP
 */

#define offset__PIO_SET_PCOMP() 0x54
#define set__PIO_SET_PCOMP(base, value) writel(value, base + \
	offset__PIO_SET_PCOMP())

/* SET_PCOMP */

#define shift__PIO_SET_PCOMP__SET_PCOMP(pin) pin
#define mask__PIO_SET_PCOMP__SET_PCOMP(pin) 0x1
#define set__PIO_SET_PCOMP__SET_PCOMP(base, pin, value) \
	writel(((value) & mask__PIO_SET_PCOMP__SET_PCOMP(pin)) << \
	shift__PIO_SET_PCOMP__SET_PCOMP(pin), base + offset__PIO_SET_PCOMP())

#define value__PIO_SET_PCOMP__SET_PCOMP__SET() 0x1
#define mask__PIO_SET_PCOMP__SET_PCOMP__SET(pin) \
	(value__PIO_SET_PCOMP__SET_PCOMP__SET() << \
	shift__PIO_SET_PCOMP__SET_PCOMP(pin))
#define set__PIO_SET_PCOMP__SET_PCOMP__SET(base, pin) \
	set__PIO_SET_PCOMP__SET_PCOMP(base, pin, \
	value__PIO_SET_PCOMP__SET_PCOMP__SET())



/*
 * PIO_CLR_PCOMP
 */

#define offset__PIO_CLR_PCOMP() 0x58
#define set__PIO_CLR_PCOMP(base, value) writel(value, base + \
	offset__PIO_CLR_PCOMP())

/* CLR_PCOMP */

#define shift__PIO_CLR_PCOMP__CLR_PCOMP(pin) pin
#define mask__PIO_CLR_PCOMP__CLR_PCOMP(pin) 0x1
#define set__PIO_CLR_PCOMP__CLR_PCOMP(base, pin, value) \
	writel(((value) & mask__PIO_CLR_PCOMP__CLR_PCOMP(pin)) << \
	shift__PIO_CLR_PCOMP__CLR_PCOMP(pin), base + offset__PIO_CLR_PCOMP())

#define value__PIO_CLR_PCOMP__CLR_PCOMP__CLEAR() 0x1
#define mask__PIO_CLR_PCOMP__CLR_PCOMP__CLEAR(pin) \
	(value__PIO_CLR_PCOMP__CLR_PCOMP__CLEAR() << \
	shift__PIO_CLR_PCOMP__CLR_PCOMP(pin))
#define set__PIO_CLR_PCOMP__CLR_PCOMP__CLEAR(base, pin) \
	set__PIO_CLR_PCOMP__CLR_PCOMP(base, pin, \
	value__PIO_CLR_PCOMP__CLR_PCOMP__CLEAR())



/*
 * PIO_PMASK
 */

#define offset__PIO_PMASK() 0x60
#define get__PIO_PMASK(base) readl(base + offset__PIO_PMASK())
#define set__PIO_PMASK(base, value) writel(value, base + \
	offset__PIO_PMASK())

/* PMASK */

#define shift__PIO_PMASK__PMASK(pin) pin
#define mask__PIO_PMASK__PMASK(pin) 0x1
#define get__PIO_PMASK__PMASK(base, pin) ((readl(base + \
	offset__PIO_PMASK()) >> shift__PIO_PMASK__PMASK(pin)) & \
	mask__PIO_PMASK__PMASK(pin))
#define set__PIO_PMASK__PMASK(base, pin, value) writel((readl(base + \
	offset__PIO_PMASK()) & ~(mask__PIO_PMASK__PMASK(pin) << \
	shift__PIO_PMASK__PMASK(pin))) | (((value) & \
	mask__PIO_PMASK__PMASK(pin)) << shift__PIO_PMASK__PMASK(pin)), base + \
	offset__PIO_PMASK())



/*
 * PIO_SET_PMASK
 */

#define offset__PIO_SET_PMASK() 0x64
#define set__PIO_SET_PMASK(base, value) writel(value, base + \
	offset__PIO_SET_PMASK())

/* SET_PMASK */

#define shift__PIO_SET_PMASK__SET_PMASK(pin) pin
#define mask__PIO_SET_PMASK__SET_PMASK(pin) 0x1
#define set__PIO_SET_PMASK__SET_PMASK(base, pin, value) \
	writel(((value) & mask__PIO_SET_PMASK__SET_PMASK(pin)) << \
	shift__PIO_SET_PMASK__SET_PMASK(pin), base + offset__PIO_SET_PMASK())

#define value__PIO_SET_PMASK__SET_PMASK__SET() 0x1
#define mask__PIO_SET_PMASK__SET_PMASK__SET(pin) \
	(value__PIO_SET_PMASK__SET_PMASK__SET() << \
	shift__PIO_SET_PMASK__SET_PMASK(pin))
#define set__PIO_SET_PMASK__SET_PMASK__SET(base, pin) \
	set__PIO_SET_PMASK__SET_PMASK(base, pin, \
	value__PIO_SET_PMASK__SET_PMASK__SET())



/*
 * PIO_CLR_PMASK
 */

#define offset__PIO_CLR_PMASK() 0x68
#define set__PIO_CLR_PMASK(base, value) writel(value, base + \
	offset__PIO_CLR_PMASK())

/* CLR_PMASK */

#define shift__PIO_CLR_PMASK__CLR_PMASK(pin) pin
#define mask__PIO_CLR_PMASK__CLR_PMASK(pin) 0x1
#define set__PIO_CLR_PMASK__CLR_PMASK(base, pin, value) \
	writel(((value) & mask__PIO_CLR_PMASK__CLR_PMASK(pin)) << \
	shift__PIO_CLR_PMASK__CLR_PMASK(pin), base + offset__PIO_CLR_PMASK())

#define value__PIO_CLR_PMASK__CLR_PMASK__CLEAR() 0x1
#define mask__PIO_CLR_PMASK__CLR_PMASK__CLEAR(pin) \
	(value__PIO_CLR_PMASK__CLR_PMASK__CLEAR() << \
	shift__PIO_CLR_PMASK__CLR_PMASK(pin))
#define set__PIO_CLR_PMASK__CLR_PMASK__CLEAR(base, pin) \
	set__PIO_CLR_PMASK__CLR_PMASK(base, pin, \
	value__PIO_CLR_PMASK__CLR_PMASK__CLEAR())



#endif
