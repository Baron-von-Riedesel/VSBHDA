//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2008 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: SB Live24/Audigy LS (CA0106) low level routines
//based on the ALSA (http://www.alsa-project.org)

#include <stdint.h>
#include <stddef.h>
#ifndef DJGPP
#include <conio.h>
#else
#include <pc.h>
#define inpd inportl
#define outpd outportl
#endif

#define SPIDAC 1 /* support SPI DAC for Audigy SE */

#include "AUISTRUC.H"
#include "DEBUG.H"
#include "DMABUFF.H"
#include "PCIBIOS.H"
#include "TIMER.H"
#include "SC_SBGEN.H"
#include "AC97MIX.H"
#include "CA0106.H"

#define CA0106_DMABUF_PERIODS    8 // max
#define CA0106_PERIOD_ALIGN     64
#define CA0106_MAX_CHANNELS      8 // 7.1 (but we use 2 only)
#define CA0106_MAX_BYTES         4 // 32 bits
#define CA0106_DMABUF_ALIGN (CA0106_DMABUF_PERIODS*CA0106_PERIOD_ALIGN) // 512

#define AUDIGYLS_USE_AC97 1 // for Audigy LS cards (set it to 0, if it doesn't work with AC97)

#define CARD(name, rev, subsys, chips, maxchn) {name, subsys, rev, chips, maxchn}

#ifndef max
#define max(a,b) (((a)>(b))? (a):(b))
#endif

/* status of the SB1070 ( SB 5.1 VX )?
 * there are 2 MSI K8N Diamonds: 0x10091462 & 0x10091102 (Diamond Plus MB);
 * the latter one has a SPI DAC 0x4021!
 */
const struct emu_card_version_s emucards_07[] = {
 CARD("Audigy LS [SB0310]"         ,0,0x10021102,EMU_CHIPS_0106,8),
 CARD("Audigy LS [SB0310b]"        ,0,0x10051102,EMU_CHIPS_0106,8),
 CARD("Live! 7.1 24bit [SB0410]"   ,0,0x10061102,EMU_CHIPS_0106,8),
 CARD("Live! 7.1 24bit [SB0413]"   ,0,0x10071102,EMU_CHIPS_0106,8),
 CARD("Live24 (MSI K8N Diamond)"   ,0,0x10091462,EMU_CHIPS_0106,8), // SB0438
 CARD("Audigy SE [SB0570]"         ,0,0x100A1102,EMU_CHIPS_0106,8),
 CARD("Audigy SE OEM [SB0570a]"    ,0,0x10111102,EMU_CHIPS_0106,8),
 CARD("Live24 (Shuttle XPC SD31P)" ,0,0x30381297,EMU_CHIPS_0106,8),
 CARD("X-Fi Xtreme Audio [SB0790]" ,0,0x10121102,EMU_CHIPS_0106,8),
 CARD("Live! 7.1 24bit [unknown]"  ,0,0         ,EMU_CHIPS_0106,8), /* catch all */
 //{NULL}
};


static void snd_emu_ac97_write( struct emu10k1_card *card, unsigned int reg, unsigned int value)
////////////////////////////////////////////////////////////////////////////////////////////////
{
	outp(card->iobase + CA0106_AC97ADDRESS, reg);
	outpw(card->iobase + CA0106_AC97DATA, value);
	return;
}

#if AUDIGYLS_USE_AC97

static unsigned int snd_emu_ac97_read( struct emu10k1_card *card, unsigned int reg)
///////////////////////////////////////////////////////////////////////////////////
{
	outp(card->iobase + CA0106_AC97ADDRESS, reg);
	return inpw(card->iobase + CA0106_AC97DATA);
}

static void snd_emu_ac97_init( struct emu10k1_card *card)
/////////////////////////////////////////////////////////
{
	snd_emu_ac97_write(card, AC97_RESET, 0);
	snd_emu_ac97_read(card, AC97_RESET);

	snd_emu_ac97_write(card, AC97_MASTER_VOL_STEREO, 0x0202);
	snd_emu_ac97_write(card, AC97_PCMOUT_VOL,        0x0202);
	snd_emu_ac97_write(card, AC97_HEADPHONE_VOL,     0x0202);
	snd_emu_ac97_write(card, AC97_EXTENDED_STATUS,AC97_EA_SPDIF);
	return;
}

#else

static void snd_emu_ac97_mute( struct emu10k1_card *card)
/////////////////////////////////////////////////////////
{
	snd_emu_ac97_write(card, AC97_MASTER_VOL_STEREO, AC97_MUTE);
	return;
}

