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

#define MPXPLAY_LINK_FULL     0

// out pcm defs
#define PCM_OUTSAMPLES    1152     // at 44100Hz
#define PCM_MIN_CHANNELS     1
#ifdef MPXPLAY_LINK_FULL
#define PCM_MAX_CHANNELS     8     // au_mixer output (au_card input) limit
#else
#define PCM_MAX_CHANNELS     2     // au_mixer output (au_card input) limit
#endif
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
#define INTSOUND_NONE      0  // no interrupt functions
#define INTSOUND_DECODER   1  // interrupt decoder
#define INTSOUND_TSR       2  // tsr mode
#define INTSOUND_FUNCTIONS (INTSOUND_DECODER|INTSOUND_TSR)
#define INTSOUND_NOINTDEC  4  // disable interrupt decoder (intsoundconfig)
#define INTSOUND_NOINT08   8  // disable int08 (intsoundconfig)
#define INTSOUND_DOSSHELL  16 // intsoundconfig:-xs intsoundcontrol:ctrl-d
#ifdef SBEMU
#define INTSOUND_NOBUSYWAIT 32
#endif
#define INTSOUND_INT08RUN  1024 // int08 process is running (has not finished)

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
#define MPXERROR_XMS_MEM           9
#define MPXERROR_CONVENTIONAL_MEM 10
#define MPXERROR_NOFILE           11
#define MPXERROR_CANTWRITEFILE    12
#define MPXERROR_DIV0             13
#define MPXERROR_EXCEPTION        14
#define MPXERROR_UNDEFINED        15 // sometimes not error, just don't save playlists and mpxplay.ini

// Mpxplay error range begins
#define MPXPLAY_ERROR_RANGE_INFILE          0
#define MPXPLAY_ERROR_RANGE_MPXINBUF     -100
#define MPXPLAY_ERROR_RANGE_FILEHAND     -200
#define MPXPLAY_ERROR_RANGE_CFGFUNC      -300
#define MPXPLAY_ERROR_RANGE_DISKDRIV     -400
#define MPXPLAY_ERROR_RANGE_GENERAL      -800

#define MPXPLAY_ERROR_OK                   0
#define MPXPLAY_ERROR_MPXINBUF_OK          0 // no error
#define MPXPLAY_ERROR_MPXINBUF_READ_EOF -100 // can't read, because EOF
#define MPXPLAY_ERROR_MPXINBUF_READ_BUF -101 // can't read, because no data left in buffer
#define MPXPLAY_ERROR_MPXINBUF_READ_LOW -102 // can't read from file-device (but not eof yet)
#define MPXPLAY_ERROR_MPXINBUF_SEEK_EOF -110 // invalid newfilepos (out of filesize)
#define MPXPLAY_ERROR_MPXINBUF_SEEK_BUF -111 // newfilepos is out of buffer
#define MPXPLAY_ERROR_MPXINBUF_SEEK_LOW -112 // can't seek file-device
#define MPXPLAY_ERROR_MPXINBUF_ARGUMENT -120 // argument error

#define MPXPLAY_ERROR_CFGFUNC_INVALIDDATA     -300
#define MPXPLAY_ERROR_CFGFUNC_UNSUPPFUNC      -301
#define MPXPLAY_ERROR_CFGFUNC_ARGUMENTMISSING -302

#define MPXPLAY_ERROR_ARGS              -800

//dosshell types
#define DSH_NO        0
#define DSH_CHILD     1
#define DSH_NEWSHELL  2
#define DSH_START     3

//other
//#define IRQ_STACK_SIZE 16384   // size of irq (errorhand and soundcard) stacks

//wave (codec) IDs at input/output
#define MPXPLAY_WAVEID_UNKNOWN   0x0000
#define MPXPLAY_WAVEID_PCM_SLE   0x0001 // signed little endian
#define MPXPLAY_WAVEID_PCM_FLOAT 0x0003 // 32/64-bit float le
#define MPXPLAY_WAVEID_AC3       0x2000
#define MPXPLAY_WAVEID_DTS       0x2001
#define MPXPLAY_WAVEID_MP2       0x0050
#define MPXPLAY_WAVEID_MP3       0x0055
#define MPXPLAY_WAVEID_WMAV1     0x0160 // 7.0
#define MPXPLAY_WAVEID_WMAV2     0x0161 // 8.0
#define MPXPLAY_WAVEID_LATMAAC   0x1602 // AAC with LATM header
#define MPXPLAY_WAVEID_AAC       0x706D
#define MPXPLAY_WAVEID_FLAC      0xF1AC
#define MPXPLAY_WAVEID_VORBIS    (('V' << 8) + 'o') // ??? from FFmpeg
// non-standard (internal) wave-ids
#define MPXPLAY_WAVEID_PCM_ULE   0x00017000 // unsigned little endian pcm
#define MPXPLAY_WAVEID_PCM_SBE   0x00017001 // signed big endian pcm
#define MPXPLAY_WAVEID_PCM_UBE   0x00017002 // unsigned big endian pcm
#define MPXPLAY_WAVEID_PCM_DVD   0x00017005 // big endian interleaved 16 + 4/8 bits
#define MPXPLAY_WAVEID_PCM_F32BE 0x00017010 // 32-bit float big endian
#define MPXPLAY_WAVEID_PCM_F64LE 0x00017015 // 64-bit float little endian
#define MPXPLAY_WAVEID_PCM_F64BE 0x00017016 // 64-bit float big endian
#define MPXPLAY_WAVEID_SPEEX     0x00018002
#define MPXPLAY_WAVEID_OPUS      0x00018003
#define MPXPLAY_WAVEID_ALAC      0x00018005
#define MPXPLAY_WAVEID_UNSUPPORTED 0x0001FFFF // !!! higher wave-ids blocked (fails) in decoder.c (modify it if it's required)

