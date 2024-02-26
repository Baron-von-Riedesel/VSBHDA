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
//function: routines in newfunc.lib

#ifndef newfunc_h
#define newfunc_h

#include <stddef.h> // for offsetof

#ifdef __cplusplus
extern "C" {
#endif

#ifndef max
#define max(a,b) (((a)>(b))? (a):(b))
#endif

#ifndef min
#define min(a,b) (((a)<(b))? (a):(b))
#endif

#if defined(SBEMU) && defined(DJGPP)
#include <pc.h>
#define inpd inportl
#define outpd outportl
#endif

struct xmsmem_s{
	union {
		uint32_t dwLinear;  /* the linear address is usually converted to a near ptr! */
		char *pMem;
	};
	uint32_t physicalptr;
	unsigned short handle;
};

#define cardmem_s xmsmem_s

#define pds_cardmem_physicalptr(cardmem, ptr) ((cardmem)->physicalptr + ((char*)(ptr) - (cardmem)->pMem))

//physmem.c
extern int  _alloc_physical_memory( struct xmsmem_s *, uint32_t size);
extern void _free_physical_memory( struct xmsmem_s *);

//memory.c
#include <string.h>
#define pds_memset(t,v,l)   memset(t,v,l)
#define pds_memcpy(t,s,l)   memcpy(t,s,l)
extern void *pds_calloc(unsigned int nitems,unsigned int itemsize);
extern void pds_free(void *bufptr);

//time.c
extern unsigned long pds_gettimeh(void); // clock time in hsec
extern int64_t pds_gettimem(void);  // clock time in msec
extern int64_t pds_gettimeu(void);  // clock time in usec
extern void pds_delay_10us(unsigned int ticks);
extern void pds_mdelay(unsigned long msec);

#ifdef __cplusplus
}
#endif

#endif // newfunc_h
