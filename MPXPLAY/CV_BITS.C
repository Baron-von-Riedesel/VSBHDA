//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2010 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: bit conversions

#include <stdint.h>

#include "CONFIG.H"
#include "MPXPLAY.H"

//#define USE_ASM_CV_BITS 1

static void cv_8bits_unsigned_to_signed( PCM_CV_TYPE_UC *pcm, unsigned int samplenum)
/////////////////////////////////////////////////////////////////////////////////////
{
	dbgprintf("cv_8bits_unsigned_to_signed( pcm=%X, smpls=%u\n", pcm, samplenum );
    for ( ; samplenum; *pcm ^= 0x80, pcm++, samplenum-- );
}

static void cv_8bits_signed_to_unsigned( PCM_CV_TYPE_UC *pcm, unsigned int samplenum)
/////////////////////////////////////////////////////////////////////////////////////
{
	dbgprintf("cv_8bits_signed_to_unsigned( pcm=%X, smpls=%u\n", pcm, samplenum );
    for ( ; samplenum; *pcm ^= 0x80, pcm++, samplenum-- );
}

/* compress 32->24, 32->16, 32->8, 24->16, 24->8, 16->8 */

static void cv_bits_down(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int instep,unsigned int outstep)
////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	PCM_CV_TYPE_C *inptr = (PCM_CV_TYPE_C *)pcm;
	PCM_CV_TYPE_C *outptr = (PCM_CV_TYPE_C *)pcm;
	unsigned int skip = instep-outstep;

	do{
		unsigned int oi = outstep;
		inptr += skip;
		do{
			*outptr = *inptr;
			outptr++;inptr++;
		}while(--oi);
	}while(--samplenum);
}

/* expand 8->16, 8->24, 8->32, 16->24, 16->32, 24->32 */

static void cv_bits_up( PCM_CV_TYPE_S *pcm, unsigned int samplenum, unsigned int instep, unsigned int outstep)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	PCM_CV_TYPE_C *inptr = (PCM_CV_TYPE_C *)pcm;
	PCM_CV_TYPE_C *outptr = (PCM_CV_TYPE_C *)pcm;

	dbgprintf("cv_bits_up( pcm=%X, smpls=%u\n", pcm, samplenum );

	inptr += samplenum * instep;
	outptr += samplenum * outstep;

	for ( ; samplenum; samplenum-- ) {
		unsigned int ii = instep;
		unsigned int oi = outstep;
		do{    //copy upper bits (bytes) to the right/correct place
			inptr--;
			outptr--;
			*outptr = *inptr;
			oi--;
		}while(--ii);
		do{    //fill lower bits (bytes) with zeroes
			outptr--;
			*outptr = 0;
		}while(--oi);
	}
}

/*
 * convert 8 -> 16 or 16 -> 8
 * it's generally assumed that 8 bit is unsigned, 16-bit is signed
 */

void cv_bits_n_to_m( PCM_CV_TYPE_S *pcm, unsigned int samplenum, unsigned int in_bytespersample, unsigned int out_bytespersample)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	dbgprintf("cv_bits_n_to_m( pcm=%X, smpls=%u, in=%u, out=%u\n", pcm, samplenum, in_bytespersample, out_bytespersample );
	if( out_bytespersample > in_bytespersample ){
		if( in_bytespersample == 1 )
			cv_8bits_unsigned_to_signed( (PCM_CV_TYPE_UC *)pcm, samplenum );
		cv_bits_up( pcm, samplenum, in_bytespersample, out_bytespersample );
	} else {
		if( out_bytespersample < in_bytespersample ) {
			cv_bits_down( pcm, samplenum, in_bytespersample, out_bytespersample );
			if( out_bytespersample == 1 )
				cv_8bits_signed_to_unsigned( (PCM_CV_TYPE_UC *)pcm, samplenum );
		}
	}
}
