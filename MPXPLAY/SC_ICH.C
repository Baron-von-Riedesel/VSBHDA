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
//function: Intel ICH audiocards low level routines
//based on: ALSA (http://www.alsa-project.org) and ICH-DOS wav player from Jeff Leyda
//v1.7: SiS 7012 support; copied from SBEMU, impl. by Thomas Perl (m@thp.io), 09/2023
//v1.8: removed snd_intel_measure_ac97_clock(), called for
//      INTEL ICH (82801AA) and ICH0|2|3 (82801AB,82801BA,82801CA)

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#if defined(__GNUC__)
#include <sys/time.h>
#endif
#ifndef DJGPP
#include <conio.h> /* for outb/outw/outd */
#endif

#include "CONFIG.H"
#include "MPXPLAY.H"
#include "DMABUFF.H"
#include "PCIBIOS.H"
#include "AC97MIX.H"

#define PCIR_CFG 0x41 // ICH4-7

/* port offsets and flags for Native Audio Bus Master Control Registers
 * 00-0F PCM in
 * 10-1F PCM out
 * 20-2B Mic in
 * 2C-34 Global ( Global Control (32), Global Status (32), Codec Write Semaphore (8) )
 * 40-4F Mic2 in
 * 50-5F PCM2 in
 * 60-6F S/PDIF
 */
/* PCM out registers */
#define ICH_PO_BDBAR  0x10  // BDBAR - Buffer Descriptor Base Address (32-bit)
#define ICH_PO_CIV    0x14  // CIV - Current Index Value (ro; 8-bit)
#define ICH_PO_LVI    0x15  // LVI - Last Valid Index (rw; 8-bit)
#define ICH_PO_SR     0x16  // SR - Status (ro,r/wc; 16-bit)
#define ICH_PO_PICB   0x18  // PICB - Position In Current Buffer (ro; 16-bit; remaining, not processed pos)
#define ICH_PO_PIV    0x1a  // PIV - Prefetched Index Value (ro; 8-bit; not used)
#define ICH_PO_CR     0x1b  // CR - Control (rw; 8-bit)

#define ICH_PO_PICB_SIS 0x16 /* for SiS, SR and PICB are exchanged */
#define ICH_PO_SR_SIS   0x18 /* PCM out Status register (16-bit) */

#define ICH_PO_SR_DCH     0x01  // DMA Controller Halted: 1=halted, 0=running (RO)
#define ICH_PO_SR_LVBCI   0x04  // Last Valid Buffer Completion Interrupt (R/WC)
#define ICH_PO_SR_BCIS    0x08  // Buffer Completion Interrupt Status (IOC) (R/WC)
#define ICH_PO_SR_FIFO    0x10  // FIFO error interrupt (R/WC)

#define ICH_PO_CR_START   0x01  // 1=start BM op, 0=pause BM op
#define ICH_PO_CR_RESET   0x02  // 1=reset all BM related regs ( autoclears to 0 )
#define ICH_PO_CR_LVBIE   0x04  // 1=Last Valid Buffer Interrupt enable
#define ICH_PO_CR_FEIE    0x08  // 1=FIFO Error Interrupt enable (unused)
#define ICH_PO_CR_IOCE    0x10  // 1=Interrupt On Completion enable

#define ICH_GBL_CTL_REG       0x2c  // Global control register (32-bit)
#define ICH_GBL_CTL_ACLINKOFF 0x00000008 // 1=turn off ac97 link
#define ICH_GBL_CTL_AC97WARM  0x00000004 // AC'97 warm reset ( writing a 1 )
#define ICH_GBL_CTL_AC97COLD  0x00000002 // AC'97 cold reset ( writing a 0 )
#define ICH_GBL_CTL_GIE       0x00000001 // 1=GPI change causes interrupt
#define ICH_GBL_CTL_PCM_246_MASK     0x00300000 // bits 20-21: 00=2, 01=4, 02=6 channel mode (not all chips)
#define ICH_GBL_CTL_PCM_246_MASK_SIS 0x000000C0 // bits 6-7: 00=2, 01=4, 02=6 channel mode
#define ICH_GBL_CTL_PCM_20BIT        0x00400000 // bits 22-23: 00=16, 01=20-bit samples (ICH4)

#define ICH_GBL_ST_REG  0x30       // Global Status register (RO; 32-bit)
#define ICH_GBL_ST_PCR  0x00000100 // Primary codec is ready for action (software must check these bits before starting the codec!)
#define ICH_GBL_ST_RCS  0x00008000 // read completion status: 1=codec read caused timeout; 0=read ok
#define ICH_GBL_ST_GSCI 0x00000001 // GPI Status Change Interrupt
#define ICH_GBL_ST_SAMPLE_CAP   0x00c00000 // bits 22-23: sample capability bits (RO) (ICH4)
#define ICH_GBL_ST_SAMPLE_16_20 0x00400000 // bit 22: 0=16, 1=20-bit samples (ICH4)

