//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2009 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: Ensoniq 1371/1373 low level routines (for SB PCI 16/64/128 cards)
//based on ALSA (http://www.alsa-project.org)

#include "sbemucfg.h"
#include "mpxplay.h"

#ifdef AU_CARDS_LINK_ES1371

#include "dmairq.h"
#include "pcibios.h"
#include "ac97_def.h"

#define ES1371_DMABUF_PERIODS  32
#define ES1371_MAX_CHANNELS     2
#define ES1371_MAX_BYTES        4
#define ES1371_DMABUF_ALIGN (ES1371_DMABUF_PERIODS*ES1371_MAX_CHANNELS*ES1371_MAX_BYTES) // 256

/* 00-07 interrupt/chip select
 * 08-0B UART
 * 0C-0F host interface	- memory page
 * 10-13 sample rate converter
 * 14-17 codec
 * 18-1F legacy
 * 20-2F serial interface
 * 30-3F host interface - memory
 */

#define POLL_COUNT      0xf000

#define ES_REG_CONTROL    0x00    /* R/W: Interrupt/Chip select control register */
#define  ES_1371_GPIO_OUT(o) (((o)&0x0f)<<16)/* GPIO out [3:0] pins - W/R */
#define  ES_1371_SYNC_RES    (1<<14)       /* Warm AC97 reset */
#define  ES_1371_PWR_INTRM   (1<<12)       /* interrupt mask for power mgmt level chnge */
#define  ES_1371_CCB_INTRM   (1<<10)       /* interrupt mask for CCB module voice */
#define  ES_DAC1_EN          (1<<6)        /* DAC1 playback channel enable */
#define  ES_1371_JYSTK_EN    (1<<2)        /* joystick enable */
#define  ES_1371_XTALCKDIS   (1<<1)        /* xtal.Clock disable */
#define  ES_1371_PCICLKDIS   (1<<0)        /* PCI clock disable */

#define ES_REG_STATUS    0x04     /* R/O: Interrupt/Chip select status register */
#define  ES_1371_ST_INTR        (1<<31)        /* 1=interrupt pending */
#define  ES_1371_ST_AC97_RST    (1<<29)        /* CT5880 AC'97 Reset bit */
#define  ES_1371_ST_UART        (1<<3)         /* 1=UART interrupt pending */
#define  ES_1371_ST_DAC1        (1<<2)         /* 1=DAC1 channel interrupt pending */
#define  ES_1371_ST_DAC2        (1<<1)         /* 1=DAC2 channel interrupt pending */
#define  ES_1371_ST_ADC         (1<<0)         /* 1=ADC channel interrupt pending */

#define ES_REG_UART_DATA    0x08    /* R/W: UART data in/out */
#define ES_REG_UART_CONTROL 0x09    /* W/O: UART control register */
#define ES_REG_UART_RES     0x0a    /* R/W: UART reserver register */

#define ES_REG_MEM_PAGE    0x0c     /* R/W: Memory page register (bits 0-3, ports 30-3F) */
#define  ES_MEM_PAGEO(o)    (((o)&0x0f)<<0)    /* memory page select - out */
#define  ES_P1_MODEM        (0x03<<0)    /* mask for above */

#define ES_REG_1371_SMPRATE 0x10 /* W/R: Sample Rate Converter (SRC) interface register */
#define  ES_1371_SRC_RAM_ADDRO(o) (((o)&0x7f)<<25)/* address of SRC RAM location */
#define  ES_1371_SRC_RAM_WE       (1<<24)    /* R/W: read/write control for accessing SRC RAM */
#define  ES_1371_SRC_RAM_BUSY     (1<<23)    /* R/O: 1=sample rate memory is busy */
#define  ES_1371_SRC_DISABLE      (1<<22)    /* R/W: 1=sample rate converter disable(d) */
#define  ES_1371_DIS_P1           (1<<21)    /* R/W: 1=playback channel 1 accumulator update disable(d) */
#define  ES_1371_DIS_P2           (1<<20)    /* R/W: 1=playback channel 2 accumulator update disable(d) */
#define  ES_1371_DIS_R1           (1<<19)    /* R/W: 1=capture channel accumulator update disable(d) */
#define  ES_1371_SRC_RAM_DATAO(o) (((o)&0xffff)<<0) /* R/W: data to write/read from the SRC RAM */