//------------------------------------------------------------------------
//main structure definitions

#include "in_file.h"
#include "au_cards\au_cards.h"

#define MPXPLAY_MPXPFRAME_FLAG_OWN_SRVFFMV_CALLBACK  (1 << 0)  // parser has own dispqt_worker callback function (in_ffmpeg.c)
#define MPXPLAY_MPXPFRAME_FLAG_SRVFFMV_CALLBACK      (1 << 1)  // else we use a general callback (in_file.c)
#define MPXPLAY_MPXPFRAME_FLAG_FILEFORMAT_CONTAINER  (1 << 4)

struct mpxpframe_s {
 long frameNum;
 long allframes;
 long timesec;
 unsigned long framecounter;
 long index_start; // =0         if not indexed
 long index_end;   // =allframes if not indexed
 long index_len;   // =allframes if not indexed (avoids inline calculation)

 unsigned int filetype;     // HFT_XXX

 //file/source infos
 mpxp_filesize_t filesize;
 mpxp_filesize_t filepos;
 struct mpxplay_diskdrive_data_s *mdds;
 struct mpxplay_filehand_low_func_s *filehand_funcs;
 void *filehand_datas; // private datas of filehand_funcs
 mpxp_uint64_t readwait_endtime;

 //file-buffer variables
 unsigned int buffertype;
 char *prebufferbegin;
 long prebufferblocksize;
 long prebuffersize;   // frp->prebufferblocksize * prebufferblocks (4 or 32)
 long prebuffergetp;
 long prebufferputp;
 long prebufferbytes_rewind;
 long prebufferbytes_forward;
 mpxp_filesize_t prebuffer_seek_retry;
 struct mpxplay_filehand_buffered_func_s *filebuf_funcs; // static!

 //decoder infos
 struct mpxplay_infile_info_s *infile_infos; // !!! static! don't clear!
 struct mpxplay_infile_func_s *infile_funcs;

 //pcm (decoder,mixer,card)
 mpxp_uint8_t *pcmdec_buffer;
 unsigned int pcmdec_bufsize;
 unsigned int pcmdec_storedsamples;
 unsigned int pcmdec_leftsamples;
 mpxp_uint8_t *pcmout_buffer;
 unsigned int pcmout_bufsize;        // in bytes
 unsigned int pcmout_blocksize;      // samplenum (with ch)
 unsigned int pcmout_storedsamples;  // samplenum (with ch)

 struct mainvars *mvp;
// struct playlist_side_info *psi;
// struct playlist_entry_info *pei;
 struct mpxp_aumixer_passinfo_s *mpi;
// void *mutexhnd_frame;
// void *mutexhnd_fillbuffer;
 int  last_errorcode;
 // for internal use only (DLL API break)
 unsigned int flags;      // MPXPLAY_MPXPFRAME_FLAG_
#if defined(MPXPLAY_LINK_INFILE_FF_MPEG) && defined(MPXPLAY_GUI_QT)
 mpxplay_packetqueue_t audio_packet_queue; // for audio visualization
 mpxp_int64_t avsync_clocktime;
 mpxp_int64_t avsync_timestamp;
#endif
 struct mpxpframe_s *fro; // crosslink (fr[0]->fr[1],fr[1]->fr[0])
};

#include "au_mixer\au_mixer.h"

struct mainvars{
 struct mpxplay_audioout_info_s *aui;
 int idone;                          // end of decoding
 unsigned int sndempty;              // no sound (above soundlimit)
 //unsigned int partselect;            // open newfile or decoding
 //unsigned int foundfile;             // found 1 playable audio file (at least) in the playlist
 //struct crossfade_info *cfi;
 struct mpxp_aumixer_main_info_s mmi;
};

//----------------------------------------------------------------------

//newfunc
//extern int  mpxplay_bitstream_fill(struct mpxplay_bitstreambuf_s *bs,struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,unsigned int needbytes); // fill with a file-read
extern unsigned int newfunc_newhandler08_maincycles_init(struct mainvars *mvp,void *cycleinit,void *cycle1,void *cycle2);

#ifdef __cplusplus
}
#endif

#endif // mpxplay_h