#define ICH_ACC_SEMA_REG  0x34  // codec write semiphore register (8-bit)
#define ICH_CODEC_BUSY    0x01  // codec register I/O is happening; self clearing

#define ICH_BD_IOC        0x8000 //buffer descriptor high word: interrupt on completion (IOC)

#define ICH_DMABUF_PERIODS  32
#define ICH_MAX_CHANNELS     2
#define ICH_MAX_BYTES        4
//#define ICH_DMABUF_ALIGN (ICH_DMABUF_PERIODS * ICH_MAX_CHANNELS * ICH_MAX_BYTES) // 256
#define ICH_DMABUF_ALIGN 512 /* v1.7 to match the setting of HDA/ES1371 */
#if 1 //def SBEMU
#define ICH_INT_INTERVAL     1 //interrupt interval in periods
#endif

#define ICH_DEFAULT_RETRY 1000

struct intel_card_s {
    unsigned long   baseport_bm;       // busmaster baseport
    unsigned long   baseport_codec;    // mixer baseport
    unsigned int    irq;
    unsigned char   device_type;
    unsigned char   sr_reg;
    struct pci_config_s pci_dev;
    struct cardmem_s dm; /* XMS memory struct */

    /* must be aligned to 8 bytes.
     * consists of ICH_DMABUF_PERIODS elements, each element has
     * 2 dwords, first is physical address, second is size (in "samples")
     */
    uint32_t *virtualpagetable;
    char *pcmout_buffer;
    long pcmout_bufsize;

    //unsigned int dma_size;
    unsigned int period_size_bytes;

    unsigned char vra;
    //unsigned char dra;
    //unsigned int ac97_clock_detected; /* v1.8: removed */
};

enum { DEVICE_INTEL, DEVICE_INTEL_ICH4567, DEVICE_NFORCE, DEVICE_SIS };
#ifdef _DEBUG
static const char *ich_devnames[]={"ICH","ICH4-7","NForce", "SIS 7012" };
#endif

//-------------------------------------------------------------------------
// low level write & read

#define snd_intel_write_8(card,reg,data)  outb(card->baseport_bm+reg,data)
#define snd_intel_write_16(card,reg,data) outw(card->baseport_bm+reg,data)
#define snd_intel_write_32(card,reg,data) outl(card->baseport_bm+reg,data)

#define snd_intel_read_8(card,reg)  inb(card->baseport_bm+reg)
#define snd_intel_read_16(card,reg) inw(card->baseport_bm+reg)
#define snd_intel_read_32(card,reg) inl(card->baseport_bm+reg)

static unsigned int snd_intel_codec_rdy(struct intel_card_s *card,unsigned int bitmask)
///////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int retry;

	if(!bitmask)
		bitmask = ICH_GBL_ST_PCR;

	// wait for codec ready status
	for ( retry = ICH_DEFAULT_RETRY; retry; retry-- ) {
		if( snd_intel_read_32(card,ICH_GBL_ST_REG) & bitmask )
			break;
		pds_delay_10us(10);
	}
#ifdef _DEBUG
	if (!retry) { dbgprintf(("snd_intel_codec_rdy: timeout\n" )); }
#endif
	return retry;
}

static void snd_intel_codec_semaphore(struct intel_card_s *card,unsigned int codec)
///////////////////////////////////////////////////////////////////////////////////
{
	unsigned int retry;

	snd_intel_codec_rdy(card,codec);

	//wait for semaphore ready (not busy) status
	for ( retry = ICH_DEFAULT_RETRY; retry; retry-- ) {
		if(!(snd_intel_read_8(card,ICH_ACC_SEMA_REG) & ICH_CODEC_BUSY))
			break;
		pds_delay_10us(10);
	}
#ifdef _DEBUG
	if (!retry) { dbgprintf(("snd_intel_codec_semaphore: timeout\n" )); }
#endif
	// clear semaphore flag
	//inw(card->baseport_codec); // (removed for ICH0)
}

/* only shorts can be written to the codec */

static void snd_intel_codec_write(struct intel_card_s *card,unsigned int reg,unsigned short data)
/////////////////////////////////////////////////////////////////////////////////////////////////
{
	snd_intel_codec_semaphore(card,ICH_GBL_ST_PCR);
	outw(card->baseport_codec + reg,data);
}