#define ES_REG_1371_CODEC 0x14    /* W/R: Codec Read/Write register address */
#define  ES_1371_CODEC_RDY        (1<<31)    /* R: 1=codec ready */
#define  ES_1371_CODEC_WIP        (1<<30)    /* R: 1=codec register access in progress */
#define  ES_1371_CODEC_PIRD       (1<<23)    /* RW: codec 0=read/1=write select register */
#define  ES_1371_CODEC_WRITE(a,d) ((((a)&0x7f)<<16)|(((d)&0xffff)<<0)) /* 16-22: address of register to be written/read */
#define  ES_1371_CODEC_READS(a)   ((((a)&0x7f)<<16)|ES_1371_CODEC_PIRD) 
#define  ES_1371_CODEC_READ(i)    (((i)>>0)&0xffff) /* data in bits 0-15; should be set to 0 for a register read */

#define ES_REG_1371_LEGACY 0x18    /* W/R: Legacy control/status register */

#define ES_REG_SERIAL    0x20    /* R/W: Serial interface control register */
#define  ES_ADC_LOOP_SEL    (1<<15)        /* ADC; 0 - loop mode; 1 = stop mode */
#define  ES_DAC2_LOOP_SEL   (1<<14)        /* DAC2; 0 - loop mode; 1 = stop mode */
#define  ES_P1_LOOP_SEL     (1<<13)        /* DAC1; 0 - loop mode; 1 = stop mode */
#define  ES_DAC2_PAUSE      (1<<12)        /* DAC2; 0 - play mode; 1 = pause mode */
#define  ES_P1_PAUSE        (1<<11)        /* DAC1; 0 - play mode; 1 = pause mode */
#define  ES_ADC_INT_EN      (1<<10)        /* ADC; 1=int enable */
#define  ES_DAC2_INT_EN     (1<<9)         /* DAC2; 1=int enable */
#define  ES_DAC1_INT_EN     (1<<8)         /* DAC1; 1=int enable */
#define  ES_P1_SCT_RLD      (1<<7)         /* force sample counter reload for DAC1 */
#define  ES_ADC_MODEO(o)    (((o)&0x03)<<4)    /* ADC mode; -- '' -- */
#define  ES_DAC2_MODEO(o)   (((o)&0x03)<<2)    /* DAC2 mode; -- '' -- */
#define  ES_P1_MODEO(o)     (((o)&0x03)<<0)    /* DAC1 mode; -- '' -- */


#define ES_REG_DAC1_COUNT 0x24    /* R/W: DAC1 sample count register; 00-0F=size-1, 10-1F)=curr */
#define ES_REG_DAC2_COUNT 0x28    /* R/W: DAC2 sample count register; 00-0F=size-1, 10-1F)=curr */
#define ES_REG_ADC_COUNT  0x2C    /* R/W: ADC sample count register; 00-0F=size-1, 10-1F)=curr */
#define ES_REG_DAC1_FRAME 0x30    /* R/W: PAGE 0x0c; DAC1 frame address */
#define ES_REG_DAC1_SIZE  0x34    /* R/W: PAGE 0x0c; DAC1 frame size */
#define  ES_REG_FCURR_COUNTI(i) (((i)>>14)&0x3fffc)

//Sample rate converter addresses
#define ES_SMPREG_DAC1        0x70
#define ES_SMPREG_DAC2        0x74
#define ES_SMPREG_ADC         0x78
#define ES_SMPREG_VOL_ADC     0x6c
#define ES_SMPREG_VOL_DAC1    0x7c
#define ES_SMPREG_VOL_DAC2    0x7e

#define ES_SMPREG_TRUNC_N     0x00
#define ES_SMPREG_INT_REGS    0x01
#define ES_SMPREG_VFREQ_FRAC  0x03

#define ES_PAGE_DAC    0x0c

#define ES1371REV_CT5880_A  0x07
#define CT5880REV_CT5880_C  0x02
#define CT5880REV_CT5880_D  0x03
#define CT5880REV_CT5880_E  0x04
#define ES1371REV_ES1373_8  0x08

#define ENSONIQ_CARD_INFOBIT_AC97RESETHACK 0x01

