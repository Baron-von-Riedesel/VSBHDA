#ifndef _SBEMUCFG_H_
#define _SBEMUCFG_H_

#define SBEMU_SAMPLERATE    22050
//#define SBEMU_SAMPLERATE    44100
#define SBEMU_CHANNELS 2
#define SBEMU_BITS 16

#define SB16 1        /* 1=support SB16 */
#define SBLIVE 1      /* 1=support SBLIVE/AUDIGY */
#define ADPCM 1       /* 1=support ADPCM */
#define SETIF 1       /* 1=enable interrupts when ISR is entered */
#define TRIGGERATONCE 1  /* 1=trigger IRQ from inside trap handler */
#define REINITOPL 1   /* 1=reinit OPL during RESET */
#define PREMAPDMA 0   /* 1=map full 16MB ISA address range */

struct globalvars {
	int base;
	int irq;
	int dma;
	int hdma;
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

#endif//_SBEMUCFG_H_