#endif // AUDIGYLS_USE_AC97

static unsigned int snd_ca0106_ptr_read( struct emu10k1_card *card,unsigned int reg,unsigned int chn)
/////////////////////////////////////////////////////////////////////////////////////////////////////
{
	outpd(card->iobase + CA0106_PTR, (reg << 16) | chn);
	return inpd(card->iobase + CA0106_DATA);
}

static void snd_ca0106_ptr_write( struct emu10k1_card *card,unsigned int reg,unsigned int chn,unsigned int data)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	outpd(card->iobase + CA0106_PTR, (reg << 16) | chn);
	outpd(card->iobase + CA0106_DATA, data);
	return;
}

#if SPIDAC

int snd_ca0106_spi_write(struct emu10k1_card *card, unsigned int data)
//////////////////////////////////////////////////////////////////////
{
	unsigned int reset, set;
	unsigned int reg, tmp;
	int n, result;
	reg = SPI;
	if (data > 0xffff) /* Only 16bit values allowed */
		return 1;
	tmp = snd_ca0106_ptr_read(card, reg, 0);
	reset = (tmp & ~0x3ffff) | 0x20000; /* Set xxx20000 */
	set = reset | 0x10000; /* Set xxx1xxxx */
	snd_ca0106_ptr_write(card, reg, 0, reset | data);
	tmp = snd_ca0106_ptr_read(card, reg, 0); /* write post */
	snd_ca0106_ptr_write(card, reg, 0, set | data);
	result = 1;
	/* Wait for status bit to return to 0 */
	for (n = 0; n < 100; n++) {
		pds_delay_10us(1);
		tmp = snd_ca0106_ptr_read(card, reg, 0);
		if (!(tmp & 0x10000)) {
			result = 0;
			break;
		}
	}
	if (result) {/* Timed out */
		dbgprintf(("snd_ca0106_spi_write(%X): timeout\n", data));
		return 1;
	}
	snd_ca0106_ptr_write(card, reg, 0, reset | data);
	tmp = snd_ca0106_ptr_read(card, reg, 0); /* Write post */
	return 0;
}

#endif

/* v2.1: snd_audigyls_selector() and snd_live24_selector() will now be called
 *       only for entries in emucards_07; hence EMU_CHIPS_0106 is ALWAYS true;
 */
static unsigned int snd_audigyls_selector( struct emu10k1_card *card, struct audioout_info_s *aui )
///////////////////////////////////////////////////////////////////////////////////////////////////
{
	//if((card->chips & EMU_CHIPS_0106) && ((card->serial == 0x10021102) || (card->serial == 0x10051102))){
	if( (card->serial == 0x10021102) || (card->serial == 0x10051102) ) {
		dbgprintf(("snd_audigyls_selector: Audigy LS detected\n"));
		return 1;
	}

	return 0;
}

static unsigned int snd_live24_selector( struct emu10k1_card *card, struct audioout_info_s *aui )
/////////////////////////////////////////////////////////////////////////////////////////////////
{
	/* variables card_capabilities->subsystem and card_serial are identical! */
	//if((card->chips & EMU_CHIPS_0106) && ((card->serial == 0x10061102) || (card->serial == 0x10071102) ||
	//     (card->serial==0x10091462) || (card->serial==0x30381297) || (card->serial == 0x10121102) ||
	//     !card->card_capabilities->subsystem)){
	if ( snd_audigyls_selector( card, aui ) )
		return 0;
	if( (card->serial == 0x100A1102) || (card->serial == 0x10111102) )
		card->spi_dac = 0x4021;
	dbgprintf(("snd_live24_selector: Live 24/Audigy LS detected, SPI DAC=%u\n", card->spi_dac));
	return 1;
}

#if SPIDAC

