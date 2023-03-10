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

#include <conio.h>
#include <time.h>
#if defined(__GNUC__)
#include <sys/time.h>
#endif
#include "mpxplay.h"

#ifdef __DOS__
#include <dos.h>
#endif

unsigned long pds_gettimeh(void)
{
 return ((unsigned long)clock()*100/CLOCKS_PER_SEC);
}

mpxp_int64_t pds_gettimem(void)
{
	mpxp_int64_t time_ms;
	time_ms = (mpxp_int64_t)clock() * (mpxp_int64_t)1000 / (mpxp_int64_t)CLOCKS_PER_SEC;
	return time_ms;
}

mpxp_int64_t pds_gettimeu(void)
{
	mpxp_int64_t time_ms;
	time_ms = (mpxp_int64_t)clock() * (mpxp_int64_t)1000000 / (mpxp_int64_t)CLOCKS_PER_SEC;
	return time_ms;
}

void pds_delay_10us(unsigned int ticks) //each tick is 10us
{
#ifdef __DOS__
 //unsigned int divisor=(oldint08_handler)? INT08_DIVISOR_NEW:INT08_DIVISOR_DEFAULT; // ???
 unsigned int divisor=INT08_DIVISOR_DEFAULT; // ???
 unsigned int i,oldtsc, tsctemp, tscdif;

 for(i=0;i<ticks;i++){
  _disable();
  outp(0x43,0x04);
  oldtsc=inp(0x40);
  oldtsc+=inp(0x40)<<8;
  _enable();

  do{
   _disable();
   outp(0x43,0x04);
   tsctemp=inp(0x40);
   tsctemp+=inp(0x40)<<8;
   _enable();
   if(tsctemp<=oldtsc)
    tscdif=oldtsc-tsctemp; // handle overflow
   else
    tscdif=divisor+oldtsc-tsctemp;
  }while(tscdif<12); //wait for 10us  (12/(65536*18) sec)
 }
#else
 pds_mdelay((ticks+99)/100);
 //unsigned int oldclock=clock();
 //while(oldclock==clock()){} // 1ms not 0.01ms (10us)
#endif
}

void pds_mdelay(unsigned long msec)
{
#ifdef __DOS__
 delay(msec);
#else
 pds_threads_sleep(msec);
#endif
}
