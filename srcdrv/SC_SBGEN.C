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
//function: generic SB routines for SB cards with EMU10K1/EMU10K2/CA0106
//based on the Creative (http://sourceforge.net/projects/emu10k1)
//         and ALSA (http://www.alsa-project.org) drivers

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "AUISTRUC.H"
#include "DEBUG.H"
#include "DMABUFF.H"
#include "PCIBIOS.H"
#include "AC97MIX.H"
#include "SC_SBGEN.H"

#define PCI_VENDOR_ID_CREATIVE         0x1102
#define PCI_DEVICE_ID_CREATIVE_EMU10K1 0x0002
#define PCI_DEVICE_ID_CREATIVE_AUDIGY  0x0004

//--- PCI vendorID/deviceID to scan for
static const struct pci_device_s creative_devices[] = {
 {"Live!"         , PCI_VENDOR_ID_CREATIVE, PCI_DEVICE_ID_CREATIVE_EMU10K1,0},
 {"Audigy"        , PCI_VENDOR_ID_CREATIVE, PCI_DEVICE_ID_CREATIVE_AUDIGY,1},
 {"Audigy 2 value", PCI_VENDOR_ID_CREATIVE, 0x0008,2}, // SB0400
 {"Live! 24bit"   , PCI_VENDOR_ID_CREATIVE, 0x0007,3}, // Live 24 , Audigy LS
 { NULL,0,0 }
};

extern const struct emu_card_version_s emucards_02[];
extern const struct emu_card_version_s emucards_04[];
extern const struct emu_card_version_s emucards_08[];
extern const struct emu_card_version_s emucards_07[];

/* SB card table for device-IDs 0002, 0004, 0008, 0007 */
static const struct emu_card_version_s *cardtable[] = {
	emucards_02,
	emucards_04,
	emucards_08,
	emucards_07,
};

extern struct emu_driver_func_s emu_driver_10k1_funcs;
extern struct emu_driver_func_s emu_driver_10k2_funcs;
#if CA0151
extern struct emu_driver_func_s emu_driver_p16v_funcs;
#endif
extern struct emu_driver_func_s emu_driver_audigyls_funcs;
extern struct emu_driver_func_s emu_driver_live24_funcs;

static const struct emu_driver_func_s *emu_driver_all_funcs[] = {
 &emu_driver_10k1_funcs, /* SB Live */
 &emu_driver_10k2_funcs, /* SB Audigy 1/2/4 if bits are <= 16 */
#if CA0151
 &emu_driver_p16v_funcs, /* SB Audigy 2/4 if bits are > 16 */
#endif
 &emu_driver_audigyls_funcs, /* SB Audify LS - defined in SBLIVE24.C */
 &emu_driver_live24_funcs, /* SB Live 24 - defined in SBLIVE24.C */
};

struct sndcard_info_s SBGEN_sndcard_info;

static void SBALL_close( struct audioout_info_s *aui );

/* autodetect for all SoundBlasters - Live, Audigy, AudigyLS, Live24 */

