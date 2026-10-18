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

#include "AUISTRUC.H"
#include "DEBUG.H"
#include "DMABUFF.H"
#include "PCIBIOS.H"
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

/* status of the SB1070 ( SB 5.1 VX )? */
const struct emu_card_version_s emucards_07[] = {
 CARD("Audigy LS [SB0310]"         ,0,0x10021102,EMU_CHIPS_0106,8),
 CARD("Audigy LS [SB0310b]"        ,0,0x10051102,EMU_CHIPS_0106,8),
 CARD("Live! 7.1 24bit [SB0410]"   ,0,0x10061102,EMU_CHIPS_0106,8),
 CARD("Live! 7.1 24bit [SB0413]"   ,0,0x10071102,EMU_CHIPS_0106,8),
 CARD("Live24 (MSI K8N Diamond)"   ,0,0x10091462,EMU_CHIPS_0106,8), // SB0438
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

	dbgprintf(("snd_live24_selector: Live 24 detected\n"));
	return 1;
}

#define SPCS_INIT SPCS_CLKACCY_1000PPM | SPCS_SAMPLERATE_48 | SPCS_CHANNELNUM_LEFT | SPCS_SOURCENUM_UNSPEC | \
    SPCS_GENERATIONSTATUS | 0x00001200 | 0x00000000 | SPCS_EMPHASIS_NONE | SPCS_COPYRIGHT

/* init for all CA0106 cards */

static void snd_ca0106_hw_init( struct emu10k1_card *card)
//////////////////////////////////////////////////////////
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

	snd_ca0106_ptr_write(card, SPDIF_SELECT1, 0, 0xf);

	/* fixme: value for SPDIF_SELECT2 in ALSA driver: 0xf0000 */
	snd_ca0106_ptr_write(card, SPDIF_SELECT2, 0, 0x01010001); // enable analog,spdif,ac97 front
	//snd_ca0106_ptr_write(card, SPDIF_SELECT2, 0, 0x0f0f0000); // enable analog and spdif 7.1

	snd_ca0106_ptr_write(card, CAPTURE_CONTROL, 0, 0x40c81000); // goes to 0x40c80000 when doing SPDIF IN/OUT
	/* (Mute) CAPTURE feedback into PLAYBACK volume.
	 * Only lower 16 bits matter.
	 */
	snd_ca0106_ptr_write(card, CAPTURE_CONTROL, 1, 0xffffffff);

#if 0 /* fixme: writes to chn 2 & 3 in ALSA */
	/* SPDIF IN Volume */
	snd_ca0106_ptr_write(chip, CAPTURE_CONTROL, 2, 0x30300000);
	/* SPDIF IN Volume, 0x70 = (vol & 0x3f) | 0x40 */
	snd_ca0106_ptr_write(chip, CAPTURE_CONTROL, 3, 0x00700000);
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
	snd_ca0106_ptr_write(chip, CAPTURE_ROUTING1, 0, 0x32765410);
	snd_ca0106_ptr_write(chip, CAPTURE_ROUTING2, 0, 0x76767676);
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
	snd_ca0106_hw_init(card);

	//outpd(card->iobase + CA0106_GPIO, 0x005f03a3); /* analog - ALSA */
	//outpd(card->iobase + CA0106_GPIO, 0x005f02a2); /* SPDIF - MpxPlay */
	/* v2.1: /O option 0=mpxplay variant, 1=ALSA variant */
	outpd(card->iobase + CA0106_GPIO, (aui->card_outdev == 1) ? 0x005f03a3 : 0x005f02a2);

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
	snd_ca0106_hw_init(card);

	//outpd(card->iobase + CA0106_GPIO, 0x005f5301); /* analog - ALSA */
	//outpd(card->iobase + CA0106_GPIO, 0x005f5201); /* SPDIF - MpxPlay */
	/* v2.1: /O option 0=MpxPlay variant, 1=ALSA variant */
	outpd(card->iobase + CA0106_GPIO, (aui->card_outdev == 1) ? 0x005f5301 : 0x005f5201 );

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

	i = inpd(card->iobase + CA0106_HCFG); // control bit width
	if(aui->bits_card == 32)
		i |= HCFG_PLAYBACK_S32_LE;
	else
		i &= ~HCFG_PLAYBACK_S32_LE;
	outpd(card->iobase + CA0106_HCFG,i);

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
	snd_ca0106_ptr_write(card, PLAYBACK_PERIOD_SIZE, channel, 0);
	//snd_ca0106_ptr_write(card, PLAYBACK_PERIOD_SIZE, channel, period_size_bytes<<16);
	snd_ca0106_ptr_write(card, PLAYBACK_POINTER, channel, 0);
	snd_ca0106_ptr_write(card, PLAYBACK_PERIOD_END_ADDR, channel, 0);
	snd_ca0106_ptr_write(card, PLAYBACK_FIFO_OFFSET_ADDRESS, channel, 0);
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

	aui->chan_card = 2;

	if(aui->bits_card > 16)
		aui->bits_card = 32;
	else
		aui->bits_card = 16;

	//if(aui->freq_card == 44100)     // forced 44.1k dac output
	if(0 == aui->freq_card % 11025)   // 11025, 22050 and 44100 -> 44100
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
	dbgprintf(("snd_setrate: dmabufsize:%d period_size:%d\n",dmabufsize,card->period_size));

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

	ptr4 /= ( 2 * sizeof(uint32_t));

	ptr = (ptr4 * card->period_size) + ptr1;

	dbgprintf(("snd_pcm_pointer_playback: bufpos:0x%X [list_ptr:%d period_ptr:%d]\n", ptr, ptr4, ptr1));

	/* AU_cardbuf_space() expects position in byte units! */
	//ptr /= aui->chan_card;
	//ptr /= aui->bits_card >> 3;

	return ptr;
}

// Live 24 cards have no ac97

static unsigned int snd_mixer_read( struct emu10k1_card *card, unsigned int reg)
////////////////////////////////////////////////////////////////////////////////
{
	unsigned int channel_id = reg >> 8;
	reg &= 0xff;
	return snd_ca0106_ptr_read(card,reg,channel_id);
}

static void snd_mixer_write( struct emu10k1_card *card, unsigned int reg, unsigned int value)
/////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int channel_id = reg >> 8;
	reg &= 0xff;
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

