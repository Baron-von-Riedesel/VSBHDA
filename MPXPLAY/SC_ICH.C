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
#include "DMAIRQ.H"
#include "PCIBIOS.H"
#include "AC97MIX.H"

#define PCIR_CFG 0x41 // ICH4

/* port offsets and flags for Native Audio Bus Master Control Registers
 * 00-0F PCM in
 * 10-1F PCM out
 * 20-2B Mic in
 * 2C-34 Global ( DW lobal Control, DW Global Status, B Codec Write Semaphore )
 * 40-4F Mic2 in
 * 50-5F PCM2 in
 * 60-6F S/PDIF
 */
#define ICH_PO_BDBAR_REG  0x10  // PCM out buffer descriptor BAR
#define ICH_PO_LVI_REG    0x15  // PCM out Last Valid Index (set it)
#define ICH_PO_CIV_REG    0x14  // PCM out current Index value (RO?)
#define ICH_PO_PICB_REG   0x18  // PCM out position in current buffer(RO) (remaining, not processed pos)

#define ICH_PO_CR_REG     0x1b  // PCM out Control Register
#define ICH_PO_CR_START   0x01  // 1=start BM op, 0=pause BM op
#define ICH_PO_CR_RESET   0x02  // 1=reset all BM related regs ( autoclears to 0 )
#define ICH_PO_CR_LVBIE   0x04  // 1=last valid buffer interrupt enable
#define ICH_PO_CR_IOCE    0x10  // 1=IOC enable

#define ICH_PO_SR_REG     0x16  // PCM out Status register
#define ICH_PO_SR_DCH     0x01  // DMA controller: 1=halted, 0=running
#define ICH_PO_SR_LVBCI   0x04  // last valid buffer completion interrupt (R/WC)
#define ICH_PO_SR_BCIS    0x08  // buffer completion interrupt status (IOC) (R/WC)
#define ICH_PO_SR_FIFO    0x10  // FIFO error interrupt (R/WC)

#define ICH_GLOB_CNT_REG       0x2c  // Global control register
#define ICH_GLOB_CNT_ACLINKOFF 0x00000008 // 1=turn off ac97 link
#define ICH_GLOB_CNT_AC97WARM  0x00000004 // AC'97 warm reset ( writing a 1 )
#define ICH_GLOB_CNT_AC97COLD  0x00000002 // AC'97 cold reset ( writing a 0 )
#define ICH_GLOB_CNT_GIE       0x00000001 // 1=GPI change causes interrupt

#define ICH_PCM_246_MASK  0x00300000 // bits 20-21: 00=2, 01=4, 02=6 channel mode (not all chips)
#define ICH_PCM_20BIT     0x00400000 // bits 22-23: 00=16, 01=20-bit samples (ICH4)

#define ICH_GLOB_STAT_REG  0x30       // Global Status register (RO)
#define ICH_GLOB_STAT_PCR  0x00000100 // Primary codec is ready for action (software must check these bits before starting the codec!)
#define ICH_GLOB_STAT_RCS  0x00008000 // read completion status: 1=codec read caused timeout 0=read ok
#define ICH_GLOB_STAT_GSCI 0x00000001 // GPI Status Change Interrupt
#define ICH_SAMPLE_CAP     0x00c00000 // bits 22-23: sample capability bits (RO) (ICH4)
#define ICH_SAMPLE_16_20   0x00400000 // bit 22: 0=16, 1=20-bit samples (ICH4)


#define ICH_ACC_SEMA_REG  0x34  // codec write semiphore register
#define ICH_CODEC_BUSY    0x01  // codec register I/O is happening self clearing

#define ICH_BD_IOC        0x8000 //buffer descriptor high word: interrupt on completion (IOC)

#define ICH_DMABUF_PERIODS  32
#define ICH_MAX_CHANNELS     2
#define ICH_MAX_BYTES        4
#define ICH_DMABUF_ALIGN (ICH_DMABUF_PERIODS * ICH_MAX_CHANNELS * ICH_MAX_BYTES) // 256
#if 1 //def SBEMU
#define ICH_INT_INTERVAL     1 //interrupt interval in periods
#endif

#define ICH_DEFAULT_RETRY 1000

struct intel_card_s {
 unsigned long   baseport_bm;       // busmaster baseport
 unsigned long   baseport_codec;    // mixer baseport
 unsigned int    irq;
 unsigned char   device_type;
 struct pci_config_s  *pci_dev;

