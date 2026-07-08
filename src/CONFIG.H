#ifndef _CONFIG_H_
#define _CONFIG_H_

#define SB16      1   /* 1=support SB16 */
#define ADPCM     1   /* 1=support ADPCM */
#define SETIF     1   /* 1=enable interrupts when ISR is entered (also see config.inc!) */
#define COMPAT4   1   /* 1=support /CF4 - mask PIT interrupt during snd interrupt */
#ifndef NOFM
#define REINITOPL 1   /* 1=reinit OPL during RESET */
#else
#define REINITOPL 0
#endif
#define SETABSVOL 0   /* 0=the master volume is set by /VOL cmdline option and isn't modified by the application */
#define VMPU      1   /* 1=virtualize MPU at port 0x330-0x331 if BLASTER contains P=xxx */
#define SLOWDOWN  1   /* 1=support /SD option */
#define SOUNDFONT 1   /* 1=support soundfonts if VMPU is on */
#define PT0V86    1   /* 1=support read access to v86 page table 0 */
#define HOSTRT    0   /* 0=protect SB irq, so it's not modified by programs; must match setting in SBISR.ASM */

#define HW_CHANNELS 2
#define HW_BITS 16

#if SOUNDFONT
#define VOICES_DEFAULT 64
#endif

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
	int freq;
	int pin;
	int device;
	int period_size;
#ifdef _DEBUG
	char *logfile;
#endif
};

/* compatflags bits */
enum {
    CF_INSTANTIRQ = 1, /* "instant" SB interrupt triggered on DSP cmd 0xF2 */
    CF_STIONEOI   = 2, /* run STI when OUT 20h,al (al=20h) is trapped */
    CF_MASKPIT    = 4, /* mask PIT during Snd interrupt */
    CF_NODIVERR   = 8, /* 16-bit pmode only: catch divide error (borland rtm bug) */
};
enum {
    OM_DOS = 1,
    OM_DIRECT = 2,
    OM_DEBUGGER = 4,
    OM_BUFFERED = 8,  /* debug version only */
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
int LogfileInit( void );
int LogfileExit( void );
#define LOGFILESIZE 0x1000000 /* size of logfile buffer (16 MB) */
#else
#define dbgprintf(x)
#define dbgcondbrk(x)
#endif

#endif//_CONFIG_H_