typedef struct ensoniq_card_s
{
 unsigned long   infobits;
 unsigned long   port;
 unsigned int    irq;
 unsigned int    chiprev;
 struct pci_config_s  *pci_dev;

 cardmem_t *dm;
 char *pcmout_buffer;
 long pcmout_bufsize;

 unsigned long ctrl;
 unsigned long sctrl;
 unsigned long cssr;
}ensoniq_card_s;

// low level write & read

static unsigned int snd_es1371_wait_src_ready(struct ensoniq_card_s *card)
//////////////////////////////////////////////////////////////////////////
{
	unsigned int t, r = 0;

	for (t = 0; t < POLL_COUNT; t++) {
		r = inl(card->port + ES_REG_1371_SMPRATE);
		if ((r & ES_1371_SRC_RAM_BUSY) == 0)
			return r;
        pds_delay_10us(10);
	}
	dbgprintf("wait_src_ready: timeout, smprate=%X\n", r);
	/* better return a valid register value than just 0; the returned value
     * may be used by the caller to "restore" the register...
	 */
	return r;
}

/* it's undocumented what purpose bit 16 has ( neither is bit 17/18 explained ) */

static unsigned int snd_es1371_src_read(struct ensoniq_card_s *card, unsigned short reg)
////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int temp, i, orig, r;

	// wait for ready
	temp = orig = snd_es1371_wait_src_ready(card);

	// expose the SRC state bits
	r = temp & (ES_1371_SRC_DISABLE | ES_1371_DIS_P1 |
				ES_1371_DIS_P2 | ES_1371_DIS_R1);
	r |= ES_1371_SRC_RAM_ADDRO(reg) | 0x10000;
	outl((card->port + ES_REG_1371_SMPRATE), r);

	// now, wait for busy and the correct time to read
	temp = snd_es1371_wait_src_ready(card);

	if((temp & 0x00870000) != 0x00010000){
		// wait for the right state
		for(i = 0; i < POLL_COUNT; i++){
			temp = inl(card->port + ES_REG_1371_SMPRATE);
			if((temp & 0x00870000) == 0x00010000)
				break;
		}
#ifdef _DEBUG
		if(i == POLL_COUNT)
			dbgprintf("src_read timeout r=%8X\n",temp);
#endif
	}

	// hide the state bits
	r = orig & (ES_1371_SRC_DISABLE | ES_1371_DIS_P1 | ES_1371_DIS_P2 | ES_1371_DIS_R1);
	r |= ES_1371_SRC_RAM_ADDRO(reg);
	outl((card->port + ES_REG_1371_SMPRATE), r);

	return temp;
}

static void snd_es1371_src_write(struct ensoniq_card_s * card, unsigned short reg, unsigned short data)
///////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int r;

	r = snd_es1371_wait_src_ready(card) &
		(ES_1371_SRC_DISABLE | ES_1371_DIS_P1 |
		 ES_1371_DIS_P2 | ES_1371_DIS_R1);
	r|= ES_1371_SRC_RAM_ADDRO(reg) | ES_1371_SRC_RAM_DATAO(data);
	outl((card->port + ES_REG_1371_SMPRATE), (r | ES_1371_SRC_RAM_WE));
}

static void snd_es1371_codec_write(struct ensoniq_card_s *card, unsigned short reg, unsigned short val)
///////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int t, x;

	dbgprintf("codec_write begin reg:%8X val:%8X\n",reg,val);

	for (t = 0; t < 1000; t++) {
		if (!(inl(card->port + ES_REG_1371_CODEC) & ES_1371_CODEC_WIP)) {

			/* save/restore the current SRC state.
			 * Why is the SRC involved here at all?
			 */
			x = snd_es1371_wait_src_ready(card);
			outl((card->port + ES_REG_1371_SMPRATE), (x & (ES_1371_SRC_DISABLE | ES_1371_DIS_P1 | ES_1371_DIS_P2 | ES_1371_DIS_R1)) | 0x00010000 );
			// wait for not busy (state 0) first to avoid transition states
			for (t = 0; t < POLL_COUNT; t++) {
				if ((inl(card->port + ES_REG_1371_SMPRATE) & 0x00870000) == 0x00000000)
					break;
			}
			// wait for a SAFE time to write addr/data and then do it, dammit
			for (t = 0; t < POLL_COUNT; t++) {
				if ((inl(card->port + ES_REG_1371_SMPRATE) & 0x00870000) == 0x00010000)
					break;
			}
			/* write CODEC word */
			outl((card->port + ES_REG_1371_CODEC),ES_1371_CODEC_WRITE(reg, val));

			/* restore SRC reg */
			snd_es1371_wait_src_ready(card);
			outl((card->port + ES_REG_1371_SMPRATE), x);

			return;
		}
		pds_delay_10us(10);
	}
	dbgprintf("codec_write: timeout (poll_count) r=%X\n",inl(card->port + ES_REG_1371_CODEC));
}