#define SPI_REG(reg, value)	(((reg) << SPI_REG_SHIFT) | (value))
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
static const unsigned int spi_dac_init[] = {
	SPI_REG(SPI_LDA1_REG,   SPI_DA_BIT_0dB), /* 0dB dig. attenuation */
	SPI_REG(SPI_RDA1_REG,   SPI_DA_BIT_0dB),
	SPI_REG(SPI_PL_REG,     SPI_PL_BIT_L_L | SPI_PL_BIT_R_R | SPI_IZD_BIT),
	SPI_REG(SPI_FMT_REG,    SPI_FMT_BIT_I2S | SPI_IWL_BIT_24),
	SPI_REG(SPI_LDA2_REG,   SPI_DA_BIT_0dB),
	SPI_REG(SPI_RDA2_REG,   SPI_DA_BIT_0dB),
	SPI_REG(SPI_LDA3_REG,   SPI_DA_BIT_0dB),
	SPI_REG(SPI_RDA3_REG,   SPI_DA_BIT_0dB),
	SPI_REG(SPI_MASTDA_REG, SPI_DA_BIT_0dB),
	SPI_REG(9,              0x00),
	SPI_REG(SPI_MS_REG,     SPI_DACD0_BIT | SPI_DACD1_BIT | SPI_DACD2_BIT),
	SPI_REG(12,             0x00),
	SPI_REG(SPI_LDA4_REG,   SPI_DA_BIT_0dB),
	SPI_REG(SPI_RDA4_REG,   SPI_DA_BIT_0dB | SPI_DA_BIT_UPDATE),
	SPI_REG(SPI_DACD4_REG,  SPI_DACD4_BIT),
};

static const int spi_dacd_reg[] = {
	SPI_DACD0_REG,
	SPI_DACD1_REG,
	SPI_DACD2_REG,
	0,
	SPI_DACD4_REG,
};
static const int spi_dacd_bit[] = {
	SPI_DACD0_BIT,
	SPI_DACD1_BIT,
	SPI_DACD2_BIT,
	0,
	SPI_DACD4_BIT,
};

/* for vsbhda, this array is static - just one instance running */
static unsigned short spi_dac_reg[16];

static int snd_ca0106_channel_dac(struct emu10k1_card *card, int channel_id)
////////////////////////////////////////////////////////////////////////////
{
	switch (channel_id) {
	case PCM_FRONT_CHANNEL:
		return (card->spi_dac & 0xf000) >> (4 * 3);
	case PCM_REAR_CHANNEL:
		return (card->spi_dac & 0x0f00) >> (4 * 2);
	case PCM_CENTER_LFE_CHANNEL:
		return (card->spi_dac & 0x00f0) >> (4 * 1);
	case PCM_UNKNOWN_CHANNEL:
		return (card->spi_dac & 0x000f) >> (4 * 0);
	default:
		dbgprintf(("snd_ca0106_channel_dac: unknown channel_id %d\n", channel_id));
	}
	return 0;
}

static int snd_ca0106_pcm_power_dac(struct emu10k1_card *card, int channel_id, int power)
/////////////////////////////////////////////////////////////////////////////////////////
{
	if (card->spi_dac) {
		const int dac = snd_ca0106_channel_dac(card, channel_id);
		const int reg = spi_dacd_reg[dac];
		const int bit = spi_dacd_bit[dac];

		if (power)
			spi_dac_reg[reg] &= ~bit; /* Power up */
		else
			spi_dac_reg[reg] |= bit; /* Power down */
		if (snd_ca0106_spi_write(card, spi_dac_reg[reg]) != 0) {
			dbgprintf(("snd_ca0106_pcm_power_dac(chanID=%u, power=%u): spi write failed, dac=%X, reg=%X\n", channel_id, power, dac, reg));
			//return -ENXIO;
			return -6;
		}
	}
	return 0;
}

#endif

#define SPCS_INIT SPCS_CLKACCY_1000PPM | SPCS_SAMPLERATE_48 | SPCS_CHANNELNUM_LEFT | SPCS_SOURCENUM_UNSPEC | \
    SPCS_GENERATIONSTATUS | 0x00001200 | 0x00000000 | SPCS_EMPHASIS_NONE | SPCS_COPYRIGHT

/* init for all CA0106 cards */

