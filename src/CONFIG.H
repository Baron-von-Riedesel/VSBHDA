#ifndef _CONFIG_H_
#define _CONFIG_H_

#define HW_CHANNELS 2
#define HW_BITS 16

#define SB16 1        /* 1=support SB16 */
#define ADPCM 1       /* 1=support ADPCM */
#define SETIF 1       /* 1=enable interrupts when ISR is entered */
#ifndef NOFM
#define REINITOPL 1   /* 1=reinit OPL during RESET */
#else
#define REINITOPL 0
#endif
#define SETABSVOL 0   /* 0=the master volume is set by /VOL cmdline option and isn't modified by the application */
#define VMPU 1        /* 1=virtualize MPU at port 0x330-0x331 if BLASTER contains P=xxx */
#define SLOWDOWN 1    /* 1=support /SD option */
//#define JHDPMI 0      /* 1=check if JLM jhdpmi is loaded */
#define DISPSTAT 1    /* 1=display VSB status */

#ifdef __cplusplus
extern "C"
{
#endif

struct globalvars {
	int base; /* /A value, default 220 */
	int irq;  /* /I value, default 7 */
	int dma;  /* /D value */
#if SB16
	int hdma; /* /H value */
#endif
	int type; /* /T value */
#if VMPU
	int mpu;  /* /P value, default 330 */
#endif
	int opl3;
	int rm;
	int pm;
	int vol;
	int buffsize;
#if SLOWDOWN
	int slowdown;
#endif
	int pin;
	int device;
};

int _dprintf(const char *fmt, ... );

#ifdef __cplusplus
}
#endif

#ifdef _DEBUG
#define dbgprintf(x) _dprintf x
#else
#define dbgprintf(x)
#endif

#endif//_CONFIG_H_
