#ifndef _CONFIG_H_
#define _CONFIG_H_

#define HW_CHANNELS 2
#define HW_BITS 16

#define SB16 1        /* 1=support SB16 */
#define ADPCM 1       /* 1=support ADPCM */
#define SETIF 1       /* 1=enable interrupts when ISR is entered (also see config.inc!) */
#define COMPAT4 1     /* 1=support /CF4 - mask PIT interrupt during snd interrupt */
#ifndef NOFM
#define REINITOPL 1   /* 1=reinit OPL during RESET */
#else
#define REINITOPL 0
#endif
#define SETABSVOL 0   /* 0=the master volume is set by /VOL cmdline option and isn't modified by the application */
#define VMPU 1        /* 1=virtualize MPU at port 0x330-0x331 if BLASTER contains P=xxx */
#define SLOWDOWN 1    /* 1=support /SD option */
#define DISPSTAT 1    /* 1=display VSB status */
#define SOUNDFONT 1   /* 1=support soundfonts if VMPU is on */
#if SOUNDFONT
#define VOICES_DEFAULT 64
#endif
#define PT0V86 1      /* 1=support read access to v86 page table 0 */

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
#if SOUNDFONT
	char *soundfont;
	int voices;
#endif
	int compatflags; /* compatibility flags */
	int pin;
	int device;
	int period_size;
};

/* compatflags bits */
enum {
    CF_INSTANTIRQ = 1,
};

int _dprintf(const char *fmt, ... );
int IsDebuggerPresent( void );
void SetBorder( uint8_t );
void fatal_error( int );

#ifdef __cplusplus
}
#endif

#ifdef _DEBUG
#define dbgprintf(x) _dprintf x
void dbgcondbrk(void *);
#else
#define dbgprintf(x)
#define dbgcondbrk(x)
#endif

#endif//_CONFIG_H_
