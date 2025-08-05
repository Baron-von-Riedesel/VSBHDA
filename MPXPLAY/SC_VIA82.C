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
//function: VIA VT82C686, VT8233/VT8235?/VT8237(R)? low level routines (onboard chips on AMD Athlon mainboards)
//some routines are based on the ALSA (http://www.alsa-project.org)

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#ifndef DJGPP
#include <conio.h>
#endif

#include "CONFIG.H"
#include "MPXPLAY.H"
#include "DMAIRQ.H"
#include "PCIBIOS.H"
#include "AC97.H"

#define SETPCMVOL 0 /* 1=PCM/HP vol depending on /VOL, 0=PCM/HP vol max */
#if SETPCMVOL
#define PCMVOL 0x02
#else
#define PCMVOL 0x00
#endif
#define DXS_VOLUME 0 /* vsbhda: originally was 0x02 */


// ac97: is a dword (RW) at port offset 0x80
#define VIA_REG_AC97_CTRL               0x80
#define VIA_REG_AC97_CODEC_ID_PRIMARY   0x00000000 /* codec mask bits 30-31 */
#define VIA_REG_AC97_PRIMARY_VALID      (1<<25) /* 25=codec 0, 27-29=codec 1-3 */
#define VIA_REG_AC97_BUSY               (1<<24)
#define VIA_REG_AC97_READ               (1<<23)
#define VIA_REG_AC97_WRITE              0
#define VIA_REG_AC97_CMD_SHIFT          16
#define VIA_REG_AC97_CMD_MASK           0x7F
#define VIA_REG_AC97_DATA_MASK          0xffff

/* 82C686 + 8233/35/37
 * 4 DMA channels at port offsets 0x, 1x, 2x, 3x
 */

#define VIA_REG_OFFSET_STATUS           0x00    /* byte - channel status */
#define VIA_REG_STATUS_FLAG 0x01 /* 1=block complete */
#define VIA_REG_STATUS_EOL  0x02 /* 1=block is last of the link (EndOfLink) */

#define VIA_REG_OFFSET_CONTROL        0x01  /* byte - channel control */
#define VIA_REG_CTRL_START            0x80  /* WO */
#define VIA_REG_CTRL_TERMINATE        0x40  /* WO */
#define VIA_REG_CTRL_PAUSE            0x08  /* RW */
#define VIA_REG_CTRL_RESET            0x01  /* 82C686: RW - probably reset? undocumented */

/* offset 2 for 82c686 only ( is volume-L for 8233/35/37 ) */
#define VIA686_REG_OFFSET_TYPE           0x02  /* byte - channel type */
#define VIA686_REG_TYPE_INT_LSAMPLE      0x04  /* interrupt on last sample sent */
#define VIA686_REG_TYPE_INT_EOL          0x02  /* interrupt on end of link */
#define VIA686_REG_TYPE_INT_FLAG         0x01  /* interrupt on flag */

#define VIA_REG_OFFSET_TABLE_PTR     0x04    /* dword - channel table pointer (W) */
#define VIA_REG_OFFSET_CURR_PTR      0x04    /* dword - channel current pointer (R) */
#define VIA_REG_PLAYBACK_CURR_COUNT  0x0C    /* dword - channel current count */

// VT8233/35/37
#define VIA_REG_CTRL_AUTOSTART       0x20 /* 1=autostart */
#define VIA_REG_CTRL_INT_STOP_IDX    0x04 /* 1=interrupt on stop index */
#define VIA_REG_CTRL_INT_EOL         0x02 /* 1=interrupt on EOL */
#define VIA_REG_CTRL_INT_FLAG        0x01 /* 1=interrupt on FLAG */

#define VIA_REG_OFFSET_STOP_IDX      0x08    /* dword - stop index (24-31), channel type, sample rate (0-19) */

