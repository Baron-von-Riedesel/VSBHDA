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

#include <fcntl.h>

#ifndef MPXPLAY
#define MPXPLAY 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

// out pcm defs
#define PCM_OUTSAMPLES    1152     // at 44100Hz
#define PCM_MIN_CHANNELS     1
#define PCM_MAX_CHANNELS     2     // au_mixer output (au_card input) limit
#define PCM_CHANNELS_DEFAULT 2
#define PCM_CHANNELS_CFG ((aui->chan_card)? aui->chan_card:((aui->chan_set)? aui->chan_set:PCM_CHANNELS_DEFAULT))
#define PCM_MIN_BITS      1
#define PCM_MAX_BITS      32
#define PCM_MIN_FREQ      512
#define PCM_MAX_FREQ      192000   // program can play higher freq too
#define PCM_MAX_SAMPLES   (((PCM_OUTSAMPLES*PCM_MAX_FREQ)+22050)/44100*PCM_CHANNELS_CFG) // only the pcm buffer is limited (in one frame)
#define PCM_MAX_BYTES     (PCM_MAX_SAMPLES*(PCM_MAX_BITS/8))  // in one frame
#define PCM_BUFFER_SIZE   (2*PCM_MAX_BYTES) // *2 : speed control expansion

// outmodes
#define OUTMODE_TYPE_NONE            0  // ie:-is,-iw
#define OUTMODE_TYPE_TEST      (1 << 0) // testmode (-t) (null output without startup, to test the speed of decoding)
#define OUTMODE_TYPE_AUDIO     (1 << 1) // audio mode
#define OUTMODE_TYPE_FILE      (1 << 2) // write output into file (-o)
#define OUTMODE_TYPE_NULL      (1 << 3) // null output with startup
#define OUTMODE_TYPE_MASK      (OUTMODE_TYPE_TEST|OUTMODE_TYPE_AUDIO|OUTMODE_TYPE_FILE|OUTMODE_TYPE_NULL)
#define OUTMODE_CONTROL_FILE_BITSTREAMOUT (1 <<  4) // -obs
#define OUTMODE_CONTROL_FILE_FLOATOUT     (1 <<  5) // -obf
#define OUTMODE_CONTROL_FILE_TAGLFN       (1 <<  6) // -oni
#define OUTMODE_CONTROL_SNDCARD_DDMA      (1 <<  8) // -ddma
//#define OUTMODE_CONTROL_FILE_MASK (OUTMODE_CONTROL_FILE_BITSTREAMOUT|OUTMODE_CONTROL_FILE_FLOATOUT|OUTMODE_CONTROL_FILE_TAGLFN)

//playcontrol flags (status and control)
#define PLAYC_STARTNEXT       32 // start playing at next song
#define PLAYC_RUNNING         64 // playing is running

//intsoundconfig and intsoundcontrol function bits
//#define INTSOUND_NONE      0  // no interrupt functions
#define INTSOUND_DECODER   1  // interrupt decoder
//#define INTSOUND_TSR       2  // tsr mode
//#define INTSOUND_FUNCTIONS (INTSOUND_DECODER|INTSOUND_TSR)
//#define INTSOUND_NOINTDEC  4  // disable interrupt decoder (intsoundconfig)
//#define INTSOUND_DOSSHELL  16 // intsoundconfig:-xs intsoundcontrol:ctrl-d
#ifdef SBEMU
#define INTSOUND_NOBUSYWAIT 32
#endif
//#define INTSOUND_INT08RUN  1024 // int08 process is running (has not finished)

//timer settings
#define INT08_DIVISOR_DEFAULT  65536
#define INT08_CYCLES_DEFAULT   (1000.0/55.0)  // 18.181818
#ifdef __DOS__
 #define INT08_DIVISOR_NEW     10375  // = 18.181818*65536 / (3 * 44100/1152)
 //#define INT08_DIVISOR_NEW      1194  // for 1ms (1/1000 sec) refresh (not recommended)
#else
 #define INT08_DIVISOR_NEW      8400  // for 7ms refresh (~130fps) (9600-> 8ms doesn't work at me)
 //#define INT08_DIVISOR_NEW      20750  // 60fps (for default Win program refresh)
#endif
#define INT08_CYCLES_NEW  (INT08_CYCLES_DEFAULT*INT08_DIVISOR_DEFAULT/INT08_DIVISOR_NEW) // 114.8 or 130

#define MPXPLAY_INTSOUNDDECODER_DISALLOW intsoundcntrl_save=intsoundcontrol;funcbit_disable(intsoundcontrol,INTSOUND_DECODER);
#define MPXPLAY_INTSOUNDDECODER_ALLOW    if(intsoundconfig&INTSOUND_DECODER) funcbit_copy(intsoundcontrol,intsoundcntrl_save,INTSOUND_DECODER);

// Mpxplay exit error codes
#define MPXERROR_OK                0
#define MPXERROR_SNDCARD           8
#define MPXERROR_UNDEFINED        15 // sometimes not error, just don't save playlists and mpxplay.ini

#define MPXPLAY_WAVEID_UNKNOWN   0x0000
#define MPXPLAY_WAVEID_PCM_SLE   0x0001 // signed little endian
#define MPXPLAY_WAVEID_PCM_FLOAT 0x0003 // 32/64-bit float le

//------------------------------------------------------------------------

#include "in_file.h"
#include "au_cards.h"
#include "au_mixer.h"

#ifdef __cplusplus
}
#endif

#endif // mpxplay_h
