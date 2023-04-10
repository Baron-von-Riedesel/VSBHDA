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
// time,delay functions

#include <stdint.h>
#include <time.h>
#if defined(__GNUC__)
#include <sys/time.h>
#endif
#include <dos.h>

#include "MPXPLAY.H"

#define _disableint() asm("mov $0x900, %%ax \n\t" "int $0x31 \n\t" "mov %%ax, %0\n\t" : "=m"(oldstate) :: "eax" )
#define _restoreint() asm("mov %0, %%ax \n\t" "int $0x31 \n\t" :: "m"(oldstate) : "eax" )

unsigned long pds_gettimeh(void)
////////////////////////////////
{
	return ((unsigned long)clock()*100/CLOCKS_PER_SEC);
}

int64_t pds_gettimem(void)
//////////////////////////
{
	int64_t time_ms;
	time_ms = (int64_t)clock() * (int64_t)1000 / (int64_t)CLOCKS_PER_SEC;
	return time_ms;
}

int64_t pds_gettimeu(void)
//////////////////////////
{
	int64_t time_ms;
	time_ms = (int64_t)clock() * (int64_t)1000000 / (int64_t)CLOCKS_PER_SEC;
	return time_ms;
}

void pds_delay_10us(unsigned int ticks) //each tick is 10us
///////////////////////////////////////
{
	//unsigned int divisor=(oldint08_handler)? INT08_DIVISOR_NEW:INT08_DIVISOR_DEFAULT; // ???
	unsigned int divisor=INT08_DIVISOR_DEFAULT; // ???
	unsigned int i,oldtsc, tsctemp, tscdif;
	unsigned short oldstate;

	for( i = 0; i < ticks; i++){
		_disableint();
		outp(0x43,0x04);
		oldtsc=inp(0x40);
		oldtsc+=inp(0x40)<<8;
		_restoreint();

		do{
			_disableint();
			outp(0x43,0x04);
			tsctemp=inp(0x40);
			tsctemp+=inp(0x40)<<8;
			_restoreint();
			if(tsctemp<=oldtsc)
				tscdif=oldtsc-tsctemp; // handle overflow
			else
				tscdif=divisor+oldtsc-tsctemp;
		}while(tscdif<12); //wait for 10us  (12/(65536*18) sec)
	}
}

void pds_mdelay(unsigned long msec)
///////////////////////////////////
{
	delay(msec);
}