#define VIA_REG_TYPE_AUTOSTART       0x80    /* RW - autostart at EOL */
#define VIA_REG_TYPE_16BIT           0x20    /* RW */
#define VIA_REG_TYPE_STEREO          0x10    /* RW */
#define VIA8233_REG_TYPE_16BIT       0x00200000    /* RW */
#define VIA8233_REG_TYPE_STEREO      0x00100000    /* RW */
#define VIA_REG_OFFSET_CURR_INDEX    0x0f    /* byte - channel current index */
#define VIA_REG_OFS_PLAYBACK_VOLUME_L 0x02
#define VIA_REG_OFS_PLAYBACK_VOLUME_R 0x03

#define VIA_TBL_BIT_EOL        0x80000000
#define VIA_TBL_BIT_FLAG       0x40000000
#define VIA_TBL_BIT_STOP       0x20000000

/* PCI config space registers */
#define VIA_ACLINK_STAT          0x40
#define  VIA_ACLINK_C11_READY    0x20
#define  VIA_ACLINK_C10_READY    0x10
#define  VIA_ACLINK_C01_READY    0x04 /* secondary codec ready */
#define  VIA_ACLINK_LOWPOWER     0x02 /* low-power state */
#define  VIA_ACLINK_C00_READY    0x01 /* primary codec ready */
#define VIA_ACLINK_CTRL          0x41
#define  VIA_ACLINK_CTRL_ENABLE  0x80 /* 0: disable, 1: enable */
#define  VIA_ACLINK_CTRL_RESET   0x40 /* 0: assert, 1: de-assert */
#define  VIA_ACLINK_CTRL_SYNC    0x20 /* 0: release SYNC, 1: force SYNC hi */
#define  VIA_ACLINK_CTRL_SDO     0x10 /* 0: release SDO, 1: force SDO hi */
#define  VIA_ACLINK_CTRL_VRA     0x08 /* 0: disable VRA, 1: enable VRA */
#define  VIA_ACLINK_CTRL_PCM     0x04 /* 0: disable PCM, 1: enable PCM */
#define  VIA_ACLINK_CTRL_FM      0x02 /* via686 only; not used */
#define  VIA_ACLINK_CTRL_SB      0x01 /* via686 only; not used */

#define  VIA_ACLINK_CTRL_INIT    (VIA_ACLINK_CTRL_ENABLE | VIA_ACLINK_CTRL_RESET | VIA_ACLINK_CTRL_PCM | VIA_ACLINK_CTRL_VRA)

#define PCI_VENDOR_ID_VIA        0x1106
#define PCI_DEVICE_ID_VT82C686   0x3058
#define PCI_DEVICE_ID_VT8233     0x3059 /* also for 8235/8237(R) */

#define VIRTUALPAGETABLESIZE   4096
#define PCMBUFFERPAGESIZE      512//4096 //page size determines the interrupt interval

#define VIA_INT_INTERVAL 1

struct via82xx_card
{
 unsigned long   iobase;
 unsigned short  model;
 unsigned int    irq;
 unsigned char   chiprev;
 struct pci_config_s  *pci_dev;

 struct cardmem_s *dm;
 unsigned long *virtualpagetable;
 char *pcmout_buffer;
 long pcmout_bufsize;
 int pcmout_pages;
 int pagesize; /* v1.7 */
};

static void via82xx_AC97Codec_ready(unsigned int baseport);
static void via82xx_ac97_write(unsigned int baseport,unsigned int reg, unsigned int value);
static unsigned int via82xx_ac97_read(unsigned int baseport, unsigned int reg);
static void via82xx_dxs_write(unsigned int baseport,unsigned int reg, unsigned int val);

static unsigned int via8233_dxs_volume = DXS_VOLUME;

static void via82xx_channel_reset(struct via82xx_card *card)
////////////////////////////////////////////////////////////
{
	unsigned int baseport = card->iobase;

	outb(baseport + VIA_REG_OFFSET_CONTROL, VIA_REG_CTRL_PAUSE | VIA_REG_CTRL_TERMINATE | VIA_REG_CTRL_RESET);
	pds_delay_10us(5);
	outb(baseport + VIA_REG_OFFSET_CONTROL, 0x00);
	outb(baseport + VIA_REG_OFFSET_STATUS, 0xFF);
	if(card->pci_dev->device_id == PCI_DEVICE_ID_VT82C686)
		outb(baseport + VIA686_REG_OFFSET_TYPE, 0x00);
	outl(baseport + VIA_REG_OFFSET_CURR_PTR, 0);
}

