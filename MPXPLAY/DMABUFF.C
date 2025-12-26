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
#include "DMABUFF.H"
#include "PHYSMEM.H"
#include "LINEAR.H"

#define AUCARDS_DMABUFSIZE_NORMAL 32768
#define AUCARDS_DMABUFSIZE_MAX    131072
#define AUCARDS_DMABUFSIZE_BLOCK  512    /* default page (block) size */

/* 1152 * 4 = 4608 = 0x1200; this is somehow related to 44100 */
//#define PCM_OUTSAMPLES 1152

/* alloc physical memory block (it's always an XMS EMB, aligned to 1kB ) */

int MDma_alloc_cardmem( struct cardmem_s *dm, unsigned int buffsize)
////////////////////////////////////////////////////////////////////
{
	dbgprintf(("MDma_alloc_cardmem(0x%X)\n", buffsize));
	/* alloc & map physical memory */
	if(!_alloc_physical_memory( dm, buffsize )) {
		return 0;
	}
	dm->pMem = NearPtr( dm->dwLinear ); /* convert linear address to near ptr */
	memset( dm->pMem, 0, buffsize );
	dbgprintf(("MDma_alloc_cardmem: %X\n", dm->pMem));
	return 1;
}

void MDma_free_cardmem(struct cardmem_s *dm)
////////////////////////////////////////////
{
	dbgprintf(("MDma_free_cardmem(%x)\n", dm));
	/* convert the near ptr back to a linear address */
	dm->dwLinear = LinearAddr( dm->pMem );
	/* unmap & free physical memory */
	_free_physical_memory(dm);
	return;
}

/* called by card-specific code during adetect(), before card is initialized.
 * max_bufsize currently is always 0, pagesize usually is "period size" (for SB Live/Audigy, it's always 4096),
 * samplesize is ignored currently; should be 2 - for SB Audigy (CA0151) it may be 4!?.
 */

unsigned int MDma_get_max_pcmoutbufsize( struct audioout_info_s *aui, unsigned int max_bufsize, unsigned int pagesize, unsigned int samplesize )
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int bufsize;
	dbgprintf(("MDma_get_max_pcmoutbufsize( max=0x%X, pgsiz=0x%X, smplsiz=%u )\n", max_bufsize, pagesize, samplesize));
	if(!max_bufsize)
		max_bufsize = AUCARDS_DMABUFSIZE_MAX; /* max is 128kB */
	if(!pagesize)
		pagesize = AUCARDS_DMABUFSIZE_BLOCK; /* =512 */
	bufsize = ( min(max_bufsize,AUCARDS_DMABUFSIZE_NORMAL) / pagesize ) * pagesize;
	dbgprintf(("MDma_get_max_pcmoutbufsize()=0x%X\n", bufsize ));
	return bufsize;
}

/* MDma_init_pcmoutbuf() is called by the card_setrate() functions;
 * these are called by AU_setrate(), which is called by main().
 * So it's NOT called during interrupt time!
 * freq_config is 0, has been removed;
 * Besides the function's arguments, used are:
 * - aui->bits_card,aui->chan_card
 * - aui->freq_card
 * out:
 * - aui->card_dmasize
 * - aui->card_bytespersign
 */

//unsigned int MDma_init_pcmoutbuf( struct audioout_info_s *aui, unsigned int maxbufsize, unsigned int pagesize, unsigned int freq_config )
unsigned int MDma_init_pcmoutbuf( struct audioout_info_s *aui, unsigned int maxbufsize, unsigned int pagesize )
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	//unsigned int dmabufsize,bit_width,tmp;
	unsigned int dmabufsize,bit_width;

	//dbgprintf(("MDma_init_pcmoutbuf(maxbufsize=0x%X pgsize=0x%X freqcfg=%u)\n", maxbufsize, pagesize, freq_config ));
	dbgprintf(("MDma_init_pcmoutbuf(maxbufsize=0x%X pgsize=0x%X)\n", maxbufsize, pagesize ));
	dbgprintf(("MDma_init_pcmoutbuf, aui fields: freq=%u/%u chan=%u/%u bits=%u/%u\n",
			aui->freq_set, aui->freq_card, aui->chan_set, aui->chan_card, aui->bits_set, aui->bits_card ));

	//if ( !freq_config ) freq_config = 44100;

	switch( aui->card_wave_id ) {
	case WAVEID_PCM_FLOAT:
		bit_width = 32;
		break;
	default:
		bit_width = aui->bits_card;
		break;
	}

	/* ensure dmabufsize is a multiple of pagesize */
#if 0
	dmabufsize = maxbufsize * aui->freq_card / freq_config;
	dmabufsize += (pagesize - 1);			// rounding up to pagesize
	dmabufsize -= (dmabufsize % pagesize);	//
	if( dmabufsize < (pagesize * 2) )
		dmabufsize = pagesize * 2;
	if( dmabufsize > maxbufsize ){
		dmabufsize = maxbufsize;
		dmabufsize -= (dmabufsize % pagesize);
	}
#else
	if( maxbufsize < (pagesize * 2) )
		dmabufsize = pagesize * 2;
	else
		dmabufsize = ( maxbufsize / pagesize ) * pagesize;
#endif

	aui->card_bytespersign = aui->chan_card * ((bit_width + 7) / 8);

	/* pagesize is a multiple of 64, so no need for next line */
	//dmabufsize -= (dmabufsize % aui->card_bytespersign);

	aui->card_dmasize = dmabufsize;

#if USELASTGOODPOS
	aui->card_dma_lastgoodpos = 0; // the soundcard is responsible for this variable
#endif
	/* v1.9: init card_dmalastput to last (two) dma buffer chunk(s) */
	//tmp = aui->card_dmasize / 2;
	//tmp -= aui->card_dmalastput % aui->card_bytespersign; // round down to pcm samples
	//aui->card_dmalastput = tmp;
	aui->card_dmalastput = dmabufsize - pagesize * ( dmabufsize >= pagesize * 4 ? 2 : 1);
	aui->card_dmaspace = aui->card_dmasize - aui->card_dmalastput;

	dbgprintf(("MDma_init_pcmoutbuf: done, card_dmasize=0x%X card_dmalastput=0x%X card_dmaspace=0x%X\n", aui->card_dmasize, aui->card_dmalastput, aui->card_dmaspace ));
	return dmabufsize;
}

void MDma_clearbuf( struct audioout_info_s *aui )
/////////////////////////////////////////////////
{
	if(aui->card_DMABUFF && aui->card_dmasize)
		memset(aui->card_DMABUFF,0,aui->card_dmasize);
	return;
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
	return;
}

