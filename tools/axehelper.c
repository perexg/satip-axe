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

#define PWIDTH 90

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
	struct regdmp *dmp;
	const char *name;
};

static struct regdmp byte[] = {
	{ 0, 0,255, "VAL" },
	{ 0, 0, 0, NULL }
};

static struct regdmp conf[] = {
	{ 0, 7, 1, "OPD" },
	{ 0, 1,63, "CONFIG" },
	{ 0, 0, 1, "XOR" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f100[] = {
	{ 0, 4,15,"IDENT" },
	{ 0, 0,15,"RELEASE" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f129[] = {
	{ 0, 3, 1, "I2C_FASTMODE" },
	{ 0, 0, 3, "I2CADDR_INC" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f12a[] = {
	{ 0, 7, 1, "I2CT_ON" },
	{ 0, 4, 7, "ENARPT_LEVEL" },
	{ 0, 3, 1, "SCLT_DELAY" },
	{ 0, 2, 1, "STOP_ENABLE" },
	{ 0, 1, 1, "STOP_SDAT2SDA" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f16a[] = {
	{ 0, 4,15, "SEL2" },
	{ 0, 0,15, "SEL1" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f16b[] = {
	{ 0, 4,15, "SEL4" },
	{ 0, 0,15, "SEL3" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f16c[] = {
	{ 0, 4,15, "SEL6" },
	{ 0, 0,15, "SEL5" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f190[] = {
	{ 0, 7, 1, "TIM_OFF" },
	{ 0, 6, 1, "DISEQC_RESET" },
	{ 0, 4, 3, "TIM_CMD" },
	{ 0, 3, 1, "DIS_PRECHARGE" },
	{ 0, 0, 7, "DISTX_MODE" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f198[] = {
	{ 0, 7, 1, "TX_FAIL" },
	{ 0, 6, 1, "FIFO_FULL" },
	{ 0, 5, 1, "TX_IDLE" },
	{ 0, 4, 1, "GAP_BURST" },
	{ 0, 0,15, "TXFIFO_BYTES" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f1b6[] = {
	{ 0, 7, 1, "STANDBY" },
	{ 0, 6, 1, "BYPASSPLLCORE" },
	{ 0, 5, 1, "SELX1RATIO" },
	{ 0, 3, 1, "STOP_PLL" },
	{ 0, 2, 1, "BYPASSPLLFSK" },
	{ 0, 1, 1, "SELOSCI" },
	{ 0, 0, 1, "BYPASSPLLADC" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f1c2[] = {
	{ 0, 7, 1, "STOP_CLKPKDT2" },
	{ 0, 6, 1, "STOP_CLKPKDT1" },
	{ 0, 5, 1, "STOP_CLKFEC" },
	{ 0, 3, 1, "STOP_CLKADCI2" },
	{ 0, 2, 1, "INV_CLKADCI2" },
	{ 0, 1, 1, "STOP_CLKADCI1" },
	{ 0, 0, 1, "INV_CLKADCI1" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f1c3[] = {
	{ 0, 4, 1, "STOP_CLKSAMP2" },
	{ 0, 3, 1, "STOP_CLKSAMP1" },
	{ 0, 2, 1, "STOP_CLKVIT2" },
	{ 0, 1, 1, "STOP_CLKVIT1" },
	{ 0, 0, 1, "STOP_CLKTS" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f1e1[] = {
	{ 0, 5, 1, "DISEQC1_PON" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f201[] = {
	{ 0, 5, 1, "DUMMYPL_NOSDATA" },
	{ 0, 3, 3, "NOSPLH_BETA" },
	{ 0, 0, 7, "NOSDATA_BETA" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f206[] = {
	{ 0, 7, 1, "AGC1_LOCKED" },
	{ 0, 4, 1, "AGC1_MINPOWER" },
	{ 0, 3, 1, "AGCOUT_FAST" },
	{ 0, 0, 7, "AGCIQ_BETA" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f211[] = {
	{ 0, 7, 1, "MANUAL_MODCOD" },
	{ 0, 2,31, "DEMOD_MODCOD" },
	{ 0, 0, 3, "DEMOD_TYPE" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f212[] = {
	{ 0, 7, 1, "CAR_LOCK" },
	{ 0, 5, 3, "TMGLOCK_QUALITY" },
	{ 0, 3, 1, "LOCK_DEFINITE" },
	{ 0, 0, 1, "OVADC_DETECT" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f213[] = {
	{ 0, 7, 1, "DEMOD_DELOCK" },
	{ 0, 3, 3, "AGC1_NOSIGNALACK" },
	{ 0, 2, 1, "AGC2_OVERFLOW" },
	{ 0, 1, 1, "CFR_OVERFLOW" },
	{ 0, 0, 1, "GAMMA_OVERUNDER" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f214[] = {
	{ 0, 7, 1, "DVBS2_ENABLE" },
	{ 0, 6, 1, "DVBS1_ENABLE" },
	{ 0, 4, 1, "SCAN_ENABLE" },
	{ 0, 3, 1, "CFR_AUTOSCAN" },
	{ 0, 0, 3, "TUN_RNG" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f215[] = {
	{ 0, 6, 1, "S1S2_SEQUENTIAL" },
	{ 0, 4, 1, "INFINITE_RELOCK" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f216[] = {
	{ 0, 0,31, "DEMOD_MODE" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f21e[] = {
	{ 0, 3, 1, "NOSTOP_FIFOFULL" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f21f[] = {
	{ 0, 3, 1, "TUNER_NRELAUNCH" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f224[] = {
        { 0, 7, 1, "SPECINV_DEMOD" },
	{ 0, 2,31, "PLH_MODCOD" },
	{ 0, 0, 3, "PLH_TYPE" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f238[] = {
        { 0, 7, 1, "CFRUPLOW_AUTO" },
	{ 0, 6, 1, "CFRUPLOW_TEST" },
	{ 0, 2, 1, "ROTAON" },
	{ 0, 0, 3, "PH_DET_ALGO" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f239[] = {
        { 0, 4, 3, "CAR_ALPHA_MANT" },
	{ 0, 0,15, "CAR_ALPHA_EXP" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f23a[] = {
        { 0, 4, 3, "CAR_BETA_MANT" },
	{ 0, 0,15, "CAR_BETA_EXP" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f23d[] = {
	{ 0, 4,15, "KC_COARSE_EXP" },
	{ 0, 0,15, "BETA_FREQ" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f250[] = {
	{ 0, 6, 3, "TMGLOCK_BETA" },
	{ 0, 4, 1, "DO_TIMING_CORR" },
	{ 0, 0, 3, "TMG_MINFREQ" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f251[] = {
	{ 0, 4,15, "TMGALPHAS_EXP" },
	{ 0, 0,15, "TMGBETAS_EXP" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f252[] = {
	{ 0, 4,15, "TMGALPHAS2_EXP" },
	{ 0, 0,15, "TMGBETAS2_EXP" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f259[] = {
	{ 0, 4,15, "SFR_SCANSTEP" },
	{ 0, 0,15, "SFR_CENTERSTEP" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f260[] = {
	{ 0, 7, 1, "AUTO_GUP" },
	{ 0, 0,127,"SYMB_FREQ_UP1" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f262[] = {
	{ 0, 7, 1, "AUTO_GLOW" },
	{ 0, 0,127,"SYMB_FREQ_LOW1" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f26d[] = {
	{ 0, 6, 3, "ROLLOFF_STATUS" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f290[] = {
	{ 0, 6, 1, "CARRIER3_DISABLE" },
	{ 0, 2, 1, "ROTA2ON" },
	{ 0, 0, 3, "PH_DET_ALGO2" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f291[] = {
	{ 0, 6, 3, "CFR2TOCFR1_DVBS1" },
	{ 0, 5, 1, "EN_S2CAR2CENTER" },
	{ 0, 4, 1, "DIS_BCHERRCFR2" },
	{ 0, 0, 7, "CFR2TOCFR1_BETA" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f297[] = {
	{ 0, 7, 1, "ENAB_SPSKSYMB" },
	{ 0, 4, 3, "CAR2S2_Q_ALPH_M" },
	{ 0, 0,15, "CAR2S2_Q_ALPH_E" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f298[] = {
	{ 0, 7, 1, "OLDI3Q_MODE" },
	{ 0, 4, 3, "CAR2S2_8_ALPH_M" },
	{ 0, 0,15, "CAR2S2_8_ALPH_M" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f29c[] = {
	{ 0, 4, 3, "CAR2S2_Q_BETA_M" },
	{ 0, 0,15, "CAR2S2_Q_BETA_M" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f29d[] = {
	{ 0, 4, 3, "CAR2S2_8_BETA_M" },
	{ 0, 0,15, "CAR2S2_8_BETA_M" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f2b7[] = {
	{ 0, 4,15, "DIS_8P_9_10" },
	{ 0, 0,15, "DIS_8P_8_9" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f2b8[] = {
	{ 0, 4,15, "DIS_8P_5_6" },
	{ 0, 0,15, "DIS_8P_3_4" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f2b9[] = {
	{ 0, 4,15, "DIS_8P_2_3" },
	{ 0, 0,15, "DIS_8P_3_5" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f2ba[] = {
	{ 0, 4,15, "DIS_QP_9_10" },
	{ 0, 0,15, "DIS_QP_8_9" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f2bb[] = {
	{ 0, 4,15, "DIS_QP_5_6" },
	{ 0, 0,15, "DIS_QP_4_5" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f2bc[] = {
	{ 0, 4,15, "DIS_QP_3_4" },
	{ 0, 0,15, "DIS_QP_2_3" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f2bd[] = {
	{ 0, 4,15, "DIS_QP_3_5" },
	{ 0, 0,15, "DIS_QP_1_2" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f2be[] = {
	{ 0, 4,15, "DIS_QP_2_5" },
	{ 0, 0,15, "DIS_QP_1_3" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f2bf[] = {
	{ 0, 4,15, "DIS_QP_1_4" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f2c0[] = {
	{ 0, 7, 1, "EN_CCIMODE" },
	{ 0, 0,127,"R0_GAUSSIEN" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f2c1[] = {
	{ 0, 7, 1, "CCIDETECT_PLHONLY" },
	{ 0, 0,127,"R0_CCI" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f2d8[] = {
	{ 0, 6, 1, "EQUALFFE_ON" },
	{ 0, 0, 7, "MU_EQUALFFE" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f2e0[] = {
	{ 0, 7, 1, "TUN_ACKFAIL" },
	{ 0, 4, 7, "TUN_TYPE" },
	{ 0, 3, 1, "TUN_SECSTOP" },
	{ 0, 2, 1, "TUN_VCOSRCH" },
	{ 0, 0, 3, "TUN_MADDRESS" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f300[] = {
	{ 0, 7, 1, "DIS_QSCALE" },
	{ 0, 0,127,"SMAPCOEF_Q_LLR12" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f301[] = {
	{ 0, 2, 1, "ADJ_8PSKLLR1" },
	{ 0, 1, 1, "OLD_8PSKLLR1" },
	{ 0, 0, 1, "DIS_AB8PSK" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f302[] = {
	{ 0, 7, 1, "DIS_8SCALE" },
	{ 0, 0,127,"SMAPCOEF_8P_LLR23" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f333[] = {
	{ 0, 7, 1, "DSS_DVB" },
	{ 0, 4, 1, "DSS_SRCH" },
	{ 0, 1, 1, "SYNCVIT" },
	{ 0, 0, 1, "IQINV" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f33c[] = {
	{ 0, 6, 1, "DIS_VTHLOCK" },
	{ 0, 5, 1, "E7_8VIT" },
	{ 0, 4, 1, "E6_7VIT" },
	{ 0, 3, 1, "E5_6VIT" },
	{ 0, 2, 1, "E3_4VIT" },
	{ 0, 1, 1, "E2_3VIT" },
	{ 0, 0, 1, "E1_2VIT" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f33d[] = {
	{ 0, 7, 1, "AMVIT" },
	{ 0, 6, 1, "FROZENVIT" },
	{ 0, 4, 3, "SNVIT" },
	{ 0, 2, 3, "TOVVIT" },
	{ 0, 0, 3, "HYPVIT" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f33e[] = {
	{ 0, 4, 1, "PRFVIT" },
	{ 0, 3, 1, "LOCKEDVIT" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f369[] = {
	{ 0, 7, 1, "PKTDELIN_DELOCK" },
	{ 0, 6, 1, "SYNCDUPDFL_BADDFL" },
	{ 0, 5, 1, "CONTINUOUS_STREAM" },
	{ 0, 4, 1, "UNACCEPTED_STREAM" },
	{ 0, 3, 1, "BCH_ERROR_FLAG" },
	{ 0, 1, 1, "PKTDELIN_LOCK" },
	{ 0, 0, 1, "FIRST_LOCK" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f36a[] = {
	{ 0, 3,31, "FRAME_MODCOD" },
	{ 0, 0, 3, "FRAME_TYPE" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f370[] = {
	{ 0, 7, 1, "TSDIL_ON" },
	{ 0, 6, 1, "TSRS_ON" },
	{ 0, 4, 1, "TSDESCRAMB_ON" },
	{ 0, 3, 1, "TSFRAME_MODE" },
	{ 0, 2, 1, "TS_DISABLE" },
	{ 0, 0, 1, "TSOUT_NOSYNC" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f372[] = {
	{ 0, 7, 1, "TSFIFO_DVBCI" },
	{ 0, 6, 7, "TSFIFO_SERIAL" },
	{ 0, 5, 1, "TSFIFO_TEIUPDATE" },
	{ 0, 4, 1, "TSFIFO_DUTY50" },
	{ 0, 3, 1, "TSFIFO_HSGNLOUT" },
	{ 0, 1, 3, "TSFIFO_ERRMODE" },
	{ 0, 0, 1, "RST_HWARE" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f381[] = {
	{ 0, 7, 1, "TSFIFO_LINEOK" },
	{ 0, 6, 1, "TSFIFO_ERROR" },
	{ 0, 0, 1, "DIL_READY" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f398[] = {
	{ 0, 7, 1, "ERR_SOURCE1" },
	{ 0, 0,127,"NUM_EVENT1" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f39d[] = {
	{ 0, 7, 1, "OLDVALUE" },
	{ 0, 0,127,"VAL" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f3a0[] = {
	{ 0, 7, 1, "SPY_ENABLE" },
	{ 0, 6, 1, "NO_SYNCBYTE" },
	{ 0, 5, 1, "SERIAL_MODE" },
	{ 0, 4, 1, "UNUSUAL_PACKET" },
	{ 0, 3, 1, "BERMETER_DATAMODE" },
	{ 0, 1, 1, "BERMETER_LMODE" },
	{ 0, 0, 1, "BERMETER_RESET" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f3a4[] = {
	{ 0, 7, 1, "SPY_ENDSIM" },
	{ 0, 6, 1, "VALID_SIM" },
	{ 0, 5, 1, "FOUND_SIGNAL" },
	{ 0, 4, 1, "DSS_SYNCBYTE" },
	{ 0, 0,15, "RESULT_STATE" },
	{ 0, 0, 0, NULL }
};

static struct regdmp f332[] = {
	{ 0, 7, 1, "NVTH_NOSRANGE" },
	{ 0, 6, 1, "VERROR_MAXMODE" },
	{ 0, 3, 1, "NSLOWSN_LOCKED" },
	{ 0, 1, 1, "DIS_RSFLOCK" },
	{ 0, 0, 0, NULL }
};

static struct regdmp fa86[] = {
	{ 0, 4, 1, "BROADCAST" },
	{ 0, 1, 1, "PRIORITY" },
	{ 0, 0, 1, "DDEMOD" },
	{ 0, 0, 0, NULL }
};

static struct regdmp ff11[] = {
	{ 0, 7, 1, "FRESFEC" },
	{ 0, 0, 0, NULL }
};

static struct reg reg_tbl[] = {
	{ 0xf100, 0, f100, "MID" },
	{ 0xf129, 0, f129, "I2CCFG" },
	{ 0xf12a, 0, f12a, "P1_I2CRPT" },
	{ 0xf12b, 0, f12a, "P2_I2CRPT" },
	{ 0xf142, 0, conf, "GPIO2CFG" },
	{ 0xf143, 0, conf, "GPIO3CFG" },
	{ 0xf144, 0, conf, "GPIO4CFG" },
	{ 0xf145, 0, conf, "GPIO5CFG" },
	{ 0xf146, 0, conf, "GPIO6CFG" },
	{ 0xf147, 0, conf, "GPIO7CFG" },
	{ 0xf148, 0, conf, "GPIO8CFG" },
	{ 0xf149, 0, conf, "GPIO9CFG" },
	{ 0xf14a, 0, conf, "GPIO10CFG" },
	{ 0xf14b, 0, conf, "GPIO11CFG" },
	{ 0xf14c, 0, conf, "GPIO12CFG" },
	{ 0xf14d, 0, conf, "GPIO13CFG" },
	{ 0xf152, 0, conf, "AGCRF1CFG" },
	{ 0xf156, 0, conf, "AGCRF2CFG" },
	{ 0xf15b, 0, conf, "ERROR1CFG" },
	{ 0xf15c, 0, conf, "DPN1CFG" },
	{ 0xf15d, 0, conf, "STROUT1CFG" },
	{ 0xf15e, 0, conf, "CLKOUT1CFG" },
	{ 0xf15f, 0, conf, "DATA71CFG" },
	{ 0xf160, 0, conf, "ERROR2CFG" },
	{ 0xf161, 0, conf, "DPN2CFG" },
	{ 0xf162, 0, conf, "STROUT2CFG" },
	{ 0xf163, 0, conf, "CLKOUT2CFG" },
	{ 0xf164, 0, conf, "DATA72CFG" },
	{ 0xf165, 0, conf, "ERROR3CFG" },
	{ 0xf166, 0, conf, "DPN3CFG" },
	{ 0xf167, 0, conf, "STROUT3CFG" },
	{ 0xf168, 0, conf, "CLKOUT3CFG" },
	{ 0xf169, 0, conf, "DATA73CFG" },
	{ 0xf16a, 0, f16a, "STRSTATUS1" },
	{ 0xf16b, 0, f16b, "STRSTATUS2" },
	{ 0xf16c, 0, f16c, "STRSTATUS3" },
	{ 0xf190, 0, f190, "P2_DISTXCTL" },
	{ 0xf197, 0, byte, "P2_DISTXDATA" },
	{ 0xf198, 0, f198, "P2_DISTXSTATUS" },
	{ 0xf199, 0, byte, "P2_F22TX" },
	{ 0xf19a, 0, byte, "P2_F22RX" },
	{ 0xf19d, 0, byte, "P2_ACRDIV" },
	{ 0xf1a0, 0, f190, "P1_DISTXCTL" },
	{ 0xf1a7, 0, byte, "P1_DISTXDATA" },
	{ 0xf1a8, 0, f198, "P1_DISTXSTATUS" },
	{ 0xf1a9, 0, byte, "P1_F22TX" },
	{ 0xf1aa, 0, byte, "P1_F22RX" },
	{ 0xf1ad, 0, byte, "P1_ACRDIV" },
	{ 0xf1b3, 0, byte, "NCOARSE" },
	{ 0xf1b6, 0, f1b6, "SYNTCTRL" },
	{ 0xf1c2, 0, f1c2, "STOPCLK1" },
	{ 0xf1c3, 0, f1c3, "STOPCLK2" },
	{ 0xf1e1, 0, f1e1, "TSTTNR2" },
	{ 0xf201, 0, f201, "NOSCFG" },
	{ 0xf206, 0, f206, "AGC1CN" },
	{ 0xf20e, 0, byte, "AGCIQIN1" },
	{ 0xf20f, 0, byte, "AGCIQIN0" },
	{ 0xf211, 0, f211, "DMDMODCOD" },
	{ 0xf212, 0, f212, "DSTATUS" },
	{ 0xf213, 0, f213, "DSTATUS2" },
	{ 0xf214, 0, f214, "DMDCFGMD" },
	{ 0xf215, 0, f215, "DMDCFG2" },
	{ 0xf216, 0, f216, "DMDISTATE" },
	{ 0xf217, 0, byte, "DMDT0M" },
	{ 0xf21e, 0, f21e, "DMDCFG3" },
	{ 0xf21f, 0, f21f, "DMDCFG4" },
	{ 0xf220, 0, byte, "CORRELMANT" },
	{ 0xf221, 0, byte, "CORRELABS" },
	{ 0xf222, 0, byte, "CORRELEXP" },
	{ 0xf224, 0, f224, "PLHMODCOD" },
	{ 0xf22d, 0, byte, "AGC2_REF" },
	{ 0xf238, 0, f238, "CARCFG" },
	{ 0xf239, 0, f239, "ACLC" },
	{ 0xf23a, 0, f23a, "BCLC" },
	{ 0xf23d, 0, f23d, "CARFREQ" },
	{ 0xf23e, 0, byte, "CARHDR/K_FREQ_HDR" },
	{ 0xf23f, 0, byte, "LDT/CARLOCK_THRES" },
	{ 0xf240, 0, byte, "LDT2/CARLOCK_THRES2" },
	{ 0xf242, 0, byte, "CFRUP1" },
	{ 0xf243, 0, byte, "CFRUP0" },
	{ 0xf246, 0, byte, "CFRLOW1" },
	{ 0xf247, 0, byte, "CFRLOW0" },
        { 0xf248, 0, byte, "CFRINIT1" },
        { 0xf249, 0, byte, "CFRINIT0" },
        { 0xf24c, 0, byte, "CFR2/CAR_FREQ2" },
        { 0xf24d, 0, byte, "CFR1/CAR_FREQ1" },
        { 0xf24e, 0, byte, "CFR0/CAR_FREQ0" },
        { 0xf250, 0, f250, "TMGCFG" },
        { 0xf251, 0, f251, "RTC" },
        { 0xf252, 0, f252, "RTCS2" },
        { 0xf253, 0, byte, "TMGTHRISE" },
        { 0xf254, 0, byte, "TMGTHFALL" },
        { 0xf255, 0, byte, "SFRUPRATIO" },
        { 0xf256, 0, byte, "SFRLOWRATIO" },
        { 0xf258, 0, byte, "KREFTMG" },
        { 0xf259, 0, f259, "SFRSTEP" },
        { 0xf25e, 0, byte, "SFRINIT1" },
        { 0xf25f, 0, byte, "SFRINIT0" },
        { 0xf260, 0, f260, "SFRUP1" },
        { 0xf261, 0, byte, "SFRUP0" },
        { 0xf262, 0, f262, "SFRLOW1" },
        { 0xf263, 0, byte, "SFRLOW0" },
        { 0xf268, 0, byte, "TMGREG2" },
        { 0xf269, 0, byte, "TMGREG1" },
        { 0xf26a, 0, byte, "TMGREG0" },
        { 0xf26b, 0, byte, "TMGLOCK1" },
        { 0xf26c, 0, byte, "TMGLOCK0" },
        { 0xf26d, 0, f26d, "TMGOBS" },
	{ 0xf280, 0, byte, "NNOSDATAT1" },
	{ 0xf281, 0, byte, "NNOSDATAT0" },
	{ 0xf284, 0, byte, "NNOSPLHT1" },
	{ 0xf285, 0, byte, "NNOSPLHT0" },
	{ 0xf290, 0, f290, "CAR2CFG" },
	{ 0xf291, 0, f291, "CAR2CFG1" },
	{ 0xf297, 0, f297, "ACLC2S2Q" },
	{ 0xf298, 0, f298, "ACLC2S28" },
	{ 0xf29c, 0, f29c, "BCLC2S2Q" },
	{ 0xf29d, 0, f29d, "BCLC2S28" },
	{ 0xf2b7, 0, f2b7, "MODCODLST7" },
	{ 0xf2b8, 0, f2b8, "MODCODLST8" },
	{ 0xf2b9, 0, f2b9, "MODCODLST9" },
	{ 0xf2ba, 0, f2ba, "MODCODLSTA" },
	{ 0xf2bb, 0, f2bb, "MODCODLSTB" },
	{ 0xf2bc, 0, f2bc, "MODCODLSTC" },
	{ 0xf2bd, 0, f2bd, "MODCODLSTD" },
	{ 0xf2be, 0, f2be, "MODCODLSTE" },
	{ 0xf2bf, 0, f2bf, "MODCODLSTF" },
	{ 0xf2c0, 0, f2c0, "GAUSSR0" },
	{ 0xf2c1, 0, f2c1, "CCIR0" },
	{ 0xf2d8, 0, f2d8, "FFECFG" },
	{ 0xf2e0, 0, f2e0, "TNRCFG" },
	{ 0xf300, 0, f300, "SMAPCOEF7" },
	{ 0xf301, 0, f301, "SMAPCOEF6" },
	{ 0xf302, 0, f302, "SMAPCOEF5" },
	{ 0xf333, 0, f333, "FECM" },
	{ 0xf334, 0, byte, "VTH12" },
	{ 0xf335, 0, byte, "VTH23" },
	{ 0xf336, 0, byte, "VTH34" },
	{ 0xf337, 0, byte, "VTH56" },
	{ 0xf338, 0, byte, "VTH67" },
	{ 0xf339, 0, byte, "VTH78" },
	{ 0xf33c, 0, f33c, "PRVIT" },
	{ 0xf33d, 0, f33d, "VAVSRVIT" },
	{ 0xf33e, 0, f33e, "VSTATUSVIT" },
	{ 0xf370, 0, f370, "TSSTATEM" },
	{ 0xf372, 0, f372, "TSCFGH" },
	{ 0xf369, 0, f369, "PDELSTATUS1" },
	{ 0xf36a, 0, f36a, "PDELSTATUS2" },
	{ 0xf380, 0, byte, "TSFIFO_OUTSPEED" },
	{ 0xf381, 0, f381, "TSSTATUS" },
	{ 0xf398, 0, f398, "ERRCTRL1" },
	{ 0xf399, 0, byte, "ERRCNT12" },
	{ 0xf39a, 0, byte, "ERRCNT11" },
	{ 0xf39b, 0, byte, "ERRCNT10" },
	{ 0xf39d, 0, f39d, "ERRCNT22" },
	{ 0xf39e, 0, byte, "ERRCNT21" },
	{ 0xf39f, 0, byte, "ERRCNT20" },
	{ 0xf3a0, 0, f3a0, "FECSPY" },
	{ 0xf3a4, 0, f3a4, "FSTATUS" },
	{ 0xf3ad, 0, byte, "FBERERR2" },
	{ 0xf3ae, 0, byte, "FBERERR1" },
	{ 0xf3af, 0, byte, "FBERERR0" },
	{ 0xf332, 0, f332, "VITSCALE" },
	{ 0xf600, 0, byte, "RCCFG2" },
	{ 0xfa43, 0, byte, "GAINLLR_NF4/QP_1_2" },
	{ 0xfa44, 0, byte, "GAINLLR_NF5/QP_3_5" },
	{ 0xfa45, 0, byte, "GAINLLR_NF6/QP_2_3" },
	{ 0xfa46, 0, byte, "GAINLLR_NF7/QP_3_4" },
	{ 0xfa47, 0, byte, "GAINLLR_NF8/QP_4_5" },
	{ 0xfa48, 0, byte, "GAINLLR_NF9/QP_5_6" },
	{ 0xfa49, 0, byte, "GAINLLR_NF10/QP_8_9" },
	{ 0xfa4a, 0, byte, "GAINLLR_NF11/QP_9_10" },
	{ 0xfa50, 0, byte, "GAINLLR_NF18/8P_9_10" },
	{ 0xfa86, 0, fa86, "GENCFG" },
	{ 0xff11, 0, ff11, "TSTRES0" },
	{ 0, 0, NULL, NULL }
};

static int
i2c_demod_valid(int addr)
{
	if (addr >= 0xf100 && addr < 0x10000)
		return 1;
	return 0;
}

static const char *
i2c_prefix(int addr)
{
	if (addr >= 0xf200 && addr < 0xf400)
		return "P2_";
	if (addr >= 0xf400 && addr < 0xf600)
		return "P1_";
	return "";
}

static int
i2c_line(int rd, int t1, int start, const char *s)
{
	static struct reg *old_rt[2] = { NULL, NULL };
	static int old_cmd[2] = { 0, 0 };
	int r, ptr, addr, addr2, cnt, d[16], val;
	struct reg *rt;
	struct regdmp *rtd;
	char buf[1024];

	r = sscanf(s + start, "%x, %d) %x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x",
	           &addr, &cnt,
	           &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7],
	           &d[8], &d[9], &d[10], &d[11], &d[12], &d[13], &d[14], &d[15]);
        if (r < 3 || cnt != r - 2)
        	return -1;
	if (addr < 0xd0 && addr > 0xd3)
		return -1;
	if (!rd)
		goto wr;

	if (old_rt[t1] == NULL)
		return -1;
	if (cnt < 1) {
		old_rt[t1] = NULL;
		return -1;
	}
	addr = old_cmd[t1];
	ptr = 0;
	do {
		val = d[ptr];
		addr2 = addr >= 0xf400 && addr < 0xf600 ? addr - 0x200 : addr;
		for (rt = reg_tbl; rt->name; rt++)
			if (rt->reg == addr2)
				break;
		addr++;
		if (rt == NULL) {
			if (s[0])
				printf("%s\n", s);
			s = "";
			continue;
		}
		snprintf(buf, sizeof(buf), "%-40s ; %s%s{%04x}", s,
		         i2c_prefix(addr - 1), rt->name, addr - 1);
		for (rtd = rt->dmp; rtd && rtd->name; rtd++) {
			if (strlen(buf) > PWIDTH) {
				printf("%s\n", buf);
				sprintf(buf, "%40s ;", "");
			}
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %s=0x%x",
				 rtd->name, (val >> rtd->shift) & rtd->mask);
			val &= ~(rtd->mask << rtd->shift);
		}
		if (val) {
			if (strlen(buf) > PWIDTH) {
				printf("%s\n", buf);
				sprintf(buf, "%40s ;", "");
			}
			snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " UNKNOWN=0x%x", val);
		}
		printf("%s\n", buf);
		s = "";
	} while (++ptr < cnt);
	old_rt[t1] = NULL;
	return 0;

wr:
	if (!i2c_demod_valid((d[0] << 8) | d[1]))
		return -1;
	if (cnt < 2)
		return -1;
	old_cmd[t1] = ((d[0] << 8) | d[1]) - 1;
	ptr = 2;
	do {
		addr = addr2 = ++old_cmd[t1];
		if (addr2 >= 0xf400 && addr2 < 0xf600)
			addr2 -= 0x200;
		for (rt = reg_tbl; rt->name; rt++)
			if (rt->reg == addr2)
				break;
		if (rt->name == NULL) {
			if (s[0])
				printf("%s\n", s);
			s = "";
			continue;
		}
		old_rt[t1] = rt;
		snprintf(buf, sizeof(buf), "%-40s ; %s%s{%04x}", s,
		         i2c_prefix(addr), rt->name, addr);
		if (ptr < cnt) {
			val = d[ptr];
			for (rtd = rt->dmp; rtd && rtd->name; rtd++) {
				if (strlen(buf) > PWIDTH) {
					printf("%s\n", buf);
					sprintf(buf, "%40s ;", "");
				}
				snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " %s=0x%x",
					 rtd->name, (val >> rtd->shift) & rtd->mask);
				val &= ~(rtd->mask << rtd->shift);
			}
			if (val) {
				if (strlen(buf) > PWIDTH) {
					printf("%s\n", buf);
					sprintf(buf, "%40s ;", "");
				}
				snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), " UNKNOWN=0x%x", val);
			}
		}
		printf("%s\n", buf);
		s = "";
	} while (++ptr < cnt);
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
			r = i2c_line(0, 0, 11, buf);
		else if (!strncmp(buf, "[i2c] read(", 11))
			r = i2c_line(1, 0, 11, buf);
		else if (!strncmp(buf, "[i2c] t1_w(", 11))
			r = i2c_line(0, 1, 11, buf);
		else if (!strncmp(buf, "[i2c] t1_r(", 11))
			r = i2c_line(1, 1, 11, buf);
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
	return cnt;
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
			printf("I2C read succeed for %s, addr 0x%02x: 0x%02x\n", path, a, v);
		close(i2c_fd);
		i2c_fd = -1;
	}
}

#define MAXARGV 32
#define MAXOPTS 32

static char *argv[MAXARGV];
static int argc;
static char *opts[MAXOPTS];
static int optc;

static int
find_opt(const char *name)
{
	int i;
	for (i = 0; i < optc; i++)
		if (strcmp(opts[i], name) == 0)
			return 1;
	return 0;
}

static void
parse_args(int _argc, char *_argv[])
{
	char *s;
	int i;

	argv[0] = _argv[0];
	argc = 1;
	for (i = 1; i < _argc; i++) {
		s = _argv[i];
		if (s[0] == '-') {
			s++;
			if (s[0] == '-')
				s++;
			if (optc < MAXOPTS)
				opts[optc++] = s;
		} else {
			if (argc < MAXARGV)
				argv[argc++] = s;
		}
	}
}

int
main(int _argc, char *_argv[])
{
	parse_args(_argc, _argv);
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
			int i, j;
			int a = strtol(argv[2], NULL, 0);
			int r = strtol(argv[3], NULL, 0);
			int c = argc > 4 ? strtol(argv[4], NULL, 0) : 1;
			u8 buf[16];
			char buf2[256];
			if (a > 0 && r > 0) {
				i = i2c_demod_reg_read(a, r, buf, c > sizeof(buf) ? sizeof(buf) : c);
				if (i < 0)
					printf("Unable to read register 0x%x from addr 0x%x\n", r, a);
				else {
					if (find_opt("decode")) {
						sprintf(buf2, "  > (%x, %d) %02x.%02x", a & ~1, 2, (r >> 8) & 0xff, r & 0xff);
						if (i2c_line(0, 0, 5, buf2) >= 0) {
							sprintf(buf2, "  < (%x, %d) ", a | 1, i);
							for (j = 0; j < i; j++)
								sprintf(buf2 + strlen(buf2), "%s%02x", j > 0 ? "." : "", buf[j]);
							if (i2c_line(1, 0, 5, buf2) >= 0)
								exit(EXIT_SUCCESS);
						}
					}
					if (!find_opt("decode") || find_opt("raw")) {
						for (j = 0; j < i; j++)
							printf("%s0x%02x", j > 0 ? ":" : "", buf[j]);
						printf("\n");
					}
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