static void via82xx_chip_init(struct via82xx_card *card)
////////////////////////////////////////////////////////
{
	unsigned int data,retry;

	/* deassert ACLink reset, force SYNC */
	pcibios_WriteConfig_Byte(card->pci_dev, VIA_ACLINK_CTRL, VIA_ACLINK_CTRL_ENABLE | VIA_ACLINK_CTRL_RESET | VIA_ACLINK_CTRL_SYNC); // 0xe0
	pds_delay_10us(10);
	if( card->pci_dev->device_id == PCI_DEVICE_ID_VT82C686 ) {
		// full reset
		pcibios_WriteConfig_Byte(card->pci_dev, VIA_ACLINK_CTRL, 0x00);
		pds_delay_10us(10);
		/* ACLink on, deassert ACLink reset, VSR, SGD data out */
		/* note - FM data out has trouble with non VRA codecs !! */
		pcibios_WriteConfig_Byte(card->pci_dev, VIA_ACLINK_CTRL, VIA_ACLINK_CTRL_INIT);// ??? SB   (0xCD)
		pds_delay_10us(10);
	}else{
		/* deassert ACLink reset, force SYNC (warm AC'97 reset) */
		pcibios_WriteConfig_Byte(card->pci_dev, VIA_ACLINK_CTRL, VIA_ACLINK_CTRL_RESET | VIA_ACLINK_CTRL_SYNC); // 0x60
		pds_delay_10us(1);
		/* ACLink on, deassert ACLink reset, VSR, SGD data out */
		pcibios_WriteConfig_Byte(card->pci_dev, VIA_ACLINK_CTRL, VIA_ACLINK_CTRL_INIT); // 0xcc
		pds_delay_10us(10);
	}

	// Make sure VRA is enabled, in case we didn't do a complete codec reset, above
	data = pcibios_ReadConfig_Byte(card->pci_dev, VIA_ACLINK_CTRL);
	if((data & VIA_ACLINK_CTRL_INIT) != VIA_ACLINK_CTRL_INIT) {
		pcibios_WriteConfig_Byte(card->pci_dev, VIA_ACLINK_CTRL, VIA_ACLINK_CTRL_INIT);
		pds_delay_10us(10);
	}

	// wait until codec ready
	retry = 65536;
	do{
		data = pcibios_ReadConfig_Byte(card->pci_dev, VIA_ACLINK_STAT);
		if(data & VIA_ACLINK_C00_READY) /* primary codec ready */
			break;
		pds_delay_10us(1);
	}while(--retry);

	//reset ac97
	via82xx_AC97Codec_ready( card->iobase );
	via82xx_ac97_write( card->iobase, AC97_RESET, 0 );
	via82xx_ac97_read( card->iobase, AC97_RESET );

	via82xx_channel_reset( card );

	if(card->pci_dev->device_id != PCI_DEVICE_ID_VT82C686){
		// Workaround for Award BIOS bug:
		// DXS channels don't work properly with VRA if MC97 is disabled.
		struct pci_config_s pci;
		if( pcibios_FindDevice(0x1106, 0x3068, &pci) == PCI_SUCCESSFUL){ /* MC97 */
			data = pcibios_ReadConfig_Byte(&pci, 0x44);
			pcibios_WriteConfig_Byte(&pci, 0x44, data | 0x40 );
		}
	}

	// initial ac97 volumes (and clear mute flag)
	via82xx_ac97_write(card->iobase, AC97_MASTER_VOL_STEREO, 0x0202);
	via82xx_ac97_write(card->iobase, AC97_PCMOUT_VOL,        PCMVOL);
	via82xx_ac97_write(card->iobase, AC97_HEADPHONE_VOL,     PCMVOL);
	via82xx_ac97_write(card->iobase, AC97_EXTENDED_STATUS, AC97_EA_SPDIF);
}

static void via82xx_chip_close(struct via82xx_card *card)
/////////////////////////////////////////////////////////
{
	via82xx_channel_reset(card);
}