 struct cardmem_s *dm; /* XMS memory struct */

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
 unsigned int ac97_clock_detected;
 float ac97_clock_corrector;
};

enum { DEVICE_INTEL, DEVICE_INTEL_ICH4, DEVICE_NFORCE };
static char *ich_devnames[3]={"ICH","ICH4","NForce"};

static void snd_intel_measure_ac97_clock( struct audioout_info_s *aui );

//-------------------------------------------------------------------------
// low level write & read

#define snd_intel_write_8(card,reg,data)  outb(card->baseport_bm+reg,data)
#define snd_intel_write_16(card,reg,data) outw(card->baseport_bm+reg,data)
#define snd_intel_write_32(card,reg,data) outl(card->baseport_bm+reg,data)

#define snd_intel_read_8(card,reg)  inb(card->baseport_bm+reg)
#define snd_intel_read_16(card,reg) inw(card->baseport_bm+reg)
#define snd_intel_read_32(card,reg) inl(card->baseport_bm+reg)

static unsigned int snd_intel_codec_ready(struct intel_card_s *card,unsigned int codec)
///////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int retry;

	if(!codec)
		codec = ICH_GLOB_STAT_PCR;

	// wait for codec ready status
	for ( retry = ICH_DEFAULT_RETRY; retry; retry-- ) {
		if(snd_intel_read_32(card,ICH_GLOB_STAT_REG) & codec)
			break;
		pds_delay_10us(10);
	}

	if (!retry) {
		dbgprintf(("snd_intel_codec_ready: timeout\n" ));
	}
	return retry;
}

static void snd_intel_codec_semaphore(struct intel_card_s *card,unsigned int codec)
///////////////////////////////////////////////////////////////////////////////////
{
	unsigned int retry;

	snd_intel_codec_ready(card,codec);

	//wait for semaphore ready (not busy) status
	for ( retry = ICH_DEFAULT_RETRY; retry; retry-- ) {
		if(!(snd_intel_read_8(card,ICH_ACC_SEMA_REG) & ICH_CODEC_BUSY))
			break;
		pds_delay_10us(10);
	}

	if (!retry) {
		dbgprintf(("snd_intel_codec_semaphore: timeout\n" ));
	}
	// clear semaphore flag
	//inw(card->baseport_codec); // (removed for ICH0)
}

static void snd_intel_codec_write(struct intel_card_s *card,unsigned int reg,unsigned int data)
///////////////////////////////////////////////////////////////////////////////////////////////
{
	snd_intel_codec_semaphore(card,ICH_GLOB_STAT_PCR);
	outw(card->baseport_codec + reg,data);
}

static unsigned int snd_intel_codec_read( struct intel_card_s *card, unsigned int reg )
///////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int data = 0,retry;
	snd_intel_codec_semaphore(card,ICH_GLOB_STAT_PCR);

	for ( retry = ICH_DEFAULT_RETRY; retry; retry-- ) {
		data = inw( card->baseport_codec + reg );
		if(!(snd_intel_read_32( card, ICH_GLOB_STAT_REG) & ICH_GLOB_STAT_RCS ) )
			break;
		pds_delay_10us(10);
	}

	if ( !retry ) {
		dbgprintf(("snd_intel_codec_read: timeout\n" ));
	}
	return data;
}

/* init card->dm, card->pcmout, card->virtualpagetable
 */

static unsigned int snd_intel_buffer_init( struct intel_card_s *card, struct audioout_info_s *aui )
///////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int bytes_per_sample = (aui->bits_set > 16) ? 4 : 2;

	card->pcmout_bufsize = MDma_get_max_pcmoutbufsize( aui, 0, ICH_DMABUF_ALIGN, bytes_per_sample, 0 );
	card->dm = MDma_alloc_cardmem(ICH_DMABUF_PERIODS * 2 * sizeof(uint32_t) + card->pcmout_bufsize );
	if (!card->dm) return 0;
	/* pagetable requires 8 byte align; MDma_alloc_cardmem() returns 1kB aligned ptr */
	card->virtualpagetable = (uint32_t *)card->dm->pMem;
	card->pcmout_buffer = ((char *)card->virtualpagetable) + ICH_DMABUF_PERIODS * 2 * sizeof(uint32_t);
	aui->card_DMABUFF = card->pcmout_buffer;