static unsigned short snd_es1371_codec_read(struct ensoniq_card_s *card, unsigned short reg)
////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int t, x;

	for(t = 0; t < 1000; t++) {
		if(!(inl(card->port + ES_REG_1371_CODEC) & ES_1371_CODEC_WIP)){

			/* save/restore the current SRC state */
			x = snd_es1371_wait_src_ready(card);
			outl((card->port + ES_REG_1371_SMPRATE) ,(x & (ES_1371_SRC_DISABLE | ES_1371_DIS_P1 | ES_1371_DIS_P2 | ES_1371_DIS_R1)) | 0x00010000);
			// wait for not busy (state 0) first to avoid transition states
			for(t = 0; t < POLL_COUNT; t++){
				if((inl(card->port + ES_REG_1371_SMPRATE) & 0x00870000) == 0x00000000)
					break;
			}
			// wait for a SAFE time to write addr/data and then do it, dammit
			for(t = 0; t < POLL_COUNT; t++){
				if((inl(card->port + ES_REG_1371_SMPRATE) & 0x00870000) == 0x00010000)
					break;
			}

			/* select the CODEC register to read */
			outl((card->port + ES_REG_1371_CODEC) , ES_1371_CODEC_READS(reg));

			/* restore SRC reg */
			snd_es1371_wait_src_ready(card);
			outl((card->port + ES_REG_1371_SMPRATE), x);

			/* wait till WIP is clear */
			for( t = 0; t < POLL_COUNT; t++ ){
				if(!(inl(card->port + ES_REG_1371_CODEC) & ES_1371_CODEC_WIP))
					break;
				pds_delay_10us(10);
			}
			/* and finally wait till the CODEC data arrived (RDY) */
			for(t = 0; t < POLL_COUNT; t++){
				if((x = inl(card->port + ES_REG_1371_CODEC)) & ES_1371_CODEC_RDY){
					return ES_1371_CODEC_READ(x);
				}
			}
		}
		pds_delay_10us(10);
	}
	dbgprintf("codec_read: timeout (poll_count) r=%X\n",inl(card->port + ES_REG_1371_CODEC));
	return 0;
}

static void snd_es1371_adc_rate(struct ensoniq_card_s *card, unsigned int rate)
///////////////////////////////////////////////////////////////////////////////
{
	unsigned int n, truncm, freq, result;

	n = rate / 3000;
	if ((1 << n) & ((1 << 15) | (1 << 13) | (1 << 11) | (1 << 9)))
		n--;
	truncm = (21 * n - 1) | 1;
	freq = ((48000UL << 15) / rate) * n;
	result = (48000UL << 15) / (freq / n);
	if(rate >= 24000){
		if(truncm > 239)
			truncm = 239;
		snd_es1371_src_write(card, ES_SMPREG_ADC + ES_SMPREG_TRUNC_N,(((239 - truncm) >> 1) << 9) | (n << 4));
	}else{
		if(truncm > 119)
			truncm = 119;
		snd_es1371_src_write(card, ES_SMPREG_ADC + ES_SMPREG_TRUNC_N,0x8000 | (((119 - truncm) >> 1) << 9) | (n << 4));
	}
	snd_es1371_src_write(card, ES_SMPREG_ADC + ES_SMPREG_INT_REGS,(snd_es1371_src_read(card, ES_SMPREG_ADC + ES_SMPREG_INT_REGS) & 0x00ff) | ((freq >> 5) & 0xfc00));
	snd_es1371_src_write(card, ES_SMPREG_ADC + ES_SMPREG_VFREQ_FRAC, freq & 0x7fff);
	snd_es1371_src_write(card, ES_SMPREG_VOL_ADC, n << 8);
	snd_es1371_src_write(card, ES_SMPREG_VOL_ADC + 1, n << 8);
}