static void via82xx_set_table_ptr(struct via82xx_card *card)
////////////////////////////////////////////////////////////
{
	via82xx_AC97Codec_ready(card->iobase);
	outl(card->iobase + VIA_REG_OFFSET_TABLE_PTR, pds_cardmem_physicalptr(card->dm,card->virtualpagetable));
	pds_delay_10us(2);
	via82xx_AC97Codec_ready(card->iobase);
}

//-------------------------------------------------------------------------
static const struct pci_device_s via_devices[] = {
 {"VT82C686",PCI_VENDOR_ID_VIA,PCI_DEVICE_ID_VT82C686},
 {"VT8233"  ,PCI_VENDOR_ID_VIA,PCI_DEVICE_ID_VT8233},
 {NULL,0,0}
};

static void VIA82XX_close(struct audioout_info_s *aui);

static void VIA82XX_card_info(struct audioout_info_s *aui)
//////////////////////////////////////////////////////////
{
	struct via82xx_card *card = aui->card_private_data;
	dbgprintf(("VIA : %s soundcard found on port:%4X irq:%d chiprev:%2X model:%4X\n",
		   card->pci_dev->device_name,card->iobase,card->irq,card->chiprev,card->model));
}

static int VIA82XX_adetect(struct audioout_info_s *aui)
///////////////////////////////////////////////////////
{
	struct via82xx_card *card;

	card = (struct via82xx_card *)calloc(1,sizeof(struct via82xx_card));
	if(!card)
		return 0;
	aui->card_private_data = card;

	card->pci_dev = (struct pci_config_s *)calloc(1,sizeof(struct pci_config_s));
	if(!card->pci_dev)
		goto err_adetect;

	if(pcibios_search_devices(via_devices,card->pci_dev) != PCI_SUCCESSFUL)
		goto err_adetect;
	pcibios_enable_BM_IO(card->pci_dev);

	card->iobase = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NAMBAR);
	if( !( card->iobase & 1) || !(card->iobase & 0xfff0 ) ) {
		printf("VIA 82XX: no base port set for AC97 controller\n");
		goto err_adetect;
	}
    card->iobase &= 0xFFF0;
	card->irq    = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_INTR_LN);
	card->chiprev= pcibios_ReadConfig_Byte(card->pci_dev, PCIR_RID);
	card->model  = pcibios_ReadConfig_Word(card->pci_dev, PCIR_SSID);
#if 1 /* modifying the IRQ? */
	aui->card_irq = card->irq;
	dbgprintf(("VIA82XX_adetect: irq=%d\n",aui->card_irq));
	if(aui->card_irq == 0 || aui->card_irq == 0xFF) {
		printf("VIA82XX_adetect: no IRQ set, setting to 10\n");
		aui->card_irq = card->irq = 10;
		pcibios_WriteConfig_Byte(card->pci_dev, PCIR_INTR_LN, aui->card_irq); //RW
	}
#endif

	/* v1.7: use /PS cmdline value if available */
	card->pagesize = ( aui->gvars->period_size ? aui->gvars->period_size : PCMBUFFERPAGESIZE );
	// alloc buffers
	card->pcmout_bufsize = MDma_get_max_pcmoutbufsize( aui, 0, card->pagesize, 2, 0 );

	card->dm = MDma_alloc_cardmem( VIRTUALPAGETABLESIZE + card->pcmout_bufsize + 4096 );
	if (!card->dm)
		return 0;

	card->virtualpagetable = (void *)(((uint32_t)card->dm->pMem + 4095) & (~4095));
	card->pcmout_buffer = (char *)card->virtualpagetable + VIRTUALPAGETABLESIZE;

#ifdef SBEMU
	memset(card->virtualpagetable, 0, VIRTUALPAGETABLESIZE);
	memset(card->pcmout_buffer, 0, card->pcmout_bufsize);
#endif

	aui->card_DMABUFF = card->pcmout_buffer;

	// init chip
	via82xx_chip_init(card);

	return 1;

err_adetect:
	VIA82XX_close(aui);
	return 0;
}