#ifdef SBEMU
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
	cmd = snd_intel_read_32( card, ICH_GLOB_STAT_REG);
	cmd &= ICH_GLOB_STAT_RCS; // ???
	snd_intel_write_32( card, ICH_GLOB_STAT_REG, cmd);

	cmd = snd_intel_read_32(card, ICH_GLOB_CNT_REG);
	cmd &= ~(ICH_GLOB_CNT_ACLINKOFF | ICH_PCM_246_MASK);
	// finish cold or do warm reset
	cmd |= ((cmd & ICH_GLOB_CNT_AC97COLD) == 0) ? ICH_GLOB_CNT_AC97COLD : ICH_GLOB_CNT_AC97WARM;
	snd_intel_write_32(card, ICH_GLOB_CNT_REG, cmd);
	dbgprintf(("snd_intel_chip_init: AC97 reset type: %s\n",((cmd & ICH_GLOB_CNT_AC97COLD) ? "cold":"warm")));

	for ( retry = ICH_DEFAULT_RETRY; retry; retry-- ) {
		unsigned int cntreg = snd_intel_read_32(card,ICH_GLOB_CNT_REG);
		if(!(cntreg & ICH_GLOB_CNT_AC97WARM))
			break;
		pds_delay_10us(10);
	}

	if ( !retry ) {
		dbgprintf(("snd_intel_chip_init: reset timeout\n" ));
	}

	// wait for primary codec ready status
	retry = snd_intel_codec_ready(card,ICH_GLOB_STAT_PCR);
	dbgprintf(("snd_intel_chip_init: primary codec reset timeout:%d\n",retry));

	//snd_intel_codec_read( card, 0); // clear semaphore flag (removed for ICH0)
	snd_intel_write_8( card, ICH_PO_CR_REG, ICH_PO_CR_RESET); // reset channels
#ifdef SBEMU
	pds_delay_10us(2000);
	snd_intel_write_8( card, ICH_PO_CR_REG, /*ICH_PO_CR_LVBIE*/ICH_PO_CR_IOCE );
#endif

	dbgprintf(("snd_intel_chip_init: exit\n"));
}

static void snd_intel_chip_close(struct intel_card_s *card)
///////////////////////////////////////////////////////////
{
	if(card->baseport_bm)
		snd_intel_write_8(card,ICH_PO_CR_REG,ICH_PO_CR_RESET); // reset codec
}

static void snd_intel_ac97_init(struct intel_card_s *card, unsigned int freq_set)
/////////////////////////////////////////////////////////////////////////////////
{
	// initial ac97 volumes (and clear mute flag)
	snd_intel_codec_write(card, AC97_MASTER_VOL_STEREO, 0x0202);
	snd_intel_codec_write(card, AC97_PCMOUT_VOL,        0x0202);
	snd_intel_codec_write(card, AC97_HEADPHONE_VOL,     0x0202);
	snd_intel_codec_write(card, AC97_EXTENDED_STATUS,AC97_EA_SPDIF);

	// set/check variable bit rate bit
	if( freq_set != 48000 ){
		snd_intel_codec_write( card, AC97_EXTENDED_STATUS, AC97_EA_VRA);
		if(snd_intel_codec_read( card, AC97_EXTENDED_STATUS) & AC97_EA_VRA)
			card->vra = 1;
	}
	dbgprintf(("intel_ac97_init: end (vra:%d)\n",card->vra));
}

/*
 * called by ICH_setrate()
 */