static void snd_ca0106_hw_init( struct emu10k1_card *card, struct audioout_info_s *aui )
////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int ch;

	dbgprintf(("snd_ca0106_hw_init\n"));
	outpd(card->iobase + CA0106_INTE, 0);

	// Only SPCS1 has been tested
	snd_ca0106_ptr_write(card, SPCS0, 0, SPCS_INIT ); /* rear */
	snd_ca0106_ptr_write(card, SPCS1, 0, SPCS_INIT ); /* front */
	snd_ca0106_ptr_write(card, SPCS2, 0, SPCS_INIT ); /* center/LFE */
	snd_ca0106_ptr_write(card, SPCS3, 0, SPCS_INIT );

	dbgprintf(("snd_ca0106_hw_init: regs 63-66=%X %X %X %X, reg 6a=%X\n",
			snd_ca0106_ptr_read(card, 0x63, 0), snd_ca0106_ptr_read(card, 0x64, 0),
			snd_ca0106_ptr_read(card, 0x65, 0), snd_ca0106_ptr_read(card, 0x66, 0),
			snd_ca0106_ptr_read(card, 0x6a, 0) ));

	/* value 0x00fc0000 mutes, value 0 unmutes;
	 * playback will be unmuted in snd_pcm_prepare_playback()
	 */
	snd_ca0106_ptr_write(card, PLAYBACK_MUTE, 0, 0x00fc0000);
	snd_ca0106_ptr_write(card, CAPTURE_MUTE, 0, 0x00fc0000);

	outp(card->iobase + CA0106_AC97ADDRESS, AC97_RECORD_GAIN);
	outpw(card->iobase + CA0106_AC97DATA, 0x8000); // mute

	/* SPDIF_SELECT1, channel 0:
	 * bits 0-3: 0=16-bit,1=24-bit for 4 channels
	 * bits 8-11: 1=tristate for 4 channels
	 * bits 16-19: 1=bypass enable for 4 channels
	 */
	//snd_ca0106_ptr_write(card, SPDIF_SELECT1, 0, 0xf);

	/* SPDIF_SELECT2, channel 0 (only):
	 * analog: 0xb0000
	 * digital: 0xb000000
	 * ALSA sets 0xf0000
	 * fixme: what's bit 0?
	 */
	snd_ca0106_ptr_write(card, SPDIF_SELECT2, 0, (aui->card_outdev == 1) ? 0x0b000001 : 0x000b0001);
	//snd_ca0106_ptr_write(card, SPDIF_SELECT2, 0, 0x0f0f0000); // enable analog and spdif 7.1

	snd_ca0106_ptr_write(card, CAPTURE_CONTROL, 0, 0x40c81000); // goes to 0x40c80000 when doing SPDIF IN/OUT
	/* (Mute) CAPTURE feedback into PLAYBACK volume.
	 * Only lower 16 bits matter.
	 */
	snd_ca0106_ptr_write(card, CAPTURE_CONTROL, 1, 0xffffffff);

#if 0 /* fixme: writes to chn 2 & 3 in ALSA */
	/* SPDIF IN Volume */
	snd_ca0106_ptr_write(card, CAPTURE_CONTROL, 2, 0x30300000);
	/* SPDIF IN Volume, 0x70 = (vol & 0x3f) | 0x40 */
	snd_ca0106_ptr_write(card, CAPTURE_CONTROL, 3, 0x00700000);
#endif

	/* ??? does this work at all?
	 * CA0106.H says that this is the default; also done in ALSA
	 */
	snd_ca0106_ptr_write(card, PLAYBACK_ROUTING1, 0, 0x32765410);
	//snd_ca0106_ptr_write(card, PLAYBACK_ROUTING1, 0, 0x10765410);
	/* CA0106.H says that this is the default
	 */
	snd_ca0106_ptr_write(card, PLAYBACK_ROUTING2, 0, 0x76767676);
#if 0 /* fixme: writes done in ALSA */
	snd_ca0106_ptr_write(card, CAPTURE_ROUTING1, 0, 0x32765410);
	snd_ca0106_ptr_write(card, CAPTURE_ROUTING2, 0, 0x76767676);
#endif
	/* playback volume 1/2 are ports 0x66/0x6a
	 * capture volume 1/2 are ports  0x61/0x62
	 * PLAYBACK_VOLUME1 is for SPDIF,
	 * PLAYBACK_VOLUME2 is for I2S (analog)
	 */

	/* mute unused channels */
	for(ch = 0; ch < 4; ch++) {
		snd_ca0106_ptr_write(card, CAPTURE_VOLUME1, ch, 0xffffffff); // Only high 16 bits matter
		snd_ca0106_ptr_write(card, CAPTURE_VOLUME2, ch, 0xffffffff); // mute ???
	}
	for(ch = 1; ch < 4; ch++) { // !!! we keep the playback volume of channel 0
		snd_ca0106_ptr_write(card, PLAYBACK_VOLUME1, ch, 0xffffffff); // Mute
		snd_ca0106_ptr_write(card, PLAYBACK_VOLUME2, ch, 0xffffffff); // Mute
	}