static int SBALL_adetect( struct audioout_info_s *aui )
///////////////////////////////////////////////////////
{
	struct emu10k1_card *card = aui->card_private_data;
	const struct emu_card_version_s *version;
	unsigned int devidx;
	int i;

	dbgprintf(("SBALL_adetect\n"));

	/* v2.1: using FindDeviceClass() instead of search_devices();
	 * this allows to start the search with an index, thus ignoring
	 * cards. This may be useful if multiple SB PCI cards are
	 * installed and one wants vsbhda to select a specific one.
	 */
	//if (pcibios_search_devices( creative_devices, &card->pci_dev) != PCI_SUCCESSFUL ) {
	//	dbgprintf(("SBALL_adetect: pcibios_search_devices failed\n"));
	//	goto err_adetect;
	//}
	for ( devidx = aui->card_devidx; ; devidx++ ) {
		if( pcibios_FindDeviceClass(&card->pci_dev, 4, 1, 0, devidx, creative_devices) != PCI_SUCCESSFUL )
			break;
		dbgprintf(("SBALL_adetect(%u): found device vendor=%X device=%X\n", devidx, card->pci_dev.vendor_id, card->pci_dev.device_id));
		if ( card->pci_dev.vendor_id != PCI_VENDOR_ID_CREATIVE )
			continue;

		card->chiprev= pcibios_ReadConfig_Byte(&card->pci_dev, PCIR_RID); /* revision ID */
		card->model  = pcibios_ReadConfig_Word(&card->pci_dev, PCIR_SSID);	/* subsys model */
		card->serial = pcibios_ReadConfig_Dword(&card->pci_dev, PCIR_SSVID); /* subsys vendorID + model */

		/* check for the SB variants that are supported */
		for ( version = cardtable[card->pci_dev.device_type]; version->longname; version++ ) {
			if (( version->subsystem == card->serial)
				|| (version->revision && (version->revision == card->chiprev)) /* this is to detect "Audigy 2 [unknown]" */
				|| (!version->revision && !version->subsystem) ) {// "catch all" - unknown but supported card
				card->card_capabilities = version;
				break;
			}
		}
		if (NULL == card->card_capabilities) { /* this should never be true, since all tables have a "catch all" last entry */
			dbgprintf(("SBALL_adetect: SB variant (dev/subs/rev=%X/%u/%u) unknown\n", card->pci_dev.device_id, card->serial, card->chiprev ));
			continue;
		}

		pcibios_enable_BM_IO(&card->pci_dev);

		card->iobase = pcibios_ReadConfig_Dword(&card->pci_dev, PCIR_BAR0);
		/* must be an I/O address */
		if (!(card->iobase & 1)) {
			dbgprintf(("SBALL_adetect(%u): invalid IO port %X in PCI address bar\n", devidx, card->iobase));
			continue;
		}
		card->iobase &= 0xfff0;

		card->chip_select = card->chips = card->card_capabilities->chips;

		/* check the "families": 10k1, 10k2, p16v, audigyls, live24 */
		for ( i = 0; i < sizeof( emu_driver_all_funcs ) / sizeof( emu_driver_all_funcs[0] ); i++ ) {
			if ( emu_driver_all_funcs[i]->selector(card,aui) ) {
				if ( !emu_driver_all_funcs[i]->buffer_init(card,aui) ) {
					dbgprintf(("SBALL_adetect: buffer_init() failed\n" ));
					break;
				}
				card->driver_funcs = emu_driver_all_funcs[i];
				break;
			}
		}

		if ( !card->driver_funcs ) {
			dbgprintf(("SBALL_adetect: SB variant (dev/subs/rev=%X/%u/%u) rejected, no driver_funcs\n", card->pci_dev.device_id, card->serial, card->chiprev ));
			continue;
		}
		//aui->card_irq = card->irq = pcibios_ReadConfig_Byte(&card->pci_dev, PCIR_INTR_LN);
		aui->card_irq = card->pci_dev.bIrq;
		aui->card_pDmaBuffer = card->pcmout_buffer;

		if (card->driver_funcs->hw_init)
			card->driver_funcs->hw_init( card, aui );

		/* v1.7: set the detailed card name */
		SBGEN_sndcard_info.shortname       = card->card_capabilities->longname;
		SBGEN_sndcard_info.cardbuf_getpos  = card->driver_funcs->pcm_pointer_playback;
		SBGEN_sndcard_info.irq_routine     = card->driver_funcs->interrupt_isr;
		SBGEN_sndcard_info.card_mixerchans = card->driver_funcs->mixerset;

		dbgprintf(("SBALL_adetect: card ok, name=%s, subsys=%X, rev=%u, base=%X, irq=%u\n", card->card_capabilities->longname, card->serial, card->chiprev, card->iobase, aui->card_irq ));

		return 1;
	}
//err_adetect:
	SBALL_close(aui);
	return 0;
}

static void SBALL_close( struct audioout_info_s *aui )
//////////////////////////////////////////////////////
{
	struct emu10k1_card *card = aui->card_private_data;

	if ( card ) {
		if (card->driver_funcs && card->driver_funcs->hw_close)
			card->driver_funcs->hw_close( card );
		MDma_free_cardmem( &card->dm );
		//free(card);
		//aui->card_private_data = NULL;
	}
	return;
}