static void snd_intel_prepare_playback( struct intel_card_s *card, struct audioout_info_s *aui )
////////////////////////////////////////////////////////////////////////////////////////////////
{
	uint32_t *table_base;
	unsigned int i,cmd,retry,spdif_rate,period_size_samples;

	dbgprintf(("intel_prepare playback: period_size_bytes:%d\n",card->period_size_bytes));

	// wait until DMA stopped ???
	for ( retry = ICH_DEFAULT_RETRY; retry; retry-- ) {
		if(snd_intel_read_8(card,ICH_PO_SR_REG) & ICH_PO_SR_DCH)
			break;
		pds_delay_10us(1);
	}

	if (!retry) {
		dbgprintf(("intel_prepare_playback: dma stop timeout: %d\n",retry));
	}

	// reset codec
	snd_intel_write_8(card,ICH_PO_CR_REG, snd_intel_read_8(card, ICH_PO_CR_REG) | ICH_PO_CR_RESET);

	// set channels (2) and bits (16/32)
	cmd = snd_intel_read_32( card, ICH_GLOB_CNT_REG );
	cmd &= ~(ICH_PCM_246_MASK | ICH_PCM_20BIT);
	if( aui->bits_set > 16 ) {
		if((card->device_type == DEVICE_INTEL_ICH4) && ((snd_intel_read_32(card,ICH_GLOB_STAT_REG) & ICH_SAMPLE_CAP) == ICH_SAMPLE_16_20 )) {
			aui->bits_card = 32;
			cmd |= ICH_PCM_20BIT;
		}
	}
	snd_intel_write_32(card,ICH_GLOB_CNT_REG,cmd);

	// set spdif freq (???)
	switch( aui->freq_card ){
	case 32000:spdif_rate = AC97_SC_SPSR_32K;break;
	case 44100:spdif_rate = AC97_SC_SPSR_44K;break;
	default:spdif_rate = AC97_SC_SPSR_48K;break;
	}
	cmd = snd_intel_codec_read( card, AC97_SPDIF_CONTROL );
	cmd &= AC97_SC_SPSR_MASK;
	cmd |= spdif_rate;
	snd_intel_codec_write( card, AC97_SPDIF_CONTROL, cmd);
	pds_delay_10us(10);

	//set analog ac97 freq
	dbgprintf(("intel_prepare_playback: AC97 front dac freq:%d\n",aui->freq_card));
	if( card->ac97_clock_corrector ){
		if( card->vra )
			snd_intel_codec_write(card,AC97_PCM_FRONT_DAC_RATE,(long)((float)aui->freq_card * card->ac97_clock_corrector));
		else
			aui->freq_card = (long)((float)aui->freq_card / card->ac97_clock_corrector);
	} else
		snd_intel_codec_write( card, AC97_PCM_FRONT_DAC_RATE, aui->freq_card);
	pds_delay_10us(1600);

	//set period table
	table_base = card->virtualpagetable;
	period_size_samples = card->period_size_bytes / (aui->bits_card >> 3);
	for( i = 0; i < ICH_DMABUF_PERIODS; i++ ) {
		table_base[i*2] = pds_cardmem_physicalptr(card->dm, (char *)card->pcmout_buffer + ( i * card->period_size_bytes ));
#ifdef SBEMU
		table_base[i*2+1] = period_size_samples | (ICH_INT_INTERVAL && ((i % ICH_INT_INTERVAL == ICH_INT_INTERVAL-1)) ? (ICH_BD_IOC<<16) : 0);
#else
		table_base[i*2+1] = period_size_samples;
#endif
	}
	snd_intel_write_32(card,ICH_PO_BDBAR_REG, pds_cardmem_physicalptr(card->dm,table_base));

	snd_intel_write_8(card,ICH_PO_LVI_REG,(ICH_DMABUF_PERIODS-1)); // set last index
	snd_intel_write_8(card,ICH_PO_CIV_REG,0); // reset current index

	dbgprintf(("intel_prepare playback end\n"));
}

//-------------------------------------------------------------------------
static const struct pci_device_s ich_devices[] = {
 {"82801AA",0x8086,0x2415, DEVICE_INTEL},
 {"82901AB",0x8086,0x2425, DEVICE_INTEL},
 {"82801BA",0x8086,0x2445, DEVICE_INTEL},
 {"ICH3"   ,0x8086,0x2485, DEVICE_INTEL},
 {"ICH4"   ,0x8086,0x24c5, DEVICE_INTEL_ICH4},
 {"ICH5"   ,0x8086,0x24d5, DEVICE_INTEL_ICH4},
 {"ESB"    ,0x8086,0x25a6, DEVICE_INTEL_ICH4},
 {"ICH6"   ,0x8086,0x266e, DEVICE_INTEL_ICH4},
 {"ICH7"   ,0x8086,0x27de, DEVICE_INTEL_ICH4},
 {"ESB2"   ,0x8086,0x2698, DEVICE_INTEL_ICH4},
 {"440MX"  ,0x8086,0x7195, DEVICE_INTEL}, // maybe doesn't work (needs extra pci hack)
 //{"SI7012" ,0x1039,0x7012, DEVICE_SIS}, // needs extra code
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
			card->pci_dev->device_name,card->baseport_bm,card->irq,
			ich_devnames[card->device_type],((card->device_type == DEVICE_INTEL_ICH4) ? ",20":"")));
