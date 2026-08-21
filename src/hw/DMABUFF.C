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
#define AUCARDS_DMABUFSIZE_BLOCK  512    /* default page (=period) size */

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
 * maxbufsize currently is always 0, pagesize usually is "period size";
 * the value returned is used by the card driver code to allocate the hardware buffers.
 * v2.0: AUCARDS_DMABUFSIZE_NORMAL obsolete, replaced by /B argument.
 */

unsigned int MDma_get_bufsize( struct audioout_info_s *aui, unsigned int maxbufsize, unsigned int pagesize )
////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int bufsize = (maxbufsize ? maxbufsize : AUCARDS_DMABUFSIZE_MAX);
	if (!pagesize)
		pagesize = ( aui->gvars->period_size ? aui->gvars->period_size : AUCARDS_DMABUFSIZE_BLOCK );
	bufsize -= bufsize % pagesize; /* align */
	/* v2.0: max buffer size now limited - using the new /B option */
	//bufsize = ( min(maxbufsize,AUCARDS_DMABUFSIZE_NORMAL) / pagesize ) * pagesize;
	bufsize = min( bufsize, ((aui->gvars->buffers > 1 ) ? aui->gvars->buffers : HW_BUFFERS_DEFAULT ) * pagesize );
	dbgprintf(("MDma_get_bufsize(max=0x%X,pgsiz=0x%X)=0x%X\n", maxbufsize, pagesize, bufsize));
	return bufsize;
}

/* MDma_initbuf() is called by the card_setrate() functions;
 * bufsize usually is the value returned by Mdma_get_bufsize();
 * v1.9: since the rate has no impact anymore on the DMA buffer size,
 *       this function has become somewhat obsolete.
 * v2.0: all checks removed, it's assumed that the card driver uses the value
 *       returned by MDma_get_bufsize().
 */
unsigned int MDma_initbuf( struct audioout_info_s *aui, unsigned int bufsize )
//////////////////////////////////////////////////////////////////////////////
{
	aui->card_dmasize = bufsize;
	dbgprintf(("MDma_initbuf(bufsize=0x%X)\n", bufsize ));
	return bufsize;
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