#if SPIDAC
	if (card->spi_dac) {
		/* The SB0570 use SPI to control DAC. */
		int size, n;

		size = ARRAY_SIZE(spi_dac_init);
		for (n = 0; n < size; n++) {
			int reg = spi_dac_init[n] >> SPI_REG_SHIFT;

			snd_ca0106_spi_write(card, spi_dac_init[n]);
			if (reg < ARRAY_SIZE(spi_dac_reg))
				spi_dac_reg[reg] = spi_dac_init[n];
		}

		/* Enable front dac only */
		snd_ca0106_pcm_power_dac(card, PCM_FRONT_CHANNEL, 1);
	}
#endif
	/* GPIO: caller (snd_audigyls_hw_init/snd_live24_hw_init) will set another value! */
	outpd(card->iobase + CA0106_GPIO, 0x0);
	return;
}

/* in ALSA there's a gpio_type variable with values:
 * 1: store 0x5f5301 in GPIO, analog, used by most cards
 * 2: ???
 * 3: store 0x5f03a3/0x5f02a2 [analog/spdif] in GPIO, used by 2 cards
 */

static void snd_audigyls_hw_init( struct emu10k1_card *card, struct audioout_info_s *aui )
//////////////////////////////////////////////////////////////////////////////////////////
{
	dbgprintf(("snd_audigyls_hw_init\n"));
	snd_ca0106_hw_init(card,aui);
	//outpd(card->iobase + CA0106_GPIO, 0x005f03a3); /* analog */
	//outpd(card->iobase + CA0106_GPIO, 0x005f02a2); /* SPDIF */
	/* v2.1: /O option 0=analog, 1=SPDIF */
	outpd(card->iobase + CA0106_GPIO, (aui->card_outdev == 1) ? 0x005f02a2 : 0x005f03a3);

	outpd(card->iobase + CA0106_HCFG, HCFG_AC97 | HCFG_AUDIOENABLE); // AC97 2.0, enable outputs

#if AUDIGYLS_USE_AC97
	snd_emu_ac97_init(card);
#else
	snd_emu_ac97_mute(card);
#endif
	return;
}

static void snd_live24_hw_init( struct emu10k1_card *card, struct audioout_info_s *aui )
////////////////////////////////////////////////////////////////////////////////////////
{
	dbgprintf(("snd_live24_hw_init\n"));
	snd_ca0106_hw_init(card,aui);
	//outpd(card->iobase + CA0106_GPIO, 0x005f5301); /* analog */
	//outpd(card->iobase + CA0106_GPIO, 0x005f5201); /* SPDIF */
	/* v2.1: /O option 0=analog, 1=SPDIF */
	outpd(card->iobase + CA0106_GPIO, (aui->card_outdev == 1) ? 0x005f5201 : 0x005f5301 );

	outpd(card->iobase + CA0106_HCFG, HCFG_AUDIOENABLE);
	return;
}

static void snd_hw_close( struct emu10k1_card *card)
////////////////////////////////////////////////////
{
	snd_ca0106_ptr_write(card, BASIC_INTERRUPT, 0, 0);
	outpd(card->iobase + CA0106_INTE, 0);
	outpd(card->iobase + CA0106_HCFG, 0);
	return;
}

static unsigned int snd_buffer_init( struct emu10k1_card *card, struct audioout_info_s *aui )
/////////////////////////////////////////////////////////////////////////////////////////////
{
	card->pcmout_bufsize = MDma_get_bufsize( aui, 0, aui->card_period_size ? aui->card_period_size : CA0106_DMABUF_ALIGN );
	if (! MDma_alloc_cardmem(&card->dm, CA0106_DMABUF_PERIODS * 2 * sizeof(uint32_t) + card->pcmout_bufsize) ) return 0;
	card->virtualpagetable = (uint32_t *)card->dm.pMem;
	card->pcmout_buffer = ((char *)card->virtualpagetable) + CA0106_DMABUF_PERIODS * 2 * sizeof(uint32_t);
	dbgprintf(("snd_buffer init: pagetable=%X pcmoutbuf=%X size=%u\n", card->virtualpagetable, card->pcmout_buffer, card->pcmout_bufsize));
	return 1;
}