#endif
}

static int ICH_adetect( struct audioout_info_s *aui )
/////////////////////////////////////////////////////
{
	struct intel_card_s *card;

	card = (struct intel_card_s *)calloc(1,sizeof(struct intel_card_s));
	if(!card)
		return 0;
	aui->card_private_data = card;

	card->pci_dev = (struct pci_config_s *)calloc(1,sizeof(struct pci_config_s));
	if(!card->pci_dev)
		goto err_adetect;

	if(pcibios_search_devices(ich_devices,card->pci_dev) != PCI_SUCCESSFUL)
		goto err_adetect;

#if 1 //def SBEMU
	if( card->pci_dev->device_type == DEVICE_INTEL_ICH4 ) {
		/*
		 * enable legacy IO space; makes values at ofs 04h/10h/14h R/W.
		 */
		pcibios_WriteConfig_Byte(card->pci_dev, PCIR_CFG, 1); //IOSE:enable IO space
		dbgprintf(("ICH_adetect: enable IO space for ICH4 (PCI reg41h).\n"));
	}
#endif

	dbgprintf(("ICH_adetect: enable PCI io and busmaster\n"));
	pcibios_enable_BM_IO(card->pci_dev);

	card->baseport_bm = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NABMBAR);
	if (!(card->baseport_bm & 1 )) {/* must be an IO address */
		dbgprintf(("ICH_adetect: no IO port for DMA engine set\n"));
		goto err_adetect;
	}
    card->baseport_bm &= ~1; /* just mask out bits 0; bits 1-5 should be 0, since IO space is 64 ports */

#if 0//def SBEMU
	/* Some BIOSes don't set NAMBAR/NABMBAR at all. assign manually.
	 * Probably a bad idea - we don't know what port ranges are free to use -
	 * so if this is done, it should be done by an external tool...
	 */
	int iobase = 0xF000;
	if( card->baseport_bm == 0 ) {
		iobase &= ~0x3F;
		pcibios_WriteConfig_Dword(card->pci_dev, PCIR_NABMBAR, iobase);
		card->baseport_bm = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NABMBAR) & 0xfff0;
	}
#endif
	if(!card->baseport_bm)
		goto err_adetect;

	card->baseport_codec = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NAMBAR);
	if (!(card->baseport_codec & 1 )) { /* must be an IO address */
		dbgprintf(("ICH_adetect: no IO port for Native Audio Mixer set\n"));
		goto err_adetect;
	}
	card->baseport_codec &= ~1; /* just mask out bit 0; bits 1-7 should be 0, since IO space is 256 ports */
#if 0 //def SBEMU
	if( card->baseport_codec == 0 ) {
		iobase -= 256;
		iobase &= ~0xFF;
		pcibios_WriteConfig_Dword(card->pci_dev, PCIR_NAMBAR, iobase);
		card->baseport_codec = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NAMBAR) & 0xfff0;
	}
#endif
	if(!card->baseport_codec)
		goto err_adetect;

	aui->card_irq = card->irq = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_INTR_LN);
#ifdef SBEMU
	/* if no interrupt assigned, assign #11??? A pretty doubtful action - BIOS should know a lot better what IRQs are to be used */
	if( aui->card_irq == 0xFF ) {
		printf(("Intel ICH detection: no IRQ set in PCI config space, try to set it to 11\n"));
		pcibios_WriteConfig_Byte(card->pci_dev, PCIR_INTR_LN, 11);
		aui->card_irq = card->irq = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_INTR_LN);
	}
#endif
 
	card->device_type = card->pci_dev->device_type;

	dbgprintf(("vend/dev_id=%X/%X devtype:%s bmport:%4X mixport:%4X irq:%d\n",
			  card->pci_dev->vendor_id, card->pci_dev->device_id, ich_devnames[card->device_type],card->baseport_bm,card->baseport_codec,card->irq));

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
		MDma_free_cardmem(card->dm);
		if(card->pci_dev)
			free(card->pci_dev);
		free(card);
		aui->card_private_data = NULL;
	}
}