static void VIA82XX_close(struct audioout_info_s *aui)
//////////////////////////////////////////////////////
{
	struct via82xx_card *card = aui->card_private_data;

	dbgprintf(("VIA82XX_close\n"));
	if(card){
		if(card->iobase)
			via82xx_chip_close(card);
		MDma_free_cardmem(card->dm);
		if(card->pci_dev)
			free(card->pci_dev);
		free(card);
		aui->card_private_data = NULL;
	}
}

static void VIA82XX_setrate(struct audioout_info_s *aui)
////////////////////////////////////////////////////////
{
	struct via82xx_card *card = aui->card_private_data;
	unsigned int dmabufsize,pagecount,spdif_rate;
	unsigned long pcmbufp;


	dbgprintf(("VIA82XX_setrate\n"));
	if(aui->freq_card < 4000)
		aui->freq_card = 4000;
	else{
		if(aui->freq_card > 48000)
			aui->freq_card = 48000;
	}

	aui->chan_card = 2;
	aui->bits_card = 16;
	aui->card_wave_id = WAVEID_PCM_SLE;

	dmabufsize = MDma_init_pcmoutbuf(aui, card->pcmout_bufsize, card->pagesize, 0);

	// page tables
	//card->pcmout_pages = dmabufsize / PCMBUFFERPAGESIZE;
	card->pcmout_pages = dmabufsize / card->pagesize;
	pcmbufp = (unsigned long)card->pcmout_buffer;
	dbgprintf(("VIA82XX_setrate: PCM pages=%u\n", card->pcmout_pages));
 
	for( pagecount = 0; pagecount < card->pcmout_pages; pagecount++) {
		card->virtualpagetable[pagecount * 2] = pds_cardmem_physicalptr(card->dm,pcmbufp);
		if( pagecount < (card->pcmout_pages - 1)) {
#if 1//def SBEMU
			//card->virtualpagetable[pagecount * 2 + 1] = ((pagecount % VIA_INT_INTERVAL) == VIA_INT_INTERVAL - 1) ? VIA_TBL_BIT_FLAG | PCMBUFFERPAGESIZE : PCMBUFFERPAGESIZE;
			card->virtualpagetable[pagecount * 2 + 1] = ((pagecount % VIA_INT_INTERVAL) == VIA_INT_INTERVAL - 1) ? VIA_TBL_BIT_FLAG | card->pagesize : card->pagesize;
#else
			card->virtualpagetable[pagecount * 2 + 1] = PCMBUFFERPAGESIZE; // 0x00001000; // period continues to the next
#endif
		} else
			//card->virtualpagetable[pagecount * 2 + 1] = VIA_TBL_BIT_EOL | PCMBUFFERPAGESIZE; // 0x80001000; // buffer boundary
			card->virtualpagetable[pagecount * 2 + 1] = VIA_TBL_BIT_EOL | card->pagesize; // 0x80001000; // buffer boundary

		pcmbufp += card->pagesize;
	}

	// ac97 config
	via82xx_ac97_write(card->iobase, AC97_EXTENDED_STATUS, AC97_EA_VRA); //this is a bug so SBEMU macro not added
	via82xx_ac97_write(card->iobase, AC97_PCM_FRONT_DAC_RATE, aui->freq_card);

	switch(aui->freq_card){
	case 32000: spdif_rate = AC97_SC_SPSR_32K;break;
	case 44100: spdif_rate = AC97_SC_SPSR_44K;break;
	default: spdif_rate = AC97_SC_SPSR_48K;break;
	}
	via82xx_ac97_write(card->iobase, AC97_SPDIF_CONTROL, spdif_rate); // ???
	pds_delay_10us(10);

	// via hw config
	via82xx_channel_reset(card);
	via82xx_set_table_ptr(card);

	if( card->pci_dev->device_id == PCI_DEVICE_ID_VT82C686 ) {
		outb(card->iobase + VIA686_REG_OFFSET_TYPE,
			 VIA_REG_TYPE_AUTOSTART | VIA_REG_TYPE_16BIT | VIA_REG_TYPE_STEREO );
	} else { // VT8233
		unsigned int rbits;
		// init dxs volume (??? here?)
		via82xx_dxs_write(card->iobase,VIA_REG_OFS_PLAYBACK_VOLUME_L, via8233_dxs_volume);
		via82xx_dxs_write(card->iobase,VIA_REG_OFS_PLAYBACK_VOLUME_R, via8233_dxs_volume);
		// freq
		if(aui->freq_card == 48000)
			rbits = 0xfffff;
		else
#ifdef SBEMU
			rbits = (0x100000 / 48000) * aui->freq_card;
#else
			rbits = (0x100000 / 48000) * aui->freq_card + ((0x100000 % 48000) * aui->freq_card) / 48000;
#endif
		outl(card->iobase + VIA_REG_OFFSET_STOP_IDX, VIA8233_REG_TYPE_16BIT | VIA8233_REG_TYPE_STEREO | rbits | 0xFF000000);
	}
	pds_delay_10us(2);
	via82xx_AC97Codec_ready(card->iobase);
}