static void SBALL_setrate( struct audioout_info_s *aui )
////////////////////////////////////////////////////////
{
	struct emu10k1_card *card = aui->card_private_data;

	//aui->card_wave_id = WAVEID_PCM_SLE;

	if ( card->driver_funcs->setrate )
		card->driver_funcs->setrate( card, aui );
	return;
}

/* generic SB card_start() */

static void SBALL_start( struct audioout_info_s *aui )
//////////////////////////////////////////////////////
{
	struct emu10k1_card *card = aui->card_private_data;

	/* v2.0: added, makes SBALL_clearbuf() obsolete */
	if ( card->driver_funcs->clear_cache )
		card->driver_funcs->clear_cache( card );

	if ( card->driver_funcs->start_playback )
		card->driver_funcs->start_playback( card );
	return;
}

static void SBALL_stop( struct audioout_info_s *aui )
/////////////////////////////////////////////////////
{
	struct emu10k1_card *card = aui->card_private_data;

	/* v1.8: added;
	 * v2.1: removed - SBALL_stop() is also used by non-EMU10kx code!
	 */
	//emu10k1_writefn0(card, EMU10K_INTENABLE, 0 );

	if ( card->driver_funcs->stop_playback )
		card->driver_funcs->stop_playback( card );

	return;
}

#if 0 /* v2.1: removed, AU_cardbuf_space() calls the pcm_pointer_playback funcs directly */

/* generic SB card_getpos() */

static unsigned int SBALL_getbufpos( struct audioout_info_s *aui )
//////////////////////////////////////////////////////////////////
{
	struct emu10k1_card *card = aui->card_private_data;
	unsigned int bufpos;

	if (card->driver_funcs->pcm_pointer_playback)
		bufpos = card->driver_funcs->pcm_pointer_playback( card, aui );
	else
		bufpos = 0;

	bufpos *= aui->chan_card;
	bufpos *= aui->bits_card >> 3;
	//dbgprintf(("SBALL_getbufpos: 0x%X\n", bufpos));
	return bufpos;
}
#endif

#if 0 /* v2.0: removed, clear_cache() is called in SBALL_start() now */
static void SBALL_clearbuf( struct audioout_info_s *aui )
/////////////////////////////////////////////////////////
{
	struct emu10k1_card *card = aui->card_private_data;
	//MDma_clearbuf(aui);
	if ( card->driver_funcs->clear_cache )
		card->driver_funcs->clear_cache( card );
	return;
}
#endif

static void SBALL_writeMIXER( struct audioout_info_s *aui, unsigned long reg, unsigned long val )
/////////////////////////////////////////////////////////////////////////////////////////////////
{
	struct emu10k1_card *card = aui->card_private_data;

	if ( card->driver_funcs->mixer_write )
		card->driver_funcs->mixer_write( card,reg, val );
	return;
}

static unsigned long SBALL_readMIXER( struct audioout_info_s *aui, unsigned long reg )
//////////////////////////////////////////////////////////////////////////////////////
{
	struct emu10k1_card *card = aui->card_private_data;

	if ( card->driver_funcs->mixer_read )
		return card->driver_funcs->mixer_read( card, reg );

	return 0;
}

#if 0 /* v2.1: removed, AU_isirq() calls the interrupt_isr funcs directly */

static int SBALL_IRQRoutine( struct audioout_info_s *aui )
//////////////////////////////////////////////////////////
{
	//dbgprintf(("SBALL_IRQRoutine\n"));
	struct emu10k1_card *card = aui->card_private_data;
	return card->driver_funcs->interrupt_isr( card );
}
#endif

/* sndcard_info_s can't be const, since various fields will be modified */

struct sndcard_info_s SBGEN_sndcard_info = {
 "SB Live!/Audigy",
 0,
 &SBALL_adetect,
 &SBALL_start,
 &SBALL_stop,
 &SBALL_close,
 &SBALL_setrate,

 NULL,
 NULL, /* v2.1: now set in SBALL_adetect(); was &SBALL_getbufpos */
 NULL, /* v2.1: now set in SBALL_adetect(); was &SBALL_IRQRoutine */
 &SBALL_writeMIXER,
 &SBALL_readMIXER,
 NULL, /* card_mixerchans */
 sizeof(struct emu10k1_card)
};