static void ICH_setrate( struct audioout_info_s *aui )
//////////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	unsigned int dmabufsize;
	if((card->device_type == DEVICE_INTEL) && !card->ac97_clock_detected )
		snd_intel_measure_ac97_clock(aui); // called from here because pds_gettimeu() needs int08

	aui->card_wave_id = WAVEID_PCM_SLE;
	aui->chan_card = 2;
	aui->bits_card = 16;

	if(!card->vra){
		aui->freq_card = 48000;
	}else{
		if(aui->freq_card < 8000)
			aui->freq_card = 8000;
		else
			if(aui->freq_card > 48000)
				aui->freq_card = 48000;
	}

	dmabufsize = MDma_init_pcmoutbuf( aui, card->pcmout_bufsize, ICH_DMABUF_ALIGN, 0);
	card->period_size_bytes = dmabufsize / ICH_DMABUF_PERIODS;

	snd_intel_prepare_playback(card,aui);
}

static void ICH_start( struct audioout_info_s *aui )
////////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	unsigned char cmd;

	snd_intel_codec_ready(card,ICH_GLOB_STAT_PCR);

	cmd = snd_intel_read_8(card,ICH_PO_CR_REG);
	cmd |= ICH_PO_CR_START;
	snd_intel_write_8(card,ICH_PO_CR_REG,cmd);
}

static void ICH_stop( struct audioout_info_s *aui )
///////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	unsigned char cmd;

	cmd = snd_intel_read_8(card,ICH_PO_CR_REG);
	cmd &= ~ICH_PO_CR_START;
	snd_intel_write_8(card,ICH_PO_CR_REG,cmd);
}

/* get time in microsecs */

static int64_t gettimeu(void)
/////////////////////////////
{
	int64_t time_ms;
	time_ms = (int64_t)clock() * (int64_t)1000000 / (int64_t)CLOCKS_PER_SEC;
	return time_ms;
}

/* called by ICH_setrate() if device_type == DEVICE_INTEL
 * uses floats!
 * sets ac97_clock_corrector
 */

static void snd_intel_measure_ac97_clock( struct audioout_info_s *aui )
///////////////////////////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	int64_t starttime,endtime,timelen; // in usecs
	long freq_save = aui->freq_card,dmabufsize;
    int cr;

	aui->freq_card = 48000;
	aui->chan_card = 2;
	aui->bits_card = 16;

	dmabufsize = min( card->pcmout_bufsize, AUCARDS_DMABUFSIZE_NORMAL ); // to avoid longer test at -ddma, -ob 24
	dmabufsize = MDma_init_pcmoutbuf( aui, dmabufsize, ICH_DMABUF_ALIGN, 0);
	card->period_size_bytes = dmabufsize / ICH_DMABUF_PERIODS;
	snd_intel_prepare_playback( card, aui);
	MDma_clearbuf(aui);

#ifdef SBEMU
	cr = snd_intel_read_8( card, ICH_PO_CR_REG);
	snd_intel_write_8( card, ICH_PO_CR_REG, 0); //disable LVBIE/IOCE
#endif
	ICH_start(aui);
	starttime = gettimeu();
	do{
		if(snd_intel_read_8(card,ICH_PO_CIV_REG) >= (ICH_DMABUF_PERIODS - 1)) // current index has reached last index
			if(snd_intel_read_8(card,ICH_PO_CIV_REG) >= (ICH_DMABUF_PERIODS - 1)) // verifying
				break;
	} while (gettimeu() <= (starttime + 1000000)); // abort after 1 sec (btw. the test should run less than 0.2 sec only)
	endtime = gettimeu();
	if(endtime > starttime)
		timelen = endtime - starttime;
	else
		timelen = 0;
	ICH_stop(aui);
#ifdef SBEMU
	snd_intel_write_8(card,ICH_PO_CR_REG, cr);
#endif

	if(timelen && (timelen < 1000000)){
		dmabufsize = card->period_size_bytes * (ICH_DMABUF_PERIODS - 1); // the test buflen
		card->ac97_clock_corrector =
			((float)aui->freq_card*aui->chan_card * (aui->bits_card / 8)) // dataspeed (have to be)
			/((float)dmabufsize * 1000000.0 / (float)timelen);            // sentspeed (the measured) (bytes/sec)
		if((card->ac97_clock_corrector > 0.99) && (card->ac97_clock_corrector < 1.01)) // dataspeed==sentspeed
			card->ac97_clock_corrector = 0.0;
		if((card->ac97_clock_corrector < 0.60) || (card->ac97_clock_corrector > 1.5)) // we assume that the result is false
			card->ac97_clock_corrector = 0.0;
	}
	aui->freq_card = freq_save;
	card->ac97_clock_detected = 1;
	//dbgprintf(("snd_intel_measure_ac97_clock: corrector=%1.4f timelen:%d us\n",card->ac97_clock_corrector,(long)timelen));
}