static void VIA82XX_start(struct audioout_info_s *aui)
//////////////////////////////////////////////////////
{
	struct via82xx_card *card = aui->card_private_data;
	dbgprintf(("VIA82XX_start\n"));
	if(card->pci_dev->device_id == PCI_DEVICE_ID_VT82C686) {
#ifdef SBEMU
		outb(card->iobase + VIA686_REG_OFFSET_TYPE, inb( card->iobase + VIA686_REG_OFFSET_TYPE ) | VIA686_REG_TYPE_INT_LSAMPLE | VIA686_REG_TYPE_INT_EOL | VIA686_REG_TYPE_INT_FLAG);
#endif
		outb(card->iobase + VIA_REG_OFFSET_CONTROL, VIA_REG_CTRL_START);
	} else
#ifdef SBEMU
		outb(card->iobase + VIA_REG_OFFSET_CONTROL, VIA_REG_CTRL_START | VIA_REG_CTRL_AUTOSTART | VIA_REG_CTRL_INT_FLAG | VIA_REG_CTRL_INT_EOL );
#else
		outb(card->iobase + VIA_REG_OFFSET_CONTROL, VIA_REG_CTRL_START | VIA_REG_CTRL_AUTOSTART);
#endif
}

static void VIA82XX_stop(struct audioout_info_s *aui)
/////////////////////////////////////////////////////
{
	struct via82xx_card *card = aui->card_private_data;
	dbgprintf(("VIA82XX_stop\n"));
	outb(card->iobase + VIA_REG_OFFSET_CONTROL, VIA_REG_CTRL_PAUSE);
}

/* VIA82 implementation of cardbuf_getpos() */

static long VIA82XX_getbufpos(struct audioout_info_s *aui)
//////////////////////////////////////////////////////////
{
	struct via82xx_card *card = aui->card_private_data;
	unsigned int baseport = card->iobase;
	unsigned long idx,count,bufpos;

	if( card->pci_dev->device_id == PCI_DEVICE_ID_VT82C686 ){
		count = inl(baseport + VIA_REG_PLAYBACK_CURR_COUNT);
		idx   = inl(baseport + VIA_REG_OFFSET_CURR_PTR);
		if(idx <= (unsigned long)card->virtualpagetable)
			idx = 0;
		else{
			idx = idx - (unsigned long)card->virtualpagetable;
			idx = idx >> 3; // 2 * 4 bytes
			idx = idx - 1;
			idx = idx % card->pcmout_pages;
		}
	} else { // VT8233/8235
		count = inl(baseport + VIA_REG_PLAYBACK_CURR_COUNT);
		idx   = count >> 24;
	}
	count &= 0xffffff;

	/* vsbhda: count may be 0 */
	//if(count && (count <= PCMBUFFERPAGESIZE)){
	if ( ( card->pci_dev->device_id != PCI_DEVICE_ID_VT82C686 ) || ( count && ( count <= card->pagesize ))) {

		bufpos = (idx * card->pagesize) + card->pagesize - count;

		if(bufpos < aui->card_dmasize)
			aui->card_dma_lastgoodpos = bufpos;
	}

	dbgprintf(("VIA82XX_getbufpos: %X\n", aui->card_dma_lastgoodpos ));
	return aui->card_dma_lastgoodpos;
}