static unsigned int snd_intel_codec_read( struct intel_card_s *card, unsigned int reg )
///////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int data = 0,retry;
	snd_intel_codec_semaphore(card,ICH_GBL_ST_PCR);

	for ( retry = ICH_DEFAULT_RETRY; retry; retry-- ) {
		data = inw( card->baseport_codec + reg );
		if(!(snd_intel_read_32( card, ICH_GBL_ST_REG) & ICH_GBL_ST_RCS ) )
			break;
		pds_delay_10us(10);
	}
#ifdef _DEBUG
	if ( !retry ) { dbgprintf(("snd_intel_codec_read: timeout\n" )); }
#endif
	return data;
}

/* init card->dm, card->pcmout, card->virtualpagetable
 */

static unsigned int snd_intel_buffer_init( struct intel_card_s *card, struct audioout_info_s *aui )
///////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int bytes_per_sample = (aui->bits_set > 16) ? 4 : 2;

	card->pcmout_bufsize = MDma_get_max_pcmoutbufsize( aui, 0, aui->gvars->period_size ? aui->gvars->period_size : ICH_DMABUF_ALIGN, bytes_per_sample, 0 );
	/* v1.8 restrict buffer to period_size * 32 */
	card->pcmout_bufsize = min( card->pcmout_bufsize, ( aui->gvars->period_size ? aui->gvars->period_size : ICH_DMABUF_ALIGN ) * ICH_DMABUF_PERIODS );

	if (!MDma_alloc_cardmem(&card->dm, ICH_DMABUF_PERIODS * 2 * sizeof(uint32_t) + card->pcmout_bufsize ) ) return 0;
	/* pagetable requires 8 byte align; MDma_alloc_cardmem() returns 1kB aligned ptr */
	card->virtualpagetable = (uint32_t *)card->dm.pMem;
	card->pcmout_buffer = ((char *)card->virtualpagetable) + ICH_DMABUF_PERIODS * 2 * sizeof(uint32_t);
	aui->card_DMABUFF = card->pcmout_buffer;
#if 0//v1.8: memory cleared by MDma_alloc_cardmem()
	memset(card->pcmout_buffer, 0, card->pcmout_bufsize);
#endif
	dbgprintf(("snd_intel_buffer init: pagetable:%X pcmoutbuf:%X size:%d\n",(unsigned long)card->virtualpagetable,(unsigned long)card->pcmout_buffer,card->pcmout_bufsize));
	return 1;
}

/*
 * called by ICH_adetect()
 */

static void snd_intel_chip_init(struct intel_card_s *card)
//////////////////////////////////////////////////////////
{
	unsigned int cmd,retry;

	dbgprintf(("intel_chip_init: enter\n"));

	cmd = snd_intel_read_32( card, ICH_GBL_ST_REG);
	cmd &= ICH_GBL_ST_RCS; // ???
	snd_intel_write_32( card, ICH_GBL_ST_REG, cmd);

	cmd = snd_intel_read_32(card, ICH_GBL_CTL_REG);
	/* v1.7: support for SiS 7012 */
	if ( card->device_type == DEVICE_SIS )
		cmd &= ~(ICH_GBL_CTL_ACLINKOFF | ICH_GBL_CTL_PCM_246_MASK_SIS);
	else
		cmd &= ~(ICH_GBL_CTL_ACLINKOFF | ICH_GBL_CTL_PCM_246_MASK);
	// finish cold or do warm reset
	cmd |= ((cmd & ICH_GBL_CTL_AC97COLD) == 0) ? ICH_GBL_CTL_AC97COLD : ICH_GBL_CTL_AC97WARM;
	snd_intel_write_32(card, ICH_GBL_CTL_REG, cmd);
	dbgprintf(("snd_intel_chip_init: AC97 reset type: %s\n",((cmd & ICH_GBL_CTL_AC97COLD) ? "cold":"warm")));

	for ( retry = ICH_DEFAULT_RETRY; retry; retry-- ) {
		unsigned int cntreg = snd_intel_read_32(card,ICH_GBL_CTL_REG);
		if(!(cntreg & ICH_GBL_CTL_AC97WARM))
			break;
		pds_delay_10us(10);
	}
#ifdef _DEBUG
	if ( !retry ) { dbgprintf(("snd_intel_chip_init: reset timeout\n" )); }
#endif
	// wait for primary codec ready status
	retry = snd_intel_codec_rdy(card,ICH_GBL_ST_PCR);
#ifdef _DEBUG
	if ( !retry ) dbgprintf(("snd_intel_chip_init: primary codec not ready\n"));
#endif

	//snd_intel_codec_read( card, 0); // clear semaphore flag (removed for ICH0)
	snd_intel_write_8( card, ICH_PO_CR, ICH_PO_CR_RESET); // reset channels
#if 1 //def SBEMU
	//pds_delay_10us(2000);
	//snd_intel_write_8( card, ICH_PO_CR, /*ICH_PO_CR_LVBIE*/ICH_PO_CR_IOCE );
#endif
	/* v1.7: support for SiS 7012 */
	card->sr_reg = ICH_PO_SR;
	if ( card->device_type == DEVICE_SIS ) {
		card->sr_reg = ICH_PO_SR_SIS;
		/* 04x is the DMA engine for the Mic 2; last field is MC2_CR at offset 0x4B, though.  */
		snd_intel_write_16( card, 0x4C, snd_intel_read_16( card, 0x4C ) | 1 ); /* unmute? */
	}

	dbgprintf(("snd_intel_chip_init: exit\n"));
}