static void snd_es1371_dac1_rate(struct ensoniq_card_s *card, unsigned int rate)
////////////////////////////////////////////////////////////////////////////////
{
	unsigned int freq, r;

	freq = ((rate << 15) + 1500) / 3000;
	r = (snd_es1371_wait_src_ready(card) & (ES_1371_SRC_DISABLE |ES_1371_DIS_P2 | ES_1371_DIS_R1)) | ES_1371_DIS_P1;
	outl((card->port + ES_REG_1371_SMPRATE), r);
	snd_es1371_src_write(card, ES_SMPREG_DAC1 + ES_SMPREG_INT_REGS,
						 (snd_es1371_src_read(card, ES_SMPREG_DAC1 +
											  ES_SMPREG_INT_REGS) & 0x00ff) |
						 ((freq >> 5) & 0xfc00));
	snd_es1371_src_write(card, ES_SMPREG_DAC1 + ES_SMPREG_VFREQ_FRAC, freq & 0x7fff);
	r = (snd_es1371_wait_src_ready(card) & (ES_1371_SRC_DISABLE | ES_1371_DIS_P2 | ES_1371_DIS_R1));
	outl((card->port + ES_REG_1371_SMPRATE), r);
}

static void snd_es1371_dac2_rate(struct ensoniq_card_s *card, unsigned int rate)
////////////////////////////////////////////////////////////////////////////////
{
	unsigned int freq, r;

	freq = ((rate << 15) + 1500) / 3000;
	r = (snd_es1371_wait_src_ready(card) & (ES_1371_SRC_DISABLE | ES_1371_DIS_P1 | ES_1371_DIS_R1)) | ES_1371_DIS_P2;
	outl((card->port + ES_REG_1371_SMPRATE), r);
	snd_es1371_src_write(card, ES_SMPREG_DAC2 + ES_SMPREG_INT_REGS,(snd_es1371_src_read(card, ES_SMPREG_DAC2 + ES_SMPREG_INT_REGS) & 0x00ff) | ((freq >> 5) & 0xfc00));
	snd_es1371_src_write(card, ES_SMPREG_DAC2 + ES_SMPREG_VFREQ_FRAC, freq & 0x7fff);
	r = (snd_es1371_wait_src_ready(card) & (ES_1371_SRC_DISABLE | ES_1371_DIS_P1 | ES_1371_DIS_R1));
	outl((card->port + ES_REG_1371_SMPRATE),r);
}

//-------------------------------------------------------------------------

static unsigned int snd_es1371_buffer_init(struct ensoniq_card_s *card,struct mpxplay_audioout_info_s *aui)
///////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int bytes_per_sample=2; // 16 bit
	card->pcmout_bufsize = MDma_get_max_pcmoutbufsize( aui, 0, ES1371_DMABUF_ALIGN, bytes_per_sample, 0);
	card->dm = MDma_alloc_cardmem( card->pcmout_bufsize );
	card->pcmout_buffer = (char *)card->dm->linearptr;
	aui->card_DMABUFF = card->pcmout_buffer;
	dbgprintf("buffer init: pcmout_buffer:%X size:%d\n",(unsigned long)card->pcmout_buffer,card->pcmout_bufsize);
	return 1;
}