static void VIA82XX_clearbuf(struct audioout_info_s *aui)
/////////////////////////////////////////////////////////
{
	MDma_clearbuf(aui);
}

//mixer

static unsigned long via82xx_ReadAC97Codec_sub(unsigned int baseport)
/////////////////////////////////////////////////////////////////////
{
	unsigned long d0;
	int retry = 2048;

	do{
		d0 = inl(baseport + VIA_REG_AC97_CTRL);
		if( (d0 & VIA_REG_AC97_PRIMARY_VALID) != 0 )
			break;
		pds_delay_10us(1);
	}while(--retry);

	d0 = inl(baseport + VIA_REG_AC97_CTRL);
	return d0;
}

static void via82xx_AC97Codec_ready(unsigned int baseport)
//////////////////////////////////////////////////////////
{
	unsigned long d0;
	int retry = 2048;

	do{
		d0 = inl(baseport + VIA_REG_AC97_CTRL);
		if( (d0 & VIA_REG_AC97_BUSY) == 0 )
			break;
		pds_delay_10us(1);
	}while(--retry);
}

static void via82xx_WriteAC97Codec_sub(unsigned int baseport,unsigned long value)
/////////////////////////////////////////////////////////////////////////////////
{
	via82xx_AC97Codec_ready(baseport);
	outl(baseport + VIA_REG_AC97_CTRL, value);
	via82xx_AC97Codec_ready(baseport);
}

static void via82xx_ac97_write(unsigned int baseport,unsigned int reg, unsigned int value)
//////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned long d0;

	reg   &= VIA_REG_AC97_CMD_MASK;
	value &= VIA_REG_AC97_DATA_MASK;
	d0 = VIA_REG_AC97_CODEC_ID_PRIMARY | VIA_REG_AC97_WRITE | (reg << VIA_REG_AC97_CMD_SHIFT) | value;
	via82xx_WriteAC97Codec_sub(baseport,d0);
	via82xx_WriteAC97Codec_sub(baseport,d0);
}

static unsigned int via82xx_ac97_read(unsigned int baseport, unsigned int reg)
//////////////////////////////////////////////////////////////////////////////
{
	long d0;

	reg &= VIA_REG_AC97_CMD_MASK;
	d0 = VIA_REG_AC97_CODEC_ID_PRIMARY | VIA_REG_AC97_READ | (reg << VIA_REG_AC97_CMD_SHIFT);
	via82xx_WriteAC97Codec_sub(baseport,d0);
	via82xx_WriteAC97Codec_sub(baseport,d0);

	return via82xx_ReadAC97Codec_sub(baseport);
}

static void via82xx_dxs_write(unsigned int baseport,unsigned int reg, unsigned int val)
///////////////////////////////////////////////////////////////////////////////////////
{
	outb(baseport+reg+0x00,val);
	//outb(baseport+reg+0x10,val);
	//outb(baseport+reg+0x20,val);
	//outb(baseport+reg+0x30,val);
	via8233_dxs_volume = val;
}

static unsigned int via82xx_dxs_read(unsigned int baseport,unsigned int reg)
////////////////////////////////////////////////////////////////////////////
{
	return via8233_dxs_volume; // is the dxs write-only?
}

static void VIA82XX_writeMIXER(struct audioout_info_s *aui,unsigned long reg, unsigned long val)
////////////////////////////////////////////////////////////////////////////////////////////////
{
	struct via82xx_card *card = aui->card_private_data;

	//if((reg == VIA_REG_OFS_PLAYBACK_VOLUME_L) || (reg == VIA_REG_OFS_PLAYBACK_VOLUME_R)){
	if( reg >= 256){ // VIA_REG_OFS_PLAYBACK_VOLUME_X
		if(card->pci_dev->device_id != PCI_DEVICE_ID_VT82C686)
			via82xx_dxs_write(card->iobase,(reg >> 8),val);
	}else
		via82xx_ac97_write(card->iobase,reg,val);
}

