#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

typedef unsigned char u8;

static unsigned long
getTick ()
{
	static unsigned long init = 0;
	unsigned long t;
	struct timespec ts;

	clock_gettime(CLOCK_REALTIME, &ts);
	t = ts.tv_nsec / 1000000;
	t += ts.tv_sec * 1000;
	if (init == 0)
		init = t;
	return t - init;
}

struct regdmp {
	unsigned flags;
	unsigned shift;
	unsigned mask;
	const char *name;
};

struct reg {
	unsigned reg;
	unsigned flags;
	struct regdmp *wdmp;
	struct regdmp *rdmp;
	const char *name;
};

static struct regdmp f129w[] = {
	{ 0, 3, 1, "I2C_FASTMODE" },
	{ 0, 0, 3, "I2CADDR_INC" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f12aw[] = {
	{ 0, 7, 1, "I2CT_ON" },
	{ 0, 4, 7, "ENARPT_LEVEL" },
	{ 0, 3, 1, "SCLT_DELAY" },
	{ 0, 2, 1, "STOP_ENABLE" },
	{ 0, 1, 1, "STOP_SDAT2SDA" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f1a0w[] = {
	{ 0, 7, 1, "TIM_OFF" },
	{ 0, 6, 1, "DISEQC_RESET" },
	{ 0, 4, 3, "TIM_CMD" },
	{ 0, 3, 1, "DIS_PRECHARGE" },
	{ 0, 0, 7, "DISTX_MODE" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f216w[] = {
	{ 0, 0,31, "DEMOD_MODE" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f23dw[] = {
        { 0, 8,255,"K_FREQ_HDR" },
	{ 0, 4,15, "KC_COARSE_EXP" },
	{ 0, 0,15, "BETA_FREQ" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f1a8r[] = {
	{ 0, 7, 1, "TX_FAIL" },
	{ 0, 6, 1, "FIFO_FULL" },
	{ 0, 5, 1, "TX_IDLE" },
	{ 0, 4, 1, "GAP_BURST" },
	{ 0, 0,15, "TXFIFO_BYTES" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f206r[] = {
	{ 0, 7, 1, "AGC1_LOCKED" },
	{ 0, 4, 1, "AGC1_MINPOWER" },
	{ 0, 3, 1, "AGCOUT_FAST" },
	{ 0, 0, 7, "AGCIQ_BETA" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f211r[] = {
	{ 0, 7, 1, "MANUAL_MODCOD" },
	{ 0, 2,31, "DEMOD_MODCOD" },
	{ 0, 0, 3, "DEMOD_TYPE" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f212r[] = {
	{ 0, 7, 1, "CAR_LOCK" },
	{ 0, 5, 3, "TMGLOCK_QUALITY" },
	{ 0, 3, 1, "LOCK_DEFINITE" },
	{ 0, 0, 1, "OVADC_DETECT" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f33er[] = {
	{ 0, 4, 1, "PRFVIT" },
	{ 0, 3, 1, "LOCKEDVIT" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f369r[] = {
	{ 0, 7, 1, "PKTDELIN_DELOCK" },
	{ 0, 6, 1, "SYNCDUPDFL_BADDFL" },
	{ 0, 5, 1, "CONTINUOUS_STREAM" },
	{ 0, 4, 1, "UNACCEPTED_STREAM" },
	{ 0, 3, 1, "BCH_ERROR_FLAG" },
	{ 0, 1, 1, "PKTDELIN_LOCK" },
	{ 0, 0, 1, "FIRST_LOCK" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f381r[] = {
	{ 0, 7, 1, "TSFIFO_LINEOK" },
	{ 0, 6, 1, "TSFIFO_ERROR" },
	{ 0, 0, 1, "DIL_READY" },
	{ 0, 0, 0, NULL }
};

static struct reg reg_tbl[] = {
	{ 0xf129, 0, f129w, NULL,  "I2CCFG" },
	{ 0xf12a, 0, f12aw, NULL,  "P1_I2CRPT" },
	{ 0xf1a0, 0, f1a0w, NULL,  "P1_DISTXCTL" },
	{ 0xf1a7, 0, NULL,  NULL,  "P1_DISTXDATA" },
	{ 0xf1a8, 0, NULL,  f1a8r, "P1_DISTXSTATUS" },
	{ 0xf206, 0, NULL,  f206r, "P2_AGC1CN" },
	{ 0xf20e, 0, NULL,  NULL,  "P2_AGCIQIN1" },
	{ 0xf211, 0, NULL,  f211r,  "P2_DMDMODCOD" },
	{ 0xf212, 0, NULL,  f212r, "P2_DSTATUS" },
	{ 0xf216, 0, f216w, NULL,  "P2_DMDISTATE" },
	{ 0xf22d, 0, NULL,  NULL,  "P2_AGC2_REF" },
	{ 0xf23d, 0, f23dw, NULL,  "P2_CARFREQ" },
        { 0xf248, 0, NULL,  NULL,  "P2_CFRINIT1" },
        { 0xf25e, 0, NULL,  NULL,  "P2_SFRINIT1" },
	{ 0xf280, 0, NULL,  NULL,  "P2_NNOSDATAT1" },
	{ 0xf284, 0, NULL,  NULL,  "P2_NNOSPLHT1" },
	{ 0xf33e, 0, NULL,  f33er, "P2_VSTATUSVIT" },
	{ 0xf369, 0, NULL,  f369r, "P2_PDELSTATUS1" },
	{ 0xf381, 0, NULL,  f381r, "P2_TSSTATUS" },
	{ 0xf399, 0, NULL,  NULL,  "P2_ERRCNT12" },
	{ 0xf406, 0, NULL,  f206r, "P1_AGC1CN" },
	{ 0xf40e, 0, NULL,  NULL,  "P1_AGCIQIN1" },
	{ 0xf411, 0, NULL,  f211r, "P1_DMDMODCOD" },
	{ 0xf412, 0, NULL,  f212r, "P1_DSTATUS" },
	{ 0xf416, 0, f216w, NULL,  "P1_DMDISTATE" },
	{ 0xf42d, 0, NULL,  NULL,  "P1_AGC2_REF" },
	{ 0xf43d, 0, f23dw, NULL,  "P1_CARFREQ" },
        { 0xf448, 0, NULL,  NULL,  "P1_CFRINIT1" },
        { 0xf45e, 0, NULL,  NULL,  "P1_SFRINIT1" },
	{ 0xf480, 0, NULL,  NULL,  "P1_NNOSDATAT1" },
	{ 0xf484, 0, NULL,  NULL,  "P1_NNOSPLHT1" },
	{ 0xf53e, 0, NULL,  f33er, "P1_VSTATUSVIT" },
	{ 0xf569, 0, NULL,  f369r, "P1_PDELSTATUS1" },
	{ 0xf581, 0, NULL,  f381r, "P1_TSSTATUS" },
	{ 0xf599, 0, NULL,  NULL,  "P1_ERRCNT12" },
	{ 0, 0, NULL, NULL }
};

static int
i2c_line(int rd, int t1, const char *s)
{
	static struct reg *old_rt[2] = { NULL, NULL };
	static int old_cmd[2] = { 0, 0 };
	int r, addr, cnt, d[16], val;
	struct reg *rt;
	struct regdmp *rtd;
	char buf[1024];

	r = sscanf(s + 11, "%x, %d) %x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x",
	           &addr, &cnt,
	           &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7],
	           &d[8], &d[9], &d[10], &d[11], &d[12], &d[13], &d[14], &d[15]);
        if (r < 3 || cnt != r - 2)
        	return -1;
	if (addr < 0xd0 || addr > 0xd3)
		return -1;
	if (rd) {
		if (old_rt[t1] == NULL)
			return -1;
		if (cnt < 1) {
			old_rt[t1] = NULL;
			return -1;
		}
		val = d[0];
		if (cnt > 1)
			val |= d[1] << 8;
		if (cnt > 2)
			val |= d[2] << 16;
		if (cnt > 3)
			val |= d[3] << 24;
		snprintf(buf, sizeof(buf), "%-40s ;", s);
		for (rtd = old_rt[t1]->rdmp; rtd && rtd->name; rtd++) {
			if (strlen(buf) > 70) {
				printf("%s\n", buf);
				sprintf(buf, "%40s ;", "");
			}
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %s=0x%x",
			         rtd->name, (val >> rtd->shift) & rtd->mask);
		}
		printf("%s\n", buf);
		old_rt[t1] = NULL;
		return 0;
	}
	if (d[0] < 0xf1 || d[0] > 0xf5)
		return -1;
	if (cnt < 2)
		return -1;
	old_cmd[t1] = (d[0] << 8) | d[1];
	for (rt = reg_tbl; rt->name; rt++)
		if (rt->reg == old_cmd[t1])
			break;
	if (rt->name == NULL)
		return -1;
	old_rt[t1] = rt;
	val = 0;
	if (cnt > 2)
		val = d[2];
	if (cnt > 3)
		val |= d[3] << 8;
	if (cnt > 4)
		val |= d[4] << 16;
	if (cnt > 5)
		val |= d[5] << 24;
	snprintf(buf, sizeof(buf), "%-40s ; %s", s, rt->name);
	for (rtd = old_rt[t1]->wdmp; rtd && rtd->name; rtd++) {
		if (strlen(buf) > 70) {
			printf("%s\n", buf);
			sprintf(buf, "%40s ;", "");
		}
		snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %s=0x%x",
			 rtd->name, (val >> rtd->shift) & rtd->mask);
	}
	printf("%s\n", buf);
	return 0;
}

static void 
i2c_decoder(void)
{
	char buf[1024];
	int r;

	while (!feof(stdin)) {
		if (fgets(buf, sizeof(buf), stdin) == NULL)
			break;
		if (buf[0] == '\0')
			continue;
		buf[strlen(buf)-1] = '\0';
		r = -1;
		if (!strncmp(buf, "[i2c] wrte(", 11))
			r = i2c_line(0, 0, buf);
		else if (!strncmp(buf, "[i2c] read(", 11))
			r = i2c_line(1, 0, buf);
		else if (!strncmp(buf, "[i2c] t1_w(", 11))
			r = i2c_line(0, 1, buf);
		else if (!strncmp(buf, "[i2c] t1_r(", 11))
			r = i2c_line(1, 1, buf);
		if (r < 0)
			printf("%s\n", buf);
	}
}

static int i2c_fd = -1;

static int
i2c_open(int num, char *_path)
{
	char path[32];
	if (num < 8)
		sprintf(path, "/dev/i2c-%d", num);
	else
		strcpy(path, "/dev/axe/i2c_drv-0");
	i2c_fd = open(path, O_RDWR);
	if (i2c_fd < 0)
		return -1;
	if (_path)
		strcpy(_path, path);
	return 0;
}

static int
i2c_open_check(void)
{
	if (i2c_fd >= 0)
		return 0;
	if (i2c_open(0, NULL)) {
		printf("Unable to open i2c device\n");
		return -1;
	}
	return 0;
}

static int
i2c_demod_reg_read(int addr, int reg, u8 *buf, int cnt)
{
	u8 buf1[3];
	struct i2c_rdwr_ioctl_data d;
	struct i2c_msg m[2];

	printf("read addr 0x%x reg 0x%x cnt %i\n", addr, reg, cnt);

	if (i2c_open_check())
		return -1;

	memset(&d, 0, sizeof(d));
	memset(&m, 0, sizeof(m));
	memset(buf1, 0, sizeof(buf1));
	memset(buf, 0, cnt);

	buf1[0] = reg >> 8;
	buf1[1] = reg;

	m[0].addr = addr >> 1;
	m[0].len = 2;
	m[0].flags = 0;
	m[0].buf = buf1;

	m[1].addr = addr >> 1;
	m[1].len = cnt;
	m[1].flags = I2C_M_RD;
	m[1].buf = buf;

	d.nmsgs = 2;
	d.msgs = m;
	if (ioctl(i2c_fd, I2C_RDWR, &d) < 0) {
		printf("I2C RDWR failed for addr 0x%x\n", addr);
		return -1;
	}
	return 0;
}

static int
i2c_demod_reg_write(int addr, int reg, u8 *buf, int cnt)
{
	u8 buf1[32];
	struct i2c_rdwr_ioctl_data d;
	struct i2c_msg m[2];

	if (i2c_open_check())
		return -1;

	memset(&d, 0, sizeof(d));
	memset(&m, 0, sizeof(m));
	memset(buf1, 0, sizeof(buf1));

	buf1[0] = reg >> 8;
	buf1[1] = reg;
	memcpy(buf1 + 2, buf, cnt);

	m[0].addr = addr >> 1;
	m[0].len = 2 + cnt;
	m[0].flags = 0;
	m[0].buf = buf1;

	d.nmsgs = 1;
	d.msgs = m;
	if (ioctl(i2c_fd, I2C_RDWR, &d) < 0) {
		printf("I2C RDWR failed for addr 0x%x\n", addr);
		return -1;
	}
	return cnt;
}

static void
i2c_scan(void)
{
	int i, a, r;
	u8 v;
	char path[32];

	for (i = 0; i < 9; i++) {
		if (i2c_open(i, path))
			continue;
		a = 0xd0;
		r = i2c_demod_reg_read(a, 0xf000, &v, 1);
		if (r >= 0)
			printf("I2C read succeed for %s, addr 0x%02x: 0x%02x\n", path, a, r);
		close(i2c_fd);
		i2c_fd = -1;
	}
}

int main(int argc, char *argv[])
{
	if (argc > 1 && !strcmp(argv[1], "wait")) {
		long int ms = 1000;
		const char *f = NULL;
		if (argc > 2)
			ms = atol(argv[2]);
		if (argc > 3)
			nice(atoi(argv[3]));
		if (argc > 4)
			f = argv[4][0] ? argv[4] : NULL;
		ms += getTick();
		while (ms > getTick())
			if (f && !access(f, R_OK))
				break;
	}
	if (argc > 1 && !strcmp(argv[1], "i2c_decoder")) {
		i2c_decoder();
	}
	if (argc > 1 && !strcmp(argv[1], "i2c_scan")) {
		i2c_scan();
	}
	if (argc > 1 && !strcmp(argv[1], "i2c_demod_reg_read")) {
		if (argc > 3) {
			int i;
			int a = strtol(argv[2], NULL, 0);
			int r = strtol(argv[3], NULL, 0);
			int c = argc > 4 ? strtol(argv[4], NULL, 0) : 1;
			u8 buf[16];
			if (a > 0 && r > 0) {
				i = i2c_demod_reg_read(a, r, buf, c > sizeof(buf) ? sizeof(buf) : c);
				if (i < 0)
					printf("Unable to read register 0x%x from addr 0x%x\n", r, a);
				else {
					for (r = 0; r < i; r++)
						printf("%s0x%02x", r > 0 ? ":" : "", buf[i]);
					exit(EXIT_SUCCESS);
				}
			}
		}
		exit(EXIT_FAILURE);
	}
	if (argc > 1 && !strcmp(argv[1], "i2c_demod_reg_write")) {
		if (argc > 4) {
			int i, j;
			int a = strtol(argv[2], NULL, 0);
			int r = strtol(argv[3], NULL, 0);
			u8 buf[16];
			for (j = 4; j < argc && j < sizeof(buf) + 4; j++)
				buf[j-4] = strtol(argv[j], NULL, 0);
			if (a > 0 && r > 0) {
				i = i2c_demod_reg_write(a, r, buf, j);
				if (i < 0)
					printf("Unable to write register 0x%x to addr 0x%x\n", r, a);
				else
					exit(EXIT_SUCCESS);
			}
		}
		exit(EXIT_FAILURE);
	}
	return 0;
}