static void snd_intel_chip_close(struct intel_card_s *card)
///////////////////////////////////////////////////////////
{
	if(card->baseport_bm)
		snd_intel_write_8(card,ICH_PO_CR,ICH_PO_CR_RESET); // reset codec
}

static void snd_intel_ac97_init(struct intel_card_s *card, unsigned int freq_set)
/////////////////////////////////////////////////////////////////////////////////
{
	uint16_t eastat;
#if 1
	/* v1.8: master volume may be 5 bit only; so check if 6 bits can be written -
	 * if no, reduce volume to 5 bits.
	 */
	snd_intel_codec_write(card, AC97_MASTER_VOL_STEREO, 0x3F3F);
	if (snd_intel_codec_read(card, AC97_MASTER_VOL_STEREO) != 0x3F3F) {
		struct aucards_mixerchan_s *onechi = (struct aucards_mixerchan_s *)*aucards_ac97chan_mixerset;
		onechi->submixerchans[0].submixch_bits = 5;
		onechi->submixerchans[1].submixch_bits = 5;
	}
#endif
	// initial ac97 volumes (and clear mute flag)
	snd_intel_codec_write(card, AC97_MASTER_VOL_STEREO, 0x0202);
	snd_intel_codec_write(card, AC97_PCMOUT_VOL,        0x0202);
	snd_intel_codec_write(card, AC97_HEADPHONE_VOL,     0x0202);
	/* v1.8: preserve the other bits of extended status reg */
	//snd_intel_codec_write(card, AC97_EXTENDED_STATUS,AC97_EA_SPDIF);
	eastat = snd_intel_codec_read(card, AC97_EXTENDED_STATUS);
	eastat |= AC97_EA_SPDIF;

	// set/check variable bit rate bit
	if( freq_set != 48000 )
		if(snd_intel_codec_read( card, AC97_EXTENDED_ID ) & AC97_EA_VRA )
			eastat |= AC97_EA_VRA;

	snd_intel_codec_write( card, AC97_EXTENDED_STATUS, eastat );
	if( snd_intel_codec_read( card, AC97_EXTENDED_STATUS ) & AC97_EA_VRA )
		card->vra = 1;

	dbgprintf(("intel_ac97_init: end (vra:%d)\n",card->vra));
}

/*
 * called by ICH_setrate()
 */