static unsigned long VIA82XX_readMIXER(struct audioout_info_s *aui,unsigned long reg)
/////////////////////////////////////////////////////////////////////////////////////
{
	struct via82xx_card *card = aui->card_private_data;
	unsigned int retval = 0;

	//if((reg == VIA_REG_OFS_PLAYBACK_VOLUME_L) || (reg == VIA_REG_OFS_PLAYBACK_VOLUME_R)){
	if(reg >= 256){ // VIA_REG_OFS_PLAYBACK_VOLUME_X
		if(card->pci_dev->device_id != PCI_DEVICE_ID_VT82C686)
			retval = via82xx_dxs_read(card->iobase,(reg >> 8));
	}else
		retval = via82xx_ac97_read(card->iobase,reg);

	return retval;
}

#if 1 /* vsbhda */

static int VIA82XX_IRQRoutine(struct audioout_info_s* aui)
//////////////////////////////////////////////////////////
{
	struct via82xx_card *card = aui->card_private_data;

	int status = inb(card->iobase + VIA_REG_OFFSET_STATUS) & (VIA_REG_STATUS_FLAG | VIA_REG_STATUS_EOL);
	if(status)
		outb(card->iobase + VIA_REG_OFFSET_STATUS, status);
	return status != 0;
}
#endif

/////////////////////////////////////////////////////////////////

/* VIA_REG_OFS_PLAYBACK_VOLUME_x:
 * 8233: 5 bits
 * 8235: 6 bits
 * 8237: 6 bits
 */

static const struct aucards_mixerchan_s via82xx_master_vol = {
	AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME), 2, {
		{ AC97_MASTER_VOL_STEREO, 0x3f, 8, SUBMIXCH_INFOBIT_REVERSEDVALUE }, // left
		{ AC97_MASTER_VOL_STEREO, 0x3f, 0, SUBMIXCH_INFOBIT_REVERSEDVALUE }, // right
		//{(VIA_REG_OFS_PLAYBACK_VOLUME_L << 8),0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE},
		//{(VIA_REG_OFS_PLAYBACK_VOLUME_R << 8),0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}
	}};

#if SETPCMVOL
static const struct aucards_mixerchan_s via82xx_pcm_vol = {
	AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_PCM,AU_MIXCHANFUNC_VOLUME), 2, {
		{ AC97_PCMOUT_VOL, 0x3f, 8, SUBMIXCH_INFOBIT_REVERSEDVALUE },
		{ AC97_PCMOUT_VOL, 0x3f, 0, SUBMIXCH_INFOBIT_REVERSEDVALUE },
		//{(VIA_REG_OFS_PLAYBACK_VOLUME_L << 8),0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}, // DXS channels
		//{(VIA_REG_OFS_PLAYBACK_VOLUME_R << 8),0x1f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE}
	}};

static const struct aucards_mixerchan_s via82xx_headphone_vol = {
	AU_MIXCHANFUNCS_PACK(AU_MIXCHAN_HEADPHONE,AU_MIXCHANFUNC_VOLUME), 2, {
		{ AC97_HEADPHONE_VOL,0x3f,8,SUBMIXCH_INFOBIT_REVERSEDVALUE },
		{ AC97_HEADPHONE_VOL,0x3f,0,SUBMIXCH_INFOBIT_REVERSEDVALUE }
	}};
#endif

static const struct aucards_mixerchan_s *via82xx_mixerset[] = {
 &via82xx_master_vol,
#if SETPCMVOL
 &via82xx_pcm_vol,
 &via82xx_headphone_vol,
#endif
 NULL
};

const struct sndcard_info_s VIA82XX_sndcard_info = {
 "VIA VT82XX AC97",
 0,
 NULL,
 NULL,                  // no init
 &VIA82XX_adetect,      // only autodetect
 &VIA82XX_card_info,
 &VIA82XX_start,
 &VIA82XX_stop,
 &VIA82XX_close,
 &VIA82XX_setrate,

 &MDma_writedata,
 &VIA82XX_getbufpos,
 &VIA82XX_clearbuf,
 &VIA82XX_IRQRoutine, /* vsbhda */

 &VIA82XX_writeMIXER,
 &VIA82XX_readMIXER,
 via82xx_mixerset
};
