//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2013 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************

#ifndef mpxplay_h
#define mpxplay_h

#ifdef __cplusplus
extern "C" {
#endif

//#define WAVEID_UNKNOWN   0x0000
#define WAVEID_PCM_SLE   0x0001 // signed little endian
#define WAVEID_PCM_FLOAT 0x0003 // 32/64-bit float le

//------------------------------------------------------------------------

#ifndef max
#define max(a,b) (((a)>(b))? (a):(b))
#endif

#ifndef min
#define min(a,b) (((a)<(b))? (a):(b))
#endif

#if defined(DJGPP)
#include <pc.h>
#define inpd inportl
#define outpd outportl
#endif

#include <string.h>
#include "PHYSMEM.H"
#include "TIMER.H"
#include "AU_CARDS.H"

#ifdef __cplusplus
}
#endif

#endif // mpxplay_h