static void snd_intel_prepare_playback( struct intel_card_s *card, struct audioout_info_s *aui )
////////////////////////////////////////////////////////////////////////////////////////////////
{
	uint32_t *table_base;
	unsigned int i,cmd,retry,period_size_samples;
	unsigned short codecdata, spdif_rate;

	dbgprintf(("intel_prepare playback: enter, period_size_bytes=%d\n",card->period_size_bytes));

	// wait until DMA stopped ???
	for ( retry = ICH_DEFAULT_RETRY; retry; retry-- ) {
		if(snd_intel_read_8(card,card->sr_reg) & ICH_PO_SR_DCH)
			break;
		pds_delay_10us(1);
	}
#ifdef _DEBUG
	if (!retry) { dbgprintf(("intel_prepare_playback: dma stop timeout=%d\n",retry)); }
#endif
	// reset codec
	snd_intel_write_8(card,ICH_PO_CR, snd_intel_read_8(card, ICH_PO_CR) | ICH_PO_CR_RESET);

	// set channels (2) and bits (16/32)
	cmd = snd_intel_read_32( card, ICH_GBL_CTL_REG );
	if ( card->device_type == DEVICE_SIS )
		cmd &= ~(ICH_GBL_CTL_PCM_246_MASK_SIS | ICH_GBL_CTL_PCM_20BIT);
	else
		cmd &= ~(ICH_GBL_CTL_PCM_246_MASK | ICH_GBL_CTL_PCM_20BIT);
	if( aui->bits_set > 16 ) {
		if((card->device_type == DEVICE_INTEL_ICH4567) && ((snd_intel_read_32(card,ICH_GBL_ST_REG) & ICH_GBL_ST_SAMPLE_CAP) == ICH_GBL_ST_SAMPLE_16_20 )) {
			aui->bits_card = 32;
			cmd |= ICH_GBL_CTL_PCM_20BIT;
		}
	}
	snd_intel_write_32(card,ICH_GBL_CTL_REG,cmd);

	// set spdif freq (???)
	switch( aui->freq_card ){
	case 32000:spdif_rate = AC97_SC_SPSR_32K;break;
	case 44100:spdif_rate = AC97_SC_SPSR_44K;break;
	default:spdif_rate = AC97_SC_SPSR_48K;break;
	}
	codecdata = snd_intel_codec_read( card, AC97_SPDIF_CONTROL );
	codecdata &= AC97_SC_SPSR_MASK;
	codecdata |= spdif_rate;
	snd_intel_codec_write( card, AC97_SPDIF_CONTROL, codecdata);
	pds_delay_10us(10);

	//set analog ac97 freq
	dbgprintf(("intel_prepare_playback: AC97 front dac freq=%d\n",aui->freq_card));
	if( card->vra ) {
		snd_intel_codec_write( card, AC97_PCM_FRONT_DAC_RATE, aui->freq_card);
		aui->freq_card = snd_intel_codec_read( card, AC97_PCM_FRONT_DAC_RATE);
		dbgprintf(("intel_prepare_playback: SRC used, freq now %u\n", aui->freq_card ));
		pds_delay_10us(1600);
	}

	//set period table
	table_base = card->virtualpagetable;
	/* v1.7: support for SiS 7012 */
	if (card->device_type == DEVICE_SIS )
		period_size_samples = card->period_size_bytes;
	else
		period_size_samples = card->period_size_bytes / (aui->bits_card >> 3);
	for( i = 0; i < ICH_DMABUF_PERIODS; i++ ) {
		table_base[i*2] = pds_cardmem_physicalptr(card->dm, (char *)card->pcmout_buffer + ( i * card->period_size_bytes ));
#if 1 //def SBEMU
		table_base[i*2+1] = period_size_samples | (ICH_INT_INTERVAL && ((i % ICH_INT_INTERVAL == ICH_INT_INTERVAL-1)) ? (ICH_BD_IOC << 16) : 0);
#else
		table_base[i*2+1] = period_size_samples;
#endif
	}
	snd_intel_write_32(card,ICH_PO_BDBAR, pds_cardmem_physicalptr(card->dm,table_base));

	snd_intel_write_8(card,ICH_PO_LVI,(ICH_DMABUF_PERIODS - 1)); // last valid index
	snd_intel_write_8(card,ICH_PO_CIV,0); // reset current index (is ro!)

	dbgprintf(("intel_prepare playback exit\n"));
}

//-------------------------------------------------------------------------
static const struct pci_device_s ich_devices[] = {
 {"82801AA",0x8086,0x2415, DEVICE_INTEL},
 {"82901AB",0x8086,0x2425, DEVICE_INTEL},
 {"82801BA",0x8086,0x2445, DEVICE_INTEL},
 {"ICH3"   ,0x8086,0x2485, DEVICE_INTEL},
 {"ICH4"   ,0x8086,0x24c5, DEVICE_INTEL_ICH4567},
 {"ICH5"   ,0x8086,0x24d5, DEVICE_INTEL_ICH4567},
 {"ESB"    ,0x8086,0x25a6, DEVICE_INTEL_ICH4567},
 {"ICH6"   ,0x8086,0x266e, DEVICE_INTEL_ICH4567},
 {"ICH7"   ,0x8086,0x27de, DEVICE_INTEL_ICH4567},
 {"ESB2"   ,0x8086,0x2698, DEVICE_INTEL_ICH4567},
 {"440MX"  ,0x8086,0x7195, DEVICE_INTEL}, // maybe doesn't work (needs extra pci hack)
 {"SI7012" ,0x1039,0x7012, DEVICE_SIS}, // needs extra code
 {"NFORCE" ,0x10de,0x01b1, DEVICE_NFORCE},
 {"MCP04"  ,0x10de,0x003a, DEVICE_NFORCE},
 {"NFORCE2",0x10de,0x006a, DEVICE_NFORCE},
 {"CK804"  ,0x10de,0x0059, DEVICE_NFORCE},
 {"CK8"    ,0x10de,0x008a, DEVICE_NFORCE},
 {"NFORCE3",0x10de,0x00da, DEVICE_NFORCE},
 {"CK8S"   ,0x10de,0x00ea, DEVICE_NFORCE},
 {"AMD8111",0x1022,0x746d, DEVICE_INTEL},
 {"AMD768" ,0x1022,0x7445, DEVICE_INTEL},
 //{"ALI5455",0x10b9,0x5455, DEVICE_ALI}, // needs extra code
 {NULL,0,0,0}
};