static void snd_pcm_prepare_playback( struct emu10k1_card *card, struct audioout_info_s *aui )
//////////////////////////////////////////////////////////////////////////////////////////////
{
	const uint32_t channel = 0;
	uint32_t *table_base = card->virtualpagetable;
	uint32_t period_size_bytes = card->period_size;
	uint32_t physaddr;
	uint32_t i,reg40_set,reg71_set;

	/* reg 40 = BASIC_INTERRUPT */
	/* reg 71 = CAPTURE_CONTROL */

	switch(aui->freq_card){
	case 44100:
		reg40_set = 1;
		reg71_set = 0x01010000;
		break;
	case 96000:
		reg40_set = 2;
		reg71_set = 0x02020000;
		break;
	case 192000:
		reg40_set = 3;
		reg71_set = 0x03030000;
		break;
	default: // 48000
		reg40_set = 0;
		reg71_set = 0;
		break;
	}

	i = inpd(card->iobase + CA0106_HCFG); // control bit width
	if(aui->bits_card == 32)
		i |= HCFG_PLAYBACK_S32_LE;
	else
		i &= ~HCFG_PLAYBACK_S32_LE;
	outpd(card->iobase + CA0106_HCFG,i);

	/* reg40,chn0 bits:
	 * 16-23: frequency, 2 bits per channel 0=48k, 1=44.1k, 2=96k, 3=192k
	 */
	i = snd_ca0106_ptr_read(card, BASIC_INTERRUPT, 0); // control host to fifo
	i &= ~(3 << (16 + channel * 2));
	i |= reg40_set << ( 16 + channel * 2);
	snd_ca0106_ptr_write(card, BASIC_INTERRUPT, 0, i);

	/* reg71,chn0 bits:
	 * 0-1=output rate;
	 * 2-3=input rate;
	 * 8-9=record rate;
	 * 16-17:I2S output rate;
	 * 18=host/SRC;
	 * 24-25=SPDIF output rate;
	 */
	i = snd_ca0106_ptr_read(card, CAPTURE_CONTROL, 0); // control DAC rate (SPDIF)
	i = (i & (~0x03030000)) | reg71_set;
	snd_ca0106_ptr_write(card, CAPTURE_CONTROL, 0, i);

	/* MpxPlay uses DOS memory as buffer, assuming linear == physical addresses;
	 * this isn't true for VSBHDA; adjustments:
	 * - use physaddr instead of pcmout_buffer for pagetable entries
	 * - to set PLAYBACK_LIST_ADDR translate table_base to physical address
	 * - to set PLAYBACK_DMA_ADDR use physaddr
	 */
	physaddr = pds_cardmem_physicalptr( card->dm, card->pcmout_buffer );

	// build pagetable
	for(i = 0; i < CA0106_DMABUF_PERIODS; i++){
		table_base[i*2]   = physaddr + i * period_size_bytes;
		table_base[i*2+1] = period_size_bytes << 16;
	}

	snd_ca0106_ptr_write(card, PLAYBACK_LIST_ADDR, channel, pds_cardmem_physicalptr( card->dm, table_base ));
	snd_ca0106_ptr_write(card, PLAYBACK_LIST_SIZE, channel, (CA0106_DMABUF_PERIODS - 1) << 19);
	snd_ca0106_ptr_write(card, PLAYBACK_LIST_PTR, channel, 0);

	snd_ca0106_ptr_write(card, PLAYBACK_DMA_ADDR, channel, physaddr);
	/* fixme: ALSA driver writes both, period_size, then 0 */
	//snd_ca0106_ptr_write(card, PLAYBACK_PERIOD_SIZE, channel, period_size_bytes << 16);
	snd_ca0106_ptr_write(card, PLAYBACK_PERIOD_SIZE, channel, 0);
	snd_ca0106_ptr_write(card, PLAYBACK_POINTER, channel, 0);
	snd_ca0106_ptr_write(card, PLAYBACK_PERIOD_END_ADDR, channel, 0); /* reg 7 */
	snd_ca0106_ptr_write(card, PLAYBACK_FIFO_OFFSET_ADDRESS, channel, 0); /* reg 8 */
	snd_ca0106_ptr_write(card, PLAYBACK_MUTE, 0x0, 0x0); // unmute output

	dbgprintf(("snd_pcm_prepare_playback: regs 63-66=%X %X %X %X, reg 6a=%X, reg71=%X\n",
			snd_ca0106_ptr_read(card, 0x63, 0), snd_ca0106_ptr_read(card, 0x64, 0),
			snd_ca0106_ptr_read(card, 0x65, 0), snd_ca0106_ptr_read(card, 0x66, 0),
			snd_ca0106_ptr_read(card, 0x6a, 0), snd_ca0106_ptr_read(card, 0x71, 0) ));

	dbgprintf(("snd_pcm_prepare playback: exit\n"));
	return;
}