static void snd_es1371_chip_init(struct ensoniq_card_s *card)
/////////////////////////////////////////////////////////////
{
	int idx;

	outl((card->port + ES_REG_CONTROL), card->ctrl);
	outl((card->port + ES_REG_SERIAL), card->sctrl);
	outl((card->port + ES_REG_1371_LEGACY), 0);
	if(funcbit_test(card->infobits,ENSONIQ_CARD_INFOBIT_AC97RESETHACK)){
		dbgprintf("chip_init: AC97 cold reset\n");
		outl((card->port + ES_REG_STATUS), card->cssr);
		pds_delay_10us(20*100);
		snd_es1371_wait_src_ready(card);
	}

	dbgprintf("chip_init: AC97 warm reset\n");
	outl((card->port + ES_REG_CONTROL), (card->ctrl | ES_1371_SYNC_RES));
	inl(card->port + ES_REG_CONTROL);
	pds_delay_10us(3);
	outl((card->port + ES_REG_CONTROL), card->ctrl);
	snd_es1371_wait_src_ready(card);

	dbgprintf("chip_init: sample rate converter init\n");
	outl((card->port + ES_REG_1371_SMPRATE), ES_1371_SRC_DISABLE);
	for (idx = 0; idx < 0x80; idx++)
		snd_es1371_src_write(card, idx, 0);
	snd_es1371_src_write(card, ES_SMPREG_DAC1 + ES_SMPREG_TRUNC_N, 16 << 4);
	snd_es1371_src_write(card, ES_SMPREG_DAC1 + ES_SMPREG_INT_REGS, 16 << 10);
	snd_es1371_src_write(card, ES_SMPREG_DAC2 + ES_SMPREG_TRUNC_N, 16 << 4);
	snd_es1371_src_write(card, ES_SMPREG_DAC2 + ES_SMPREG_INT_REGS, 16 << 10);
	snd_es1371_src_write(card, ES_SMPREG_VOL_ADC, 1 << 12);
	snd_es1371_src_write(card, ES_SMPREG_VOL_ADC + 1, 1 << 12);
	snd_es1371_src_write(card, ES_SMPREG_VOL_DAC1, 1 << 12);
	snd_es1371_src_write(card, ES_SMPREG_VOL_DAC1 + 1, 1 << 12);
	snd_es1371_src_write(card, ES_SMPREG_VOL_DAC2, 1 << 12);
	snd_es1371_src_write(card, ES_SMPREG_VOL_DAC2 + 1, 1 << 12);
	snd_es1371_adc_rate(card, 22050);
	snd_es1371_dac1_rate(card, 22050);
	snd_es1371_dac2_rate(card, 22050);
	snd_es1371_wait_src_ready(card);

    /* why is the SRC reset after it has been "initialized"? */

	dbgprintf("chip_init: SMPRATE reset\n");
	outl((card->port + ES_REG_1371_SMPRATE), 0);
	snd_es1371_wait_src_ready(card);

	dbgprintf("chip_init: CODEC reset\n");
	outl((card->port + ES_REG_1371_CODEC),  ES_1371_CODEC_WRITE(0, 0));
	snd_es1371_wait_src_ready(card);

	dbgprintf("chip_init: UART reset\n");
	outb((card->port + ES_REG_UART_CONTROL), 0x00);
	outb((card->port + ES_REG_UART_RES), 0x00);
	snd_es1371_wait_src_ready(card);

	dbgprintf("chip_init: STATUS reset\n");
	outl((card->port + ES_REG_STATUS), card->cssr);
	snd_es1371_wait_src_ready(card);

	dbgprintf("chip_init end\n");
}

static void snd_es1371_chip_close(struct ensoniq_card_s *card)
//////////////////////////////////////////////////////////////
{
	outl((card->port + ES_REG_CONTROL), 0);
	outl((card->port + ES_REG_SERIAL),  0);
}

static void snd_es1371_ac97_init(struct ensoniq_card_s *card)
/////////////////////////////////////////////////////////////
{
	snd_es1371_codec_write(card, AC97_MASTER_VOL_STEREO, 0x0404);
	snd_es1371_codec_write(card, AC97_PCMOUT_VOL,        0x0404);
	snd_es1371_codec_write(card, AC97_HEADPHONE_VOL,     0x0404);
	snd_es1371_codec_write(card, AC97_EXTENDED_STATUS,AC97_EA_SPDIF);
	dbgprintf("ac97 init end\n");
}

static void snd_es1371_prepare_playback(struct ensoniq_card_s *card,struct mpxplay_audioout_info_s *aui)
////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	funcbit_disable(card->ctrl, ES_DAC1_EN);
	outl((card->port + ES_REG_CONTROL), card->ctrl);
	outl((card->port + ES_REG_MEM_PAGE), ES_MEM_PAGEO(ES_PAGE_DAC));

	/* todo: the "card" should probably be feeded with the physical address!? */
	outl((card->port + ES_REG_DAC1_FRAME), (unsigned long) card->pcmout_buffer);

	outl((card->port + ES_REG_DAC1_SIZE), (aui->card_dmasize >> 2) - 1);
	funcbit_disable( card->sctrl, (ES_P1_LOOP_SEL|ES_P1_PAUSE|ES_P1_SCT_RLD|ES_P1_MODEM ));
	funcbit_enable( card->sctrl, ES_P1_MODEO(0x03) ); // stereo, 16 bits
	outl((card->port + ES_REG_SERIAL), card->sctrl);
	outl((card->port + ES_REG_DAC1_COUNT), (aui->card_dmasize >> 2) -1);
	outl((card->port + ES_REG_CONTROL), card->ctrl);
	snd_es1371_dac1_rate(card, aui->freq_card);
	dbgprintf("prepare playback end\n");
}