static void ICH_close( struct audioout_info_s *aui );

static void ICH_show_card_info( struct audioout_info_s *aui )
/////////////////////////////////////////////////////////////
{
#if 0
	struct intel_card_s *card = aui->card_private_data;
	dbgprintf(("ICH : Intel %s found on port:X irq:%d (type:%s, bits:16%s)\n",
			card->pci_dev.device_name,card->baseport_bm,card->irq,
			ich_devnames[card->device_type],((card->device_type == DEVICE_INTEL_ICH4567) ? ",20":"")));
#endif
}

struct sndcard_info_s ICH_sndcard_info;

static int ICH_adetect( struct audioout_info_s *aui )
/////////////////////////////////////////////////////
{
	struct intel_card_s *card;

	card = (struct intel_card_s *)calloc(1,sizeof(struct intel_card_s));
	if(!card)
		return 0;
	aui->card_private_data = card;

	if(pcibios_search_devices(ich_devices,&card->pci_dev) != PCI_SUCCESSFUL)
		goto err_adetect;

#if 1 //def SBEMU
	if( card->pci_dev.device_type == DEVICE_INTEL_ICH4567 ) {
		/*
		 * enable legacy IO space; makes values at ofs 04h/10h/14h R/W.
		 */
		pcibios_WriteConfig_Byte(&card->pci_dev, PCIR_CFG, 1); //IOSE:enable IO space
		dbgprintf(("ICH_adetect: enable legacy IO space for ICH4-7 (PCI reg 41h).\n"));
	}
#endif

	dbgprintf(("ICH_adetect: enable PCI io and busmaster\n"));
	pcibios_enable_BM_IO(&card->pci_dev);

	card->baseport_bm = pcibios_ReadConfig_Dword(&card->pci_dev, PCIR_NABMBAR); /* PCI offset 0x14 */
	if (!(card->baseport_bm & 1 )) {/* must be an IO address */
		dbgprintf(("ICH_adetect: no IO port for DMA engine set\n"));
		goto err_adetect;
	}
	card->baseport_bm &= ~1; /* just mask out bits 0; bits 1-5 should be 0, since IO space is 64 ports */

	/* Some BIOSes don't set NAMBAR/NABMBAR at all. assign manually!?
	 * Almost certainly a bad idea - we don't know what port ranges are free to use -
	 * so if this is done, it should be done by an external tool.
	 * Better approach may be to set PnP OS in BIOS to "NO".
	 */
	if( card->baseport_bm == 0 ) {
#if 0
		int iobase = 0xF000;
		iobase &= ~0x3F;
		pcibios_WriteConfig_Dword(&card->pci_dev, PCIR_NABMBAR, iobase);
		card->baseport_bm = pcibios_ReadConfig_Dword(&card->pci_dev, PCIR_NABMBAR) & 0xfff0;
		if(!card->baseport_bm)
			goto err_adetect;
#else
		dbgprintf(("ICH_adetect: PCI IO addr for DMA engine (offs 0x14) not set\n"));
		goto err_adetect;
#endif
	}

	card->baseport_codec = pcibios_ReadConfig_Dword(&card->pci_dev, PCIR_NAMBAR); /* PCI offset 0x10 */
	if (!(card->baseport_codec & 1 )) { /* must be an IO address */
		dbgprintf(("ICH_adetect: no IO port for Native Audio Mixer set\n"));
		goto err_adetect;
	}
	card->baseport_codec &= ~1; /* just mask out bit 0; bits 1-7 should be 0, since IO space is 256 ports */
	if( card->baseport_codec == 0 ) {
#if 0
		/* see comment above for PCIR_NABMBAR! */
		iobase -= 256;
		iobase &= ~0xFF;
		pcibios_WriteConfig_Dword(&card->pci_dev, PCIR_NAMBAR, iobase);
		card->baseport_codec = pcibios_ReadConfig_Dword(&card->pci_dev, PCIR_NAMBAR) & 0xfff0;
		if(!card->baseport_codec)
			goto err_adetect;
#else
		dbgprintf(("ICH_adetect: PCI IO addr for codec (offs 0x10) not set\n"));
		goto err_adetect;
#endif
	}

	aui->card_irq = card->irq = pcibios_ReadConfig_Byte(&card->pci_dev, PCIR_INTR_LN);
#if 1
	/* if no interrupt assigned, assign #11?
	 * Also a doubtful action - BIOS should know better what IRQs are to be used.
	 * Probably it's better to just display an error -
	 * the appropriate fix might be to set "PnP OS" in the BIOS to "NO".
	 */
	if( aui->card_irq == 0xFF ) {
		printf(("Intel ICH: no IRQ set in PCI config space, trying to set it to 11\n"));
		pcibios_WriteConfig_Byte(&card->pci_dev, PCIR_INTR_LN, 11);
		aui->card_irq = card->irq = pcibios_ReadConfig_Byte(&card->pci_dev, PCIR_INTR_LN);
	}
#endif
 
	card->device_type = card->pci_dev.device_type;
	/* v1.7: set more exact name of card found */
	//ICH_sndcard_info.shortname = (char *)ich_devnames[card->device_type];
	ICH_sndcard_info.shortname = card->pci_dev.device_name;

	dbgprintf(("vend/dev_id=%X/%X devtype:%s bmport:%4X mixport:%4X irq:%d\n",
			  card->pci_dev.vendor_id, card->pci_dev.device_id, ich_devnames[card->device_type],card->baseport_bm,card->baseport_codec,card->irq));

	if( !snd_intel_buffer_init( card, aui ) )
		goto err_adetect;
	snd_intel_chip_init( card );
	snd_intel_ac97_init( card, aui->freq_set );
	return 1;

err_adetect:
	ICH_close(aui);
	return 0;
}

