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
//function: channel conversions

#include <stdint.h>

#include "MPXPLAY.H"

unsigned int cv_channels_1_to_n(PCM_CV_TYPE_S *pcm_sample, unsigned int samplenum, unsigned int newchannels, unsigned int bytespersample)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	register unsigned int i,ch,b;
	PCM_CV_TYPE_C *pcms = ((PCM_CV_TYPE_C *)pcm_sample) + (samplenum * bytespersample);
	PCM_CV_TYPE_C *pcmt = ((PCM_CV_TYPE_C *)pcm_sample) + (samplenum * bytespersample * newchannels);

	for( i = samplenum; i; i--){
		pcms -= bytespersample;
		for(ch = newchannels; ch; ch--){
			pcmt -= bytespersample;
			for(b = 0; b < bytespersample; b++)
				pcmt[b] = pcms[b];
		}
	}
	return (samplenum * newchannels);
}