//-------------------------------------------------------------------------
static pci_device_s ensoniq_devices[]={
 //{"ES1370",0x1274,0x5000, 0}, // not supported/implemented
 {"ES1371",0x1274,0x1371, 0},
 {"ES1373",0x1274,0x5880, 0}, // CT5880
 {"EV1938",0x1102,0x8938, 0}, // Ectiva
 {NULL,0,0,0}
};

static pci_device_s amplifier_hack_devices[]={
 {" ",0x107b,0x2150, 0}, // Gateway Solo 2150
 {" ",0x13bd,0x100c, 0}, // EV1938 on Mebius PC-MJ100V
 {" ",0x1102,0x5938, 0}, // Targa Xtender300
 {" ",0x1102,0x8938, 0}, // IPC Topnote G notebook
 {NULL,0,0,0}
};

static void ES1371_close(struct mpxplay_audioout_info_s *aui);

static void ES1371_card_info(struct mpxplay_audioout_info_s *aui)
/////////////////////////////////////////////////////////////////
{
	struct ensoniq_card_s *card=aui->card_private_data;
	printf("ENS : Ensoniq %s found on port:%X irq:%d rev:%X\n",
		   card->pci_dev->device_name, card->port, card->irq, card->chiprev);
}

static int ES1371_adetect(struct mpxplay_audioout_info_s *aui)
//////////////////////////////////////////////////////////////
{
	struct ensoniq_card_s *card;

	card=(struct ensoniq_card_s *)pds_calloc(1,sizeof(struct ensoniq_card_s));
	if(!card)
		return 0;
	aui->card_private_data=card;

	card->pci_dev=(struct pci_config_s *)pds_calloc(1,sizeof(struct pci_config_s));
	if(!card->pci_dev)
		goto err_adetect;

	if(pcibios_search_devices(ensoniq_devices,card->pci_dev) != PCI_SUCCESSFUL)
		goto err_adetect;

	dbgprintf("chip init : enable PCI io and busmaster\n");
	pcibios_set_master( card->pci_dev );

	card->port = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NAMBAR);
	if(!card->port)
		goto err_adetect;
	card->irq = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_INTR_LN);
	card->chiprev= pcibios_ReadConfig_Byte(card->pci_dev, PCIR_RID);

	if((card->pci_dev->vendor_id==0x1274) &&
	   ( ((card->pci_dev->device_id==0x1371) && ((card->chiprev==ES1371REV_CT5880_A) || (card->chiprev==ES1371REV_ES1373_8)))
		||
		((card->pci_dev->device_id==0x5880) && ((card->chiprev==CT5880REV_CT5880_C) || (card->chiprev==CT5880REV_CT5880_D) || (card->chiprev==CT5880REV_CT5880_E)))
	   )
	  ){
		funcbit_enable(card->infobits,ENSONIQ_CARD_INFOBIT_AC97RESETHACK);
		funcbit_enable(card->sctrl,ES_1371_ST_AC97_RST);
	}

	if( pcibios_search_devices( amplifier_hack_devices, NULL ) == PCI_SUCCESSFUL)
		funcbit_enable(card->ctrl,ES_1371_GPIO_OUT(1)); // turn on amplifier

	card->port &= 0xfff0;
	dbgprintf("vend_id:%4X dev_id:%4X port:%X irq:%d rev:%2X info:%X\n",
			  card->pci_dev->vendor_id,card->pci_dev->device_id,card->port,card->irq,card->chiprev,card->infobits);

	if(!snd_es1371_buffer_init(card,aui))
		goto err_adetect;

	snd_es1371_chip_init(card);
	snd_es1371_ac97_init(card);

	return 1;

err_adetect:
	ES1371_close(aui);
	return 0;
}

static void ES1371_close(struct mpxplay_audioout_info_s *aui)
/////////////////////////////////////////////////////////////
{
	struct ensoniq_card_s *card=aui->card_private_data;
	if(card){
		snd_es1371_chip_close(card);
		MDma_free_cardmem(card->dm);
		if(card->pci_dev)
			pds_free(card->pci_dev);
		pds_free(card);
		aui->card_private_data = NULL;
	}
}