static void ICH_close( struct audioout_info_s *aui )
////////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	if(card){
		snd_intel_chip_close(card);
		MDma_free_cardmem(&card->dm);
		free(card);
		aui->card_private_data = NULL;
	}
}

static void ICH_setrate( struct audioout_info_s *aui )
//////////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	unsigned int dmabufsize;

	dbgprintf(("ICH_setrate() enter\n"));

	//aui->card_wave_id = WAVEID_PCM_SLE; /* done in AU_setrate() */
	//aui->chan_card = 2;
	//aui->bits_card = 16;

	if(!card->vra){
		aui->freq_card = 48000;
	} else {
		if(aui->freq_card < 8000)
			aui->freq_card = 8000;
		else
			if(aui->freq_card > 48000)
				aui->freq_card = 48000;
	}

	dmabufsize = MDma_init_pcmoutbuf( aui, card->pcmout_bufsize, aui->gvars->period_size ? aui->gvars->period_size : ICH_DMABUF_ALIGN, aui->freq_card);
	card->period_size_bytes = dmabufsize / ICH_DMABUF_PERIODS;

	snd_intel_prepare_playback(card,aui);
	dbgprintf(("ICH_setrate() exit\n"));
}

static void ICH_start( struct audioout_info_s *aui )
////////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	unsigned char cmd;

	snd_intel_codec_rdy(card,ICH_GBL_ST_PCR);
#if 0
	cmd = snd_intel_read_8(card,ICH_PO_CR);
	cmd |= ICH_PO_CR_START;
	snd_intel_write_8(card,ICH_PO_CR,cmd);
#else
	/* v1.7: bit ICH_PO_CR_IOCE now set here, also ICH_PO_CR_LVBIE ? */
	snd_intel_write_8( card, ICH_PO_CR, ICH_PO_CR_START | ICH_PO_CR_IOCE /* | ICH_PO_CR_LVBIE */ );
#endif
}

static void ICH_stop( struct audioout_info_s *aui )
///////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	unsigned char cmd;
#if 0
	cmd = snd_intel_read_8(card,ICH_PO_CR);
	cmd &= ~ICH_PO_CR_START;
	snd_intel_write_8(card,ICH_PO_CR,cmd);
#else
	snd_intel_write_8(card,ICH_PO_CR,snd_intel_read_8( card, ICH_PO_CR ) & ~ICH_PO_CR_START );
#endif
}

/* get time in microsecs */

static int64_t gettimeu(void)
/////////////////////////////
{
	int64_t time_ms;
	time_ms = (int64_t)clock() * (int64_t)1000000 / (int64_t)CLOCKS_PER_SEC;
	return time_ms;
}

//------------------------------------------------------------------------

static void ICH_writedata( struct audioout_info_s *aui, char *src, unsigned long left )
///////////////////////////////////////////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	unsigned int index;

	MDma_writedata(aui,src,left);

#if 1//def SBEMU
	snd_intel_write_8(card,ICH_PO_LVI,(snd_intel_read_8(card, ICH_PO_CIV)-1) % ICH_DMABUF_PERIODS);
#else
	index = aui->card_dmalastput / card->period_size_bytes;
	snd_intel_write_8(card,ICH_PO_LVI,(index-1) % ICH_DMABUF_PERIODS); // set stop position (to keep playing in an endless loop)
#endif
	//dbgprintf(("ICH_writedata: index=%d\n",index));
}

/* ICH implementation of cardbuf_getpos() */

