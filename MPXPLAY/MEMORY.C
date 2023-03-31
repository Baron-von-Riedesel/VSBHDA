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

#include "mpxplay.h"

#include <malloc.h>

// can be different (more safe) than the normal malloc
void *pds_malloc(unsigned int bufsize)
{
	void *bufptr;
	if(!bufsize)
		return NULL;
	bufptr=malloc(bufsize + 8);
	return bufptr;
}

void *pds_calloc(unsigned int nitems,unsigned int itemsize)
{
	void *bufptr;
	if(!nitems || !itemsize)
		return NULL;
	bufptr=calloc(nitems,(itemsize + 8));
	return bufptr;
}

void pds_free(void *bufptr)
{
	if(bufptr){
		free(bufptr);
	}
}