static void snd_setrate( struct emu10k1_card *card, struct audioout_info_s *aui )
/////////////////////////////////////////////////////////////////////////////////
{
	unsigned int dmabufsize;

	if(aui->bits_card > 16)
		aui->bits_card = 32;

    /* analog output with rate 44100 seems not possible with CA0106 */
    /* 44.1 only accepted for SPDIF (/O1) */
	if((aui->card_outdev == 1) && (0 == aui->freq_card % 11025))   // 11025, 22050 and 44100 -> 44100
		aui->freq_card = 44100;
	else {
		int multiple = max(1,aui->freq_card / 48000);
		aui->freq_card = 48000 * multiple;
	}

	/* v1.7: use /PS value if set */
	//dmabufsize = MDma_initbuf(aui, card->pcmout_bufsize, aui->card_period_size ? aui->card_period_size : CA0106_DMABUF_ALIGN, 0);
	dmabufsize = MDma_initbuf( aui, card->pcmout_bufsize );
	//card->period_size = (dmabufsize / CA0106_DMABUF_PERIODS);
	card->period_size = aui->card_period_size ? aui->card_period_size : (dmabufsize / CA0106_DMABUF_PERIODS);

	dbgprintf(("snd_setrate: dmasize:%d period_size:%d, freq=%u\n",dmabufsize,card->period_size, aui->freq_card));

	snd_pcm_prepare_playback(card,aui);
	return;
}

static void snd_pcm_start_playback( struct emu10k1_card *card)
//////////////////////////////////////////////////////////////
{
	const uint32_t channel = 0;
	snd_ca0106_ptr_write(card, EXTENDED_INT_MASK, 0, snd_ca0106_ptr_read(card, EXTENDED_INT_MASK, 0 ) | 0x00000010); /* full period interrupt */
	snd_ca0106_ptr_write(card, BASIC_INTERRUPT, 0, snd_ca0106_ptr_read(card, BASIC_INTERRUPT, 0) | (1 << channel));
	/* v2.1: added */
	outpd(card->iobase + CA0106_INTE, 0x100);
	dbgprintf(("snd_pcm_start_playback\n"));
	return;
}

static void snd_pcm_stop_playback( struct emu10k1_card *card)
/////////////////////////////////////////////////////////////
{
	const uint32_t channel = 0;
	/* v2.1: added */
	outpd(card->iobase + CA0106_INTE, 0);
	snd_ca0106_ptr_write(card, BASIC_INTERRUPT, 0, snd_ca0106_ptr_read(card, BASIC_INTERRUPT, 0) & ~(1 << channel));
	snd_ca0106_ptr_write(card, EXTENDED_INT_MASK, 0, snd_ca0106_ptr_read(card, EXTENDED_INT_MASK, 0 ) & ~0x00000010);
	dbgprintf(("snd_pcm_stop_playback\n"));
	return;
}

/* v2.1: now called directly by AU_cardbuf_space(), expecting pos in byte units! */

static unsigned int snd_pcm_pointer_playback( struct audioout_info_s *aui )
///////////////////////////////////////////////////////////////////////////
{
	struct emu10k1_card *card = aui->card_private_data;
	unsigned int ptr,ptr1,ptr3,ptr4;
	const uint32_t channel = 0;

	ptr3 = snd_ca0106_ptr_read(card, PLAYBACK_LIST_PTR, channel);
	ptr1 = snd_ca0106_ptr_read(card, PLAYBACK_POINTER, channel);
	ptr4 = snd_ca0106_ptr_read(card, PLAYBACK_LIST_PTR, channel);
	if(ptr3 != ptr4)
		ptr1 = snd_ca0106_ptr_read(card, PLAYBACK_POINTER, channel);

	/* entry in the virtualpagetable is 2 dwords */
	ptr4 /= ( 2 * sizeof(uint32_t));

	ptr = (ptr4 * card->period_size) + ptr1;

	dbgprintf(("snd_pcm_pointer_playback: bufpos:0x%X [list_ptr:%d period_ptr:%d]\n", ptr, ptr4, ptr1));

	/* AU_cardbuf_space() expects position in byte units! */
	//ptr /= aui->chan_card;
	//ptr /= aui->bits_card >> 3;

	return ptr;
}

/* Live 24 cards have no ac97
 * reg: bits 0-7: CA0106 register (0x66 or 0x6a)
 *           8-15: channel (0 or 3)
 */

static unsigned int snd_mixer_read( struct emu10k1_card *card, unsigned int reg)
////////////////////////////////////////////////////////////////////////////////
{
	unsigned int channel_id = reg >> 8;
#ifdef _DEBUG
	unsigned int value = snd_ca0106_ptr_read(card,reg & 0xff,channel_id);
	dbgprintf(("snd_mixer_read(reg=0x%X, chn=%u)=0x%X\n", reg, channel_id, value ));
	return value;
#else
	reg &= 0xff;
	return snd_ca0106_ptr_read(card,reg,channel_id);
#endif
}

