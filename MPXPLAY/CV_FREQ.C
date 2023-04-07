//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2014 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: speed/freq control

#include <math.h>
#include <stdlib.h>
#include <assert.h>

#include "SBEMUCFG.H"
#include "MPXPLAY.H"

#define MALLOCSTATIC 1

unsigned int mixer_speed_lq( PCM_CV_TYPE_S *pcmsrc, unsigned int samplenum, unsigned int channels, unsigned int samplerate, unsigned int newrate)
{
	const unsigned int instep=((samplerate/newrate)<<12) | (((4096*(samplerate%newrate)+newrate-1)/newrate)&0xFFF);
	const unsigned int inend=(samplenum/channels) << 12;
	PCM_CV_TYPE_S *pcmdst;
	unsigned long ipi;
	unsigned int inpos = 0;//(samplerate<newrate) ? instep/2 : 0;
#if MALLOCSTATIC
	static int maxsample = 0;
	static PCM_CV_TYPE_S* buff = NULL;
#endif
	if(!samplenum)
		return 0;
	//assert(((samplenum/channels)&0xFFF00000) == 0); //too many samples, need other approches.

	/* changed: the source is now copied to the buffer and then the conversion is done into pcmsrc */
	//unsigned int buffcount = max(((unsigned long long)max( samplenum, 512) * newrate + samplerate - 1)/samplerate,
	//							 max(samplenum,512))*2+32;
	//PCM_CV_TYPE_S* buff = (PCM_CV_TYPE_S*)malloc(buffcount*sizeof(PCM_CV_TYPE_S));
#if MALLOCSTATIC
	if ( samplenum > maxsample ) {
		if ( buff )
			free( buff );
		buff = (PCM_CV_TYPE_S*)malloc(samplenum * sizeof(PCM_CV_TYPE_S));
		maxsample = samplenum;
	}
#else
	PCM_CV_TYPE_S* buff = (PCM_CV_TYPE_S*)malloc(samplenum * sizeof(PCM_CV_TYPE_S));
#endif
	memcpy( buff, pcmsrc, samplenum * sizeof(PCM_CV_TYPE_S) );

	dbgprintf("mixer_speed_lq: srcrate=%u dstrate=%u chn=%u smpl=%u step=%x end=%x\n", samplerate, newrate, channels, samplenum, instep, inend );

	pcmdst = pcmsrc;
	int total = samplenum/channels;

	do{
		int m1,m2;
		unsigned int ipi,ch;
		PCM_CV_TYPE_S *intmp1,*intmp2;
		ipi = inpos >> 12;
		m2 = inpos & 0xFFF;
		m1 = 4096 - m2;
		ch = channels;
		ipi *= ch;
		intmp1 = buff + ipi;
		intmp2 = ipi < total - ch ? intmp1 + ch : intmp1;
		do{
			*pcmdst++= ((*intmp1++) * m1 + (*intmp2++) * m2) / 4096;// >> 12; //don't use shift, signed right shift impl defined, maybe logical shift
		}while (--ch);
		inpos += instep;
	}while( inpos < inend );

	//dbgprintf("mixer_speed_lq: sample cnt=%d, buffcnt=%d\n", pcmdst - buff, buffcount);
	//assert(pcmdst - buff <= buffcount);
	//memcpy( pcmsrc, buff, (pcmdst - buff) * sizeof(PCM_CV_TYPE_S));
#if !MALLOCSTATIC
	free(buff);
#endif
	return pcmdst - pcmsrc;
}
