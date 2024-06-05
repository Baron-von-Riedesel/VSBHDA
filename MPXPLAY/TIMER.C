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
// delay function

#include <stdint.h>
#include <dos.h>

#include "MPXPLAY.H"

#ifdef DJGPP

#define _disableint() asm("mov $0x900, %%ax \n\t" "int $0x31 \n\t" "mov %%ax, %0\n\t" : "=m"(oldstate) :: "eax" )
#define _restoreint() asm("mov %0, %%ax \n\t" "int $0x31 \n\t" :: "m"(oldstate) : "eax" )

static inline uint16_t gettimercnt(void)
{
	unsigned short oldstate;
	uint16_t tsc;
	_disableint();
	outp( 0x43, 0x04 );
	tsc = inp(0x40);
	tsc += inp(0x40) << 8;
	_restoreint();
    return tsc;
}

#else

static  uint16_t gettimercnt(void);
#pragma aux gettimercnt = \
    "mov ax, 0900h" \
    "int 31h"      \
    "push eax"     \
    "mov al, 4"    \
    "out 43h, al"  \
    "in al, 40h"   \
    "mov ah, al"   \
    "in al, 40h"   \
    "xchg al,ah"   \
    "mov dx, ax"   \
    "pop eax"      \
    "int 31h"      \
    "mov ax, dx"   \
    parm[]         \
    modify exact[ax dx]
#endif

void pds_delay_10us(unsigned int ticks) //each tick is 10us
///////////////////////////////////////
{
	unsigned int divisor = PIT_DIVISOR_DEFAULT; // is 65536
	unsigned int i;
	unsigned short oldtsc, tsctemp, tscdif;

	oldtsc = gettimercnt();
	for( i = 0; i < ticks; i++ ){
		do{
			tsctemp = gettimercnt();
			if(tsctemp <= oldtsc)
				tscdif = oldtsc - tsctemp; // handle overflow
			else
				tscdif = divisor + oldtsc - tsctemp;
		} while ( tscdif < 12); //wait for 10us  (12/(65536*18) sec)
		oldtsc = tsctemp;
	}
}
