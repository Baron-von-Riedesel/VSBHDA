#ifndef _CONFIG_H_
#define _CONFIG_H_

#define HW_CHANNELS 2
#define HW_BITS 16

#define SB16 1        /* 1=support SB16 */
#define ADPCM 1       /* 1=support ADPCM */
#define SETIF 1       /* 1=enable interrupts when ISR is entered */
#define TRIGGERATONCE 1  /* 1=trigger IRQ from inside trap handler */
#define REINITOPL 1   /* 1=reinit OPL during RESET */
#define PREMAPDMA 0   /* 1=map full 16MB ISA address range */
#define SETABSVOL 0   /* 0=the master volume is set by /VOL cmdline option and isn't modified by the application */

struct globalvars {
	int base;
	int irq;
	int dma;
#if SB16
	int hdma;
#endif
	int type;
	int opl3;
	int rm;
	int pm;
	int vol;
	int pin;
	int device;
};

int dbgprintf(const char *fmt, ... );
#ifdef _DEBUG
#define dbgprintf dbgprintf
#else
#define dbgprintf
#endif

#endif//_CONFIG_H_
