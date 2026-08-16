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

#ifndef physmem_h
#define physmem_h

#ifdef __cplusplus
extern "C" {
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
#define pds_cardmem_physicalptr(cardmem, ptr) ((cardmem).physicalptr + ((char*)(ptr) - (cardmem).pMem))

//physmem.c
extern int  _alloc_physical_memory( struct xmsmem_s *, uint32_t size);
extern void _free_physical_memory( struct xmsmem_s *);

#ifdef __cplusplus
}
#endif

#endif // physmem_h
