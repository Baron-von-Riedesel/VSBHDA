//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2012 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************

#ifndef memory_h
#define memory_h

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#define pds_memset(t,v,l)   memset(t,v,l)
#define pds_memcpy(t,s,l)   memcpy(t,s,l)
extern void *pds_calloc(unsigned int nitems,unsigned int itemsize);
extern void pds_free(void *bufptr);

#ifdef __cplusplus
}
#endif

#endif // memory_h