static long ICH_getbufpos( struct audioout_info_s *aui )
////////////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	unsigned long bufpos = 0;
	unsigned int index,pcmpos,retry = 3;

	do{
		index = snd_intel_read_8( card, ICH_PO_CIV );  // number of current period
#if 0//ndef SBEMU
		//dbgprintf(("index1: %d\n",index));
		if(index >= ICH_DMABUF_PERIODS){
			if(retry > 1)
				continue;
			MDma_clearbuf(aui);
			snd_intel_write_8( card, ICH_PO_LVI, (ICH_DMABUF_PERIODS - 1) );
			snd_intel_write_8( card, ICH_PO_CIV, 0);
			aui->card_infobits |= AUINFOS_CARDINFOBIT_DMAUNDERRUN;
			continue;
		}
#endif

		/* v1.7: support for SiS 7012 */
		if ( card->device_type == DEVICE_SIS ) {
			pcmpos = snd_intel_read_16(card, ICH_PO_PICB_SIS); // position in the current period (remaining unprocessed in SAMPLEs)
		} else {
			pcmpos = snd_intel_read_16(card, ICH_PO_PICB ); // position in the current period (remaining unprocessed in SAMPLEs)
			pcmpos *= aui->bits_card >> 3;
		}
		//pcmpos*=aui->chan_card;
		//printf("%d %d %d %d\n",aui->bits_card, aui->chan_card, pcmpos, card->period_size_bytes);
		//dbgprintf(("ICH_getbufpos: pcmpos=%d\n",pcmpos));
		if(!pcmpos || pcmpos > card->period_size_bytes){
			if( snd_intel_read_8(card,ICH_PO_LVI) == index ) {
				MDma_clearbuf(aui);
				snd_intel_write_8(card,ICH_PO_LVI,(index-1) % ICH_DMABUF_PERIODS); // to keep playing in an endless loop
				//snd_intel_write_8(card,ICH_PO_CIV,index); // ??? -RO
				aui->card_infobits |= AUINFOS_CARDINFOBIT_DMAUNDERRUN;
			}
#if 0//ndef SBEMU
			continue;
#endif
		}
#if 0//ndef SBEMU
		if(snd_intel_read_8(card,ICH_PO_CIV) != index) // verifying
			continue;
#endif

		pcmpos = card->period_size_bytes - pcmpos;
		bufpos = index * card->period_size_bytes + pcmpos;

		if( bufpos < aui->card_dmasize ) {
			aui->card_dma_lastgoodpos = bufpos;
			break;
		}

	}while(--retry);

	//dbgprintf(("ICH_getbufpos: pos=%d dmasize=%d, dma_lastgoodpos=%d\n", bufpos, aui->card_dmasize, aui->card_dma_lastgoodpos ));

	return aui->card_dma_lastgoodpos;
}

/*--------------------------------------------------------------------------
 * mixer access; 4 volumes are set (AU_CARDS.C au_mixchan_outs[]):
 * AU_MIXCHAN_MASTER
 * AU_MIXCHAN_PCM
 * AU_MIXCHAN_HEADPHONE
 * AU_MIXCHAN_SPDIFOUT
 */

static void ICH_writeMIXER( struct audioout_info_s *aui, unsigned long reg, unsigned long val )
///////////////////////////////////////////////////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	dbgprintf(("ICH_writeMIXER(%X,%X)\n", reg, val ));
	snd_intel_codec_write(card,reg,val);
}

static unsigned long ICH_readMIXER( struct audioout_info_s *aui, unsigned long reg )
////////////////////////////////////////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
#ifdef _DEBUG
	unsigned long tmp = snd_intel_codec_read(card,reg);
	dbgprintf(("ICH_readMIXER(%X)=%X\n", reg, tmp ));
	return tmp;
#else
	return snd_intel_codec_read(card,reg);
#endif
}

#if 1 /* vsbhda */
static int ICH_IRQRoutine( struct audioout_info_s* aui )
////////////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	int status = snd_intel_read_8(card,card->sr_reg);
	status &= ICH_PO_SR_LVBCI | ICH_PO_SR_BCIS | ICH_PO_SR_FIFO;
	if(status)
		snd_intel_write_8(card, card->sr_reg, status); //ack
	return status != 0;
}
#endif

/* v1.7: const attribute removed, since shortname member must be r/w now */
struct sndcard_info_s ICH_sndcard_info = {
 "ICH AC97",
 0,
 NULL,
 NULL,              // no init
 &ICH_adetect,      // only autodetect
 &ICH_show_card_info,
 &ICH_start,
 &ICH_stop,
 &ICH_close,
 &ICH_setrate,

 &ICH_writedata,
 &ICH_getbufpos,
 &MDma_clearbuf,
 &ICH_IRQRoutine, /* vsbhda */
 &ICH_writeMIXER,
 &ICH_readMIXER,
 aucards_ac97chan_mixerset
};