//------------------------------------------------------------------------

static void ICH_writedata( struct audioout_info_s *aui, char *src, unsigned long left )
///////////////////////////////////////////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	unsigned int index;

	MDma_writedata(aui,src,left);

#ifdef SBEMU
	snd_intel_write_8(card,ICH_PO_LVI_REG,(snd_intel_read_8(card, ICH_PO_CIV_REG)-1) % ICH_DMABUF_PERIODS);
#else
	index = aui->card_dmalastput / card->period_size_bytes;
	snd_intel_write_8(card,ICH_PO_LVI_REG,(index-1) % ICH_DMABUF_PERIODS); // set stop position (to keep playing in an endless loop)
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
		index = snd_intel_read_8( card, ICH_PO_CIV_REG );  // number of current period
#ifndef SBEMU
		//dbgprintf(("index1: %d\n",index));
		if(index >= ICH_DMABUF_PERIODS){
			if(retry > 1)
				continue;
			MDma_clearbuf(aui);
			snd_intel_write_8( card, ICH_PO_LVI_REG, (ICH_DMABUF_PERIODS - 1) );
			snd_intel_write_8( card, ICH_PO_CIV_REG, 0);
			aui->card_infobits |= AUINFOS_CARDINFOBIT_DMAUNDERRUN;
			continue;
		}
#endif

		pcmpos = snd_intel_read_16(card,ICH_PO_PICB_REG); // position in the current period (remaining unprocessed in SAMPLEs)
		pcmpos *= aui->bits_card >> 3;
		//pcmpos*=aui->chan_card;
		//printf("%d %d %d %d\n",aui->bits_card, aui->chan_card, pcmpos, card->period_size_bytes);
		//dbgprintf(("ICH_getbufpos: pcmpos=%d\n",pcmpos));
		if(!pcmpos || pcmpos > card->period_size_bytes){
			if( snd_intel_read_8(card,ICH_PO_LVI_REG) == index ) {
				MDma_clearbuf(aui);
				snd_intel_write_8(card,ICH_PO_LVI_REG,(index-1) % ICH_DMABUF_PERIODS); // to keep playing in an endless loop
				//snd_intel_write_8(card,ICH_PO_CIV_REG,index); // ??? -RO
				aui->card_infobits |= AUINFOS_CARDINFOBIT_DMAUNDERRUN;
			}
#ifndef SBEMU
			continue;
#endif
		}
#ifndef SBEMU
		if(snd_intel_read_8(card,ICH_PO_CIV_REG) != index) // verifying
			continue;
#endif

		pcmpos = card->period_size_bytes - pcmpos;
		bufpos = index * card->period_size_bytes + pcmpos;

		if( bufpos < aui->card_dmasize ) {
			aui->card_dma_lastgoodpos = bufpos;
			break;
		}

	}while(--retry);

	dbgprintf(("ICH_getbufpos: pos=%d dmasize=%d, dma_lastgoodpos=%d\n", bufpos, aui->card_dmasize, aui->card_dma_lastgoodpos ));

	return aui->card_dma_lastgoodpos;
}

//--------------------------------------------------------------------------
//mixer

static void ICH_writeMIXER( struct audioout_info_s *aui, unsigned long reg, unsigned long val )
///////////////////////////////////////////////////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	snd_intel_codec_write(card,reg,val);
}

static unsigned long ICH_readMIXER( struct audioout_info_s *aui, unsigned long reg )
////////////////////////////////////////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	return snd_intel_codec_read(card,reg);
}

#if 1 /* vsbhda */
static int ICH_IRQRoutine( struct audioout_info_s* aui )
////////////////////////////////////////////////////////
{
	struct intel_card_s *card = aui->card_private_data;
	int status = snd_intel_read_8(card,ICH_PO_SR_REG);
	status &= ICH_PO_SR_LVBCI | ICH_PO_SR_BCIS | ICH_PO_SR_FIFO;
	if(status)
		snd_intel_write_8(card, ICH_PO_SR_REG, status); //ack
	return status != 0;
}
#endif

const struct sndcard_info_s ICH_sndcard_info = {
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
