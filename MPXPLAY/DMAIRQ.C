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
//function: DMA & IRQ handling
//based on the MPG123 (DOS)

#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <io.h>
#include <dos.h>
#include <string.h>

#include "SBEMUCFG.H"
#include "MPXPLAY.H"
#include "DMAIRQ.H"

//declared in control.c
//extern struct mpxplay_audioout_info_s au_infos;
//extern unsigned int intsoundconfig,intsoundcontrol;

//**************************************************************************
// DMA functions
//**************************************************************************

//-----------------------------------------------------------------------
//common (ISA & PCI)

cardmem_t *MDma_alloc_cardmem(unsigned int buffsize)
////////////////////////////////////////////////////
{
    dbgprintf("MDma_alloc_cardmem\n");
	cardmem_t *dm;
	dm=calloc(1,sizeof(cardmem_t));
	if(!dm)
		return NULL;
	if(!pds_dpmi_xms_allocmem(dm,buffsize)) {
		free(dm);
		return NULL;
	}
	memset( dm->linearptr, 0, buffsize);
	dbgprintf("MDma_alloc_cardmem: %X\n", dm->linearptr);
	return dm;
}

void MDma_free_cardmem(cardmem_t *dm)
/////////////////////////////////////
{
    dbgprintf("MDma_free_cardmem\n");
	if(dm){
		pds_dpmi_xms_freemem(dm);
		free(dm);
	}
}

unsigned int MDma_get_max_pcmoutbufsize(struct mpxplay_audioout_info_s *aui,unsigned int max_bufsize,unsigned int pagesize,unsigned int samplesize,unsigned long freq_config)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int bufsize;
	dbgprintf("MDma_get_max_pcmoutbufsize\n");
	if(!max_bufsize)
		max_bufsize=AUCARDS_DMABUFSIZE_MAX;
	if(!pagesize)
		pagesize=AUCARDS_DMABUFSIZE_BLOCK;
	if(samplesize<2)
		samplesize=2;
	bufsize=AUCARDS_DMABUFSIZE_NORMAL/2; // samplesize/=2;

	if(freq_config)
		bufsize=(int)((float)bufsize*(float)freq_config/44100.0);

	if(aui->card_controlbits&AUINFOS_CARDCNTRLBIT_DOUBLEDMA)
		bufsize*=2;                  // 2x bufsize at -ddma
	bufsize*=samplesize;          // 2x bufsize at 32-bit output (1.5x at 24)
	bufsize+=(pagesize-1);        // rounding up to pagesize
	bufsize-=(bufsize%pagesize);  //
	if(bufsize>max_bufsize)
		bufsize=max_bufsize-(max_bufsize%pagesize);
	return bufsize;
}

