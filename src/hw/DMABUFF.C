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
//function: DMA buffer handling

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "CONFIG.H"
#include "AU_CARDS.H"
#include "DMABUFF.H"
#include "LINEAR.H"

//#define AUCARDS_DMABUFSIZE_NORMAL 32768
#define AUCARDS_DMABUFSIZE_MAX    131072
#define AUCARDS_DMABUFSIZE_BLOCK  512    /* default page (block) size */

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
	if ( dm->handle ) {
		dbgprintf(("MDma_free_cardmem(%x) pMem=%X handle=%X\n", dm, dm->pMem, dm->handle));
		/* convert the near ptr back to a linear address */
		dm->dwLinear = LinearAddr( dm->pMem );
		/* unmap & free physical memory */
		_free_physical_memory(dm);
	}
	return;
}

/* called by card-specific code during adetect(), before card is initialized.
 * max_bufsize currently is always 0, pagesize usually is "period size" (for SB Live/Audigy, it's always 4096),
 * the value returned is used by the card driver code to allocate the hardware buffers.
 * v2.0: AUCARDS_DMABUFSIZE_NORMAL obsolete, replaced by /B argument.
 */

unsigned int MDma_get_max_pcmoutbufsize( struct audioout_info_s *aui, unsigned int max_bufsize, unsigned int pagesize )
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int bufsize;
	dbgprintf(("MDma_get_max_pcmoutbufsize( max=0x%X, pgsiz=0x%X )\n", max_bufsize, pagesize));
	if (!max_bufsize)
		max_bufsize = AUCARDS_DMABUFSIZE_MAX; /* max is 128kB */
	if (!pagesize)
		pagesize = AUCARDS_DMABUFSIZE_BLOCK; /* =512 */
	max_bufsize -= max_bufsize % pagesize;
	/* v2.0: max buffer size now limited - using the new /B option */
	//bufsize = ( min(max_bufsize,AUCARDS_DMABUFSIZE_NORMAL) / pagesize ) * pagesize;
	bufsize = min( max_bufsize, ((aui->gvars->buffers > 1 ) ? aui->gvars->buffers : HW_BUFFERS_DEFAULT ) * pagesize );
	dbgprintf(("MDma_get_max_pcmoutbufsize()=0x%X\n", bufsize ));
	return bufsize;
}

/* MDma_init_pcmoutbuf() is called by the card_setrate() functions;
 * however, since the rate has no impact anymore on the DMA buffer size,
 * this could be changed now.
 * maxbufsize usually is the value returned by Mdma_get_max_pcmoutbufsize();
 */
unsigned int MDma_init_pcmoutbuf( struct audioout_info_s *aui, unsigned int maxbufsize, unsigned int pagesize )
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	/* card_dmasize must be a multiple of pagesize */
	aui->card_dmasize = (maxbufsize < (pagesize * 2)) ? pagesize * 2 : (maxbufsize - maxbufsize % pagesize);
	dbgprintf(("MDma_init_pcmoutbuf(maxbufsize=0x%X pgsize=0x%X): card_dmasize=0x%X\n", maxbufsize, pagesize, aui->card_dmasize ));
	return aui->card_dmasize;
}

void MDma_clearbuf( struct audioout_info_s *aui )
/////////////////////////////////////////////////
{
	if(aui->card_pDmaBuffer && aui->card_dmasize)
		memset(aui->card_pDmaBuffer,0,aui->card_dmasize);
	return;
}

void MDma_writedata( struct audioout_info_s *aui, char *src, unsigned int left )
////////////////////////////////////////////////////////////////////////////////
{
	unsigned int todo;

	//dbgprintf(("MDma_writedata( buffer=%X, src=%X)\n", aui->card_pDmaBuffer + aui->card_dmalastput, src));
	todo = min( left, aui->card_dmasize - aui->card_dmalastput );

	memcpy(aui->card_pDmaBuffer + aui->card_dmalastput,src,todo);
	if ( left > todo )
		memcpy(aui->card_pDmaBuffer,src + todo,left - todo);
	return;
}

