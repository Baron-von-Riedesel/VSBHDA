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
//function: definitions

#ifndef mpxplay_h
#define mpxplay_h

#ifdef __cplusplus
extern "C" {
#endif

// out pcm defs
#define PCM_OUTSAMPLES    1152     // at 44100Hz ( see PIT_DIVISOR_NEW )
//#define PCM_MIN_CHANNELS     1
//#define PCM_MAX_CHANNELS     2     // au_mixer output (au_card input) limit
#define PCM_CHANNELS_DEFAULT 2
//#define PCM_CHANNELS_CFG ((aui->chan_card) ? aui->chan_card : ((aui->chan_set) ? aui->chan_set: PCM_CHANNELS_DEFAULT))
//#define PCM_MIN_BITS      1
//#define PCM_MAX_BITS      32
//#define PCM_MIN_FREQ      512
//#define PCM_MAX_FREQ      192000   // program can play higher freq too
//#define PCM_MAX_SAMPLES   (((PCM_OUTSAMPLES * PCM_MAX_FREQ)+22050)/44100 * PCM_CHANNELS_CFG) // only the pcm buffer is limited (in one frame)
//#define PCM_MAX_BYTES     (PCM_MAX_SAMPLES * (PCM_MAX_BITS/8))  // in one frame
//#define PCM_BUFFER_SIZE   (2 * PCM_MAX_BYTES) // *2 : speed control expansion

//intsoundconfig function bits
#define INTSOUND_NOBUSYWAIT 32

//timer settings
#define PIT_DIVISOR_DEFAULT  65536
//#define PIT_CYCLES_DEFAULT   (1000.0/55.0)  // 18.181818

#define WAVEID_UNKNOWN   0x0000
#define WAVEID_PCM_SLE   0x0001 // signed little endian
#define WAVEID_PCM_FLOAT 0x0003 // 32/64-bit float le

//------------------------------------------------------------------------

#include "NEWFUNC.H"
#include "AU_CARDS.H"

#ifdef __cplusplus
}
#endif

#endif // mpxplay_h
