//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2011 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
// in-file and decoder definitions

#ifndef mpxplay_in_file_h
#define mpxplay_in_file_h

#include "NEWFUNC.H"

#ifdef __cplusplus
extern "C" {
#endif

//adi->infobits
//info
#define ADI_FLAG_BITSTREAMOUT    (1 << 3) // bitstream out is enabled/supported (controlled by ADI_CNTRLBIT_BITSTREAMOUT)
#define ADI_FLAG_BITSTREAMNOFRH  (1 << 4) // bitstream out "no-frame-headers" is supported
#define ADI_FLAG_BITSTREAMHEAD   (1 << 5) // write a header at the begin of file (ie: AAC,OGG) (controlled by ADI_CNTRLBIT_BITSTREAMOUT)
//control
#define ADI_CNTRLBIT_DECODERSPECINF  (1 <<  7) // bitstream (init) contains a decoder specific info
#define ADI_CNTRLBIT_BITSTREAMOUT    (1 <<  8) // bitstream out (don't decode) (write into file or soundcard decoding) (except: APE,CDW,MPC,WAV)
#define ADI_CNTRLBIT_BITSTREAMNOFRH  (1 <<  9) // cut frame headers (usually 4 bytes) with the frame (so don't cut it) (usually: file out with header, soundcard out without header)
#define ADI_CNTRLBIT_SILENTBLOCK     (1 << 10) // frame contains no sound (to disable soundlimitvol)

// adi->chanmatrix, aucards_card_channelmap
#define MPXPLAY_PCMOUTCHAN_FRONT_LEFT    0
#define MPXPLAY_PCMOUTCHAN_FRONT_RIGHT   1
#define MPXPLAY_PCMOUTCHAN_FRONT_CENTER  2
#define MPXPLAY_PCMOUTCHAN_REAR_LEFT     3
#define MPXPLAY_PCMOUTCHAN_REAR_RIGHT    4
#define MPXPLAY_PCMOUTCHAN_LFE           5
#define MPXPLAY_PCMOUTCHAN_REAR_CENTER   6
#define MPXPLAY_PCMOUTCHAN_SIDE_LEFT     7
#define MPXPLAY_PCMOUTCHAN_SIDE_RIGHT    8
#define MPXPLAY_PCMOUTCHAN_FCENTER_LEFT  9
#define MPXPLAY_PCMOUTCHAN_FCENTER_RIGHT 10
#define MPXPLAY_PCMOUTCHAN_DISABLED      126
#define MPXPLAY_PCMOUTCHAN_MAX           11

// *************************************************************************
// audio decoder

/* audio_decoder_info is used as input for AU_setrate() */
typedef struct mpxplay_audio_decoder_info_s{
 void *private_data;         // decoder
 //info
 unsigned int infobits;      // flags
 unsigned int wave_id;       // audio type (wav:0x0001,mp3:0x0055,ac3:0x2000)
 unsigned int freq;          // frequency (44100,48000,...)
 unsigned int filechannels;  // number of channels in file ( not used by SBEMU )
 unsigned int outchannels;   // decoded (used) channels, comes out from the decoder
 uint8_t *chanmatrix;        // output channel matrix (ie: 5.1)
 unsigned int bits;          // 8,16 ... (scalebits at float,filebits at integer output)
 unsigned int bytespersample;// used in au_mixer
 unsigned int bitrate;       // in kbit/s (lossy formats)(ie: mp3,ogg,ac3,dts)
 unsigned int pcm_framelen;  // comes out from the decoder (samplenum without channels!)(=bytes/bytespersample)
 //float_t replaygain;         // not used yet
 char *shortname;            // set by decoder (3 chars), can static ("MP3","OGG","AC3","WMA")
 //char *bitratetext;          // set by decoder (8 chars), else displays bitrate (if exists) or bits
 //char *freqtext;             // set by decoder (7 chars), else displays freq
 //char *channeltext;          // set by decoder (8 chars), can static ("msStereo","i-Stereo","DualChan","c-Stereo","5.1 chan") else displays 1ch->"mono",2ch->"stereo",Nch->"n-chan")
 //control
 // unsigned int  channelcfg;    // configure output channels (eq to channelmode)
 //pcm out
 //uint8_t *pcm_bufptr;         // head ptr in pcm_buffer
 //unsigned int  pcm_samplenum; // got back from decoder (with ch)
 //unsigned char reserved[5*4];
}mpxplay_audio_decoder_info_s;// 24*4=96 bytes

#ifdef __cplusplus
}
#endif

#endif // mpxplay_in_file_h
