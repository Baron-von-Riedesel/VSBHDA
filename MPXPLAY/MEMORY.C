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
//function: memory handling

#include <stdint.h>
#include <malloc.h>
#include <dpmi.h>
#include <sys/segments.h>  /* for _my_ds() */

void *pds_calloc( unsigned int nitems, unsigned int itemsize)
/////////////////////////////////////////////////////////////
{
	void *p = calloc( nitems, itemsize + 8 );
	/* ensure that DS limit remains 4G */
	__dpmi_set_segment_limit(_my_ds(), 0xFFFFFFFF);
	return p;
}

void pds_free( void *bufptr )
/////////////////////////////
{
	free( bufptr );
}