static void snd_mixer_write( struct emu10k1_card *card, unsigned int reg, unsigned int value)
/////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int channel_id = reg >> 8;
	reg &= 0xff;
	dbgprintf(("snd_mixer_write(reg=0x%X, chn=%u, value=0x%X)\n", reg, channel_id, value ));
	snd_ca0106_ptr_write(card,reg,channel_id,value);
	return;
}

static int snd_isr( struct emu10k1_card *card )
///////////////////////////////////////////////
{
	unsigned int status;
	unsigned int stat76;

	status = inpd(card->iobase + CA0106_IPR );
	if (!status)
		return 0;

	//dbgprintf(("snd_isr: status=%X\n", status));

	/* v1.7: check if to use EXTENDED_INT instead of EXTENDED_INT_MASK.
	 * v2.1: EXTENDED_INT is used.
	 */
	stat76 = snd_ca0106_ptr_read(card, EXTENDED_INT, 0);
	snd_ca0106_ptr_write(card, EXTENDED_INT, 0, stat76); //ack
	outpd( card->iobase + CA0106_IPR, status ); //also ack here

	return stat76 | status;
}

static const struct aucards_mixerchan_s emu_analog_front = {
 AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME,2,
 {
  {((CONTROL_FRONT_CHANNEL << 8) | PLAYBACK_VOLUME2), 8, 24, SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {((CONTROL_FRONT_CHANNEL << 8) | PLAYBACK_VOLUME2), 8, 16, SUBMIXCH_INFOBIT_REVERSEDVALUE}
  //{((CONTROL_REAR_CHANNEL << 8) | PLAYBACK_VOLUME2), 8, 24, SUBMIXCH_INFOBIT_REVERSEDVALUE}, // !!! bad place
  //{((CONTROL_REAR_CHANNEL << 8) | PLAYBACK_VOLUME2), 8, 16, SUBMIXCH_INFOBIT_REVERSEDVALUE}
 }
};

static const struct aucards_mixerchan_s emu_spdif_front = {
 AU_MIXCHAN_SPDIFOUT,AU_MIXCHANFUNC_VOLUME,2,
 {
  {((CONTROL_FRONT_CHANNEL << 8) | PLAYBACK_VOLUME1), 8, 24, SUBMIXCH_INFOBIT_REVERSEDVALUE},
  {((CONTROL_FRONT_CHANNEL << 8) | PLAYBACK_VOLUME1), 8, 16, SUBMIXCH_INFOBIT_REVERSEDVALUE}
  //{((CONTROL_REAR_CHANNEL << 8) | PLAYBACK_VOLUME1), 8, 24, SUBMIXCH_INFOBIT_REVERSEDVALUE}, // !!! bad place
  //{((CONTROL_REAR_CHANNEL << 8) | PLAYBACK_VOLUME1), 8, 16, SUBMIXCH_INFOBIT_REVERSEDVALUE}
 }
};

static const struct aucards_mixerchan_s *emu_mixerset[] = {
 &emu_analog_front,
 &emu_spdif_front,
 NULL
};

/* function address vectors for SC_SBGEN.C */

const struct emu_driver_func_s emu_driver_live24_funcs = {
 &snd_live24_selector,
 &snd_live24_hw_init,
 &snd_hw_close,
 &snd_buffer_init,
 &snd_setrate,
 &snd_pcm_start_playback,
 &snd_pcm_stop_playback,
 &snd_pcm_pointer_playback,
 NULL,
 &snd_isr,
 &snd_mixer_read,
 &snd_mixer_write,
 emu_mixerset
};

const struct emu_driver_func_s emu_driver_audigyls_funcs = {
 &snd_audigyls_selector,
 &snd_audigyls_hw_init,
 &snd_hw_close,
 &snd_buffer_init,
 &snd_setrate,
 &snd_pcm_start_playback,
 &snd_pcm_stop_playback,
 &snd_pcm_pointer_playback,
 NULL,
 &snd_isr,
#if AUDIGYLS_USE_AC97
 &snd_emu_ac97_read,
 &snd_emu_ac97_write,
 aucards_ac97chan_mixerset
#else
 &snd_mixer_read,
 &snd_mixer_write,
 emu_mixerset
#endif
};