static void ES1371_setrate(struct mpxplay_audioout_info_s *aui)
///////////////////////////////////////////////////////////////
{
	struct ensoniq_card_s *card=aui->card_private_data;

	aui->card_wave_id=MPXPLAY_WAVEID_PCM_SLE;
	aui->chan_card=2;
	aui->bits_card=16;

	if(aui->freq_card<3000)
		aui->freq_card=3000;
	else if(aui->freq_card>48000)
		aui->freq_card=48000;

	MDma_init_pcmoutbuf(aui,card->pcmout_bufsize,ES1371_DMABUF_ALIGN,0);

	snd_es1371_prepare_playback(card,aui);
}

static void ES1371_start(struct mpxplay_audioout_info_s *aui)
/////////////////////////////////////////////////////////////
{
	struct ensoniq_card_s *card=aui->card_private_data;
	funcbit_enable(card->ctrl,ES_DAC1_EN);
	outl(card->port + ES_REG_CONTROL, card->ctrl);
	funcbit_disable(card->sctrl,ES_P1_PAUSE);
#ifdef SBEMU
	funcbit_enable(card->sctrl, ES_DAC1_INT_EN );
#endif
	outl(card->port + ES_REG_SERIAL, card->sctrl);
}

static void ES1371_stop(struct mpxplay_audioout_info_s *aui)
////////////////////////////////////////////////////////////
{
	struct ensoniq_card_s *card=aui->card_private_data;
	funcbit_enable(card->sctrl,ES_P1_PAUSE);
	outl(card->port + ES_REG_SERIAL, card->sctrl);
}

static long ES1371_getbufpos(struct mpxplay_audioout_info_s *aui)
/////////////////////////////////////////////////////////////////
{
	struct ensoniq_card_s *card=aui->card_private_data;
	unsigned long bufpos=0;
	if(inl(card->port + ES_REG_CONTROL) & ES_DAC1_EN) {
		outl((card->port + ES_REG_MEM_PAGE), ES_MEM_PAGEO(ES_PAGE_DAC));
		bufpos = ES_REG_FCURR_COUNTI(inl(card->port + ES_REG_DAC1_SIZE));
		if(bufpos<aui->card_dmasize)
			aui->card_dma_lastgoodpos=bufpos;
	}
	dbgprintf("bufpos:%d gpos:%d dmasize:%d\n",bufpos,aui->card_dma_lastgoodpos,aui->card_dmasize);

	return aui->card_dma_lastgoodpos;
}

//mixer

static void ES1371_writeMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg, unsigned long val)
///////////////////////////////////////////////////////////////////////////////////////////////////////
{
	struct ensoniq_card_s *card=aui->card_private_data;
	snd_es1371_codec_write(card,reg,val);
}

static unsigned long ES1371_readMIXER(struct mpxplay_audioout_info_s *aui,unsigned long reg)
////////////////////////////////////////////////////////////////////////////////////////////
{
	struct ensoniq_card_s *card=aui->card_private_data;
	return snd_es1371_codec_read(card,reg);
}

#ifdef SBEMU
static int ES1371_IRQRoutine(struct mpxplay_audioout_info_s *aui)
/////////////////////////////////////////////////////////////////
{
	struct ensoniq_card_s *card=aui->card_private_data;
	//dbgprintf("ES1371_IRQRoutine\n");
	int intmask = inl(card->port + ES_REG_STATUS );
	if ( intmask & ES_1371_ST_INTR ) {
		outl(card->port + ES_REG_STATUS , intmask ); //ack???
		return intmask;
	}
    return 0;
}
#endif

one_sndcard_info ES1371_sndcard_info={
 "ENS",
 SNDCARD_LOWLEVELHAND,

 NULL,
 NULL,                 // no init
 &ES1371_adetect,      // only autodetect
 &ES1371_card_info,
 &ES1371_start,
 &ES1371_stop,
 &ES1371_close,
 &ES1371_setrate,

 &MDma_writedata,
 &ES1371_getbufpos,
 &MDma_clearbuf,
#ifdef SBEMU
 NULL,
 &ES1371_IRQRoutine,
#else
 &MDma_interrupt_monitor,
 NULL,
#endif

 &ES1371_writeMIXER,
 &ES1371_readMIXER,
 &mpxplay_aucards_ac97chan_mixerset[0]
};

#endif // AUCARDS_LINK_ES1371