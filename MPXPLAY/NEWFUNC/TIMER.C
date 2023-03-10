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
//function: timer

#include "newfunc.h"
#include <mpxplay.h>

volatile unsigned long int08counter;
volatile unsigned long mpxplay_signal_events;


//------------------------------------------------------------------------
unsigned long mpxplay_timer_secs_to_counternum(unsigned long secs)
{
 mpxp_int64_t cn;     // 1000.0ms/55.0ms = 18.181818 ticks per sec
 pds_fto64i((float)secs*(1000.0/55.0)*(float)INT08_DIVISOR_DEFAULT/(float)INT08_DIVISOR_NEW,&cn);
 return cn;
}

unsigned long mpxplay_timer_msecs_to_counternum(unsigned long msecs)
{
 mpxp_int64_t cn;     // 1000.0ms/55.0ms = 18.181818 ticks per sec
 pds_fto64i((float)(msecs + 54) / 55.0 * (float)INT08_DIVISOR_DEFAULT / (float)INT08_DIVISOR_NEW, &cn); // +54 : round up
 return cn;
}

