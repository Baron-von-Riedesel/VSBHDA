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
//function: DMA handling
//based on the MPG123 (DOS)

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "CONFIG.H"
#include "MPXPLAY.H"
#include "DMAIRQ.H"
#include "PHYSMEM.H"
#include "LINEAR.H"

/* alloc physical memory block (it's always an XMS EMB, aligned to 1kB ) */

struct cardmem_s *MDma_alloc_cardmem(unsigned int buffsize)
///////////////////////////////////////////////////////////
{
	struct cardmem_s *dm;
	dbgprintf(("MDma_alloc_cardmem(0x%X)\n", buffsize));
	dm = calloc( 1, sizeof(struct cardmem_s) );
	if(!dm)
		return NULL;
	/* alloc & map physical memory */
	if(!_alloc_physical_memory( dm, buffsize )) {
		free(dm);
		return NULL;
	}
	dm->pMem = NearPtr( dm->dwLinear ); /* convert linear address to near ptr */
	memset( dm->pMem, 0, buffsize );
	dbgprintf(("MDma_alloc_cardmem: %X\n", dm->pMem));
	return dm;
}

void MDma_free_cardmem(struct cardmem_s *dm)
////////////////////////////////////////////
{
	dbgprintf(("MDma_free_cardmem(%x)\n", dm));
	if( dm ){
		/* convert the near ptr back to a linear address */
		dm->dwLinear = LinearAddr( dm->pMem );
		/* unmap & free physical memory */
		_free_physical_memory(dm);
		free(dm);
	}
}

unsigned int MDma_get_max_pcmoutbufsize( struct audioout_info_s *aui, unsigned int max_bufsize, unsigned int pagesize, unsigned int samplesize, unsigned long freq_config)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int bufsize;
	dbgprintf(("MDma_get_max_pcmoutbufsize( max=0x%X, pgsiz=0x%X, smplsiz=%u, freq=%u )\n", max_bufsize, pagesize, samplesize, freq_config));
	if(!max_bufsize)
		max_bufsize = AUCARDS_DMABUFSIZE_MAX; /* max is 128kB */
	if(!pagesize)
		pagesize = AUCARDS_DMABUFSIZE_BLOCK; /* =4096 */
	if(samplesize < 2)
		samplesize = 2;
	bufsize = AUCARDS_DMABUFSIZE_NORMAL / 2; /* =32kB/2 */

	if( freq_config )
		bufsize = bufsize * freq_config / 44100;

	bufsize *= samplesize;        // 2x bufsize at 32-bit output (1.5x at 24)
	bufsize += (pagesize - 1);    // rounding up to pagesize
	bufsize -= (bufsize % pagesize);  //
	if(bufsize > max_bufsize)
		bufsize = max_bufsize - (max_bufsize % pagesize);
	dbgprintf(("MDma_get_max_pcmoutbufsize()=0x%X\n", bufsize ));
	return bufsize;
}

/* MDma_init_pcmoutbuf() is called by the card_setrate() functions;
 * these are called by AU_setrate(), which is called by main().
 * So it's NOT called during interrupt time!
 * freq_config is usually 0, except for HDA.
 */

unsigned int MDma_init_pcmoutbuf( struct audioout_info_s *aui, unsigned int maxbufsize, unsigned int pagesize, unsigned long freq_config )
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int dmabufsize,bit_width,tmp;
	uint32_t freq;

	dbgprintf(("MDma_init_pcmoutbuf(maxbufsize=0x%X pgsize=0x%X freqcfg=%u)\n", maxbufsize, pagesize, freq_config ));
	dbgprintf(("MDma_init_pcmoutbuf, aui fields: freq=%u/%u chan=%u/%u bits=%u/%u\n",
			aui->freq_set, aui->freq_card, aui->chan_set, aui->chan_card, aui->bits_set, aui->bits_card ));
	freq = (freq_config) ? freq_config : 44100;

	switch( aui->card_wave_id ) {
	case WAVEID_PCM_FLOAT:
		bit_width = 32;
		break;
	default:
		bit_width = aui->bits_card;
		break;
	}

	dmabufsize = maxbufsize * aui->freq_card / freq;
	dmabufsize += (pagesize - 1);           // rounding up to pagesize
	dmabufsize -= (dmabufsize % pagesize);  //
	if( dmabufsize < (pagesize * 2) )
		dmabufsize = pagesize * 2;
	if( dmabufsize > maxbufsize ){
		dmabufsize = maxbufsize;
		dmabufsize -= (dmabufsize % pagesize);
	}

	aui->card_bytespersign = aui->chan_card * ((bit_width + 7) / 8);

	//dmabufsize -= (dmabufsize % aui->card_bytespersign);

	aui->card_dmasize = dmabufsize;

	/* PCM_OUTSAMPLES is 1152 ( 1152*4=4608=0x1200 ) somehow related to 44.100 */
	if(!aui->card_outbytes)
		aui->card_outbytes = PCM_OUTSAMPLES * aui->card_bytespersign; // not exact

	aui->card_dma_lastgoodpos = 0; // !!! the soundcard also must to do this
	tmp = aui->card_dmasize / 2;
	tmp -= aui->card_dmalastput % aui->card_bytespersign; // round down to pcm_samples
	aui->card_dmalastput = tmp;
	aui->card_dmafilled = aui->card_dmalastput;
	aui->card_dmaspace = aui->card_dmasize - aui->card_dmafilled;

	dbgprintf(("MDma_init_pcmoutbuf: done, card_dmasize=0x%X, card_outbytes=%u\n", aui->card_dmasize, aui->card_outbytes ));
	return dmabufsize;
}

void MDma_clearbuf( struct audioout_info_s *aui )
/////////////////////////////////////////////////
{
	if(aui->card_DMABUFF && aui->card_dmasize)
		memset(aui->card_DMABUFF,0,aui->card_dmasize);
}

void MDma_writedata( struct audioout_info_s *aui, char *src, unsigned long left )
/////////////////////////////////////////////////////////////////////////////////
{
	unsigned int todo;

	//dbgprintf(("MDma_writedata( buffer=%X, src=%X)\n", aui->card_DMABUFF+aui->card_dmalastput, src));
	todo = aui->card_dmasize - aui->card_dmalastput;

	if(todo <= left){
		memcpy(aui->card_DMABUFF + aui->card_dmalastput,src,todo);
		aui->card_dmalastput = 0;
		left -= todo;
		src += todo;
	}
	if(left){
		memcpy(aui->card_DMABUFF + aui->card_dmalastput,src,left);
		aui->card_dmalastput += left;
	}
}

#if 0
// *************** called from int08 **********************************
// checks the DMA buffer and if it's empty, fills with zeroes

void MDma_interrupt_monitor( struct audioout_info_s *aui )
//////////////////////////////////////////////////////////
{
	if(aui->card_dmafilled < (aui->card_dmaout_under_int08 * 2)) {
		if(!(aui->card_infobits & AUINFOS_CARDINFOBIT_DMAUNDERRUN)){
			MDma_clearbuf(aui);
			aui->card_infobits |= AUINFOS_CARDINFOBIT_DMAUNDERRUN;
		}
	}else{
		aui->card_dmafilled -= aui->card_dmaout_under_int08;
		aui->card_infobits &= ~AUINFOS_CARDINFOBIT_DMAUNDERRUN;
	}
}
#endif