unsigned int MDma_init_pcmoutbuf(struct mpxplay_audioout_info_s *aui, unsigned int maxbufsize, unsigned int pagesize, unsigned long freq_config)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int dmabufsize,bit_width,tmp;
	float freq;

	asm("sub $200, %esp \n\tfsave (%esp)"); /* save/restore fpu state in case this function is called during interrupt time */

	dbgprintf("MDma_init_pcmoutbuf: maxbuf=%u freqcfg=%u freq=%u bits=%u\n", maxbufsize, freq_config, aui->freq_card, aui->bits_card );
	freq = (freq_config) ? freq_config:44100;

	switch( aui->card_wave_id ) {
	case MPXPLAY_WAVEID_PCM_FLOAT:
		bit_width = 32;
		break;
	default:
		bit_width = aui->bits_card;
		break;
	}

	dmabufsize=(unsigned int)((float)maxbufsize
							  *(float)aui->freq_card/freq);
	// *(float)bit_width/16.0);
	dmabufsize+=(pagesize-1);           // rounding up to pagesize
	dmabufsize-=(dmabufsize%pagesize);  //
	if( dmabufsize < (pagesize*2) )
		dmabufsize = (pagesize*2);
	if( dmabufsize > maxbufsize ){
		dmabufsize = maxbufsize;
		dmabufsize -= (dmabufsize%pagesize);
	}

	funcbit_smp_int32_put( aui->card_bytespersign, aui->chan_card * ((bit_width+7)/8));

	//dmabufsize-=(dmabufsize%aui->card_bytespersign);

	funcbit_smp_int32_put( aui->card_dmasize, dmabufsize);

	if(!aui->card_outbytes)
		funcbit_smp_int32_put( aui->card_outbytes, PCM_OUTSAMPLES * aui->card_bytespersign); // not exact

	tmp=(long) ( (float)aui->freq_card*(float)aui->card_bytespersign
				/((float)INT08_DIVISOR_DEFAULT*(float)INT08_CYCLES_DEFAULT/(float)INT08_DIVISOR_NEW) );

	tmp += aui->card_bytespersign-1;                              // rounding up
	tmp -= (aui->card_dmaout_under_int08%aui->card_bytespersign); // to pcm_samples
	funcbit_smp_int32_put( aui->card_dmaout_under_int08, tmp);

	funcbit_smp_int32_put( aui->card_dma_lastgoodpos, 0); // !!! the soundcard also must to do this
	tmp = aui->card_dmasize / 2;
	tmp -= aui->card_dmalastput%aui->card_bytespersign; // round down to pcm_samples
	funcbit_smp_int32_put( aui->card_dmalastput, tmp);
	funcbit_smp_int32_put( aui->card_dmafilled, aui->card_dmalastput);
	funcbit_smp_int32_put( aui->card_dmaspace, aui->card_dmasize-aui->card_dmafilled);

	freq = (aui->freq_song<22050) ? 22050.0 : (float)aui->freq_song;
	funcbit_smp_int32_put( aui->int08_decoder_cycles, (long)(freq/ (float)PCM_OUTSAMPLES) * (float)INT08_DIVISOR_NEW / (float)(INT08_CYCLES_DEFAULT * INT08_DIVISOR_DEFAULT)+1);

	asm("frstor (%esp) \n\tadd $200, %esp \n\t");

	dbgprintf("MDma_init_pcmoutbuf: done, dmabufsize=%u\n", dmabufsize );
	return dmabufsize;
}

void MDma_clearbuf(struct mpxplay_audioout_info_s *aui)
///////////////////////////////////////////////////////
{
	if(aui->card_DMABUFF && aui->card_dmasize)
		pds_memset(aui->card_DMABUFF,0,aui->card_dmasize);
}

void MDma_writedata(struct mpxplay_audioout_info_s *aui,char *src,unsigned long left)
/////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int todo;

    //dbgprintf("MDma_writedata( buffer=%X, src=%X)\n", aui->card_DMABUFF+aui->card_dmalastput, src);
	todo=aui->card_dmasize-aui->card_dmalastput;

	if(todo<=left){
		pds_memcpy(aui->card_DMABUFF+aui->card_dmalastput,src,todo);
		aui->card_dmalastput=0;
		left-=todo;
		src+=todo;
	}
	if(left){
		pds_memcpy(aui->card_DMABUFF+aui->card_dmalastput,src,left);
		aui->card_dmalastput+=left;
	}
}

// *************** called from int08 **********************************
// not used by SBEMU

// checks the DMA buffer and if it's empty, fills with zeroes
void MDma_interrupt_monitor(struct mpxplay_audioout_info_s *aui)
////////////////////////////////////////////////////////////////
{
	if(aui->card_dmafilled < (aui->card_dmaout_under_int08*2)) {
		if(!(aui->card_infobits&AUINFOS_CARDINFOBIT_DMAUNDERRUN)){
			MDma_clearbuf(aui);
			funcbit_smp_enable(aui->card_infobits,AUINFOS_CARDINFOBIT_DMAUNDERRUN);
		}
	}else{
		aui->card_dmafilled-=aui->card_dmaout_under_int08;
		funcbit_smp_disable(aui->card_infobits,AUINFOS_CARDINFOBIT_DMAUNDERRUN);
	}
}

