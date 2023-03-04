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

#include "newfunc\newfunc.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MPXPLAY_LINK_ORIGINAL_FFMPEG
 #define MPXPLAY_LINK_INFILE_FF_MPEG 1 // ffmpeg library parser
 #define MPXPLAY_LINK_INFILE_OGG  1
#elif defined(MPXPLAY_LINK_FULL)
 #define MPXPLAY_LINK_INFILE_AAC  1
 #define MPXPLAY_LINK_INFILE_AC3  1
 #define MPXPLAY_LINK_INFILE_FLAC 1
 #define MPXPLAY_LINK_INFILE_WAVPACK 1
 #if (defined(__i386__) || defined(__WATCOMC__))
 #define MPXPLAY_LINK_INFILE_APE  1
 #define MPXPLAY_LINK_INFILE_MPX  1
 #define MPXPLAY_LINK_INFILE_MPC  1
 #endif

 #define MPXPLAY_LINK_INFILE_ASF  1
 #define MPXPLAY_LINK_INFILE_AVI  1
 #define MPXPLAY_LINK_INFILE_MP4  1
 #define MPXPLAY_LINK_INFILE_OGG  1
 #define MPXPLAY_LINK_INFILE_WAV  1
 #define MPXPLAY_LINK_INFILE_FFMPG 1 // MKV,MPG,TS

 #define MPXPLAY_LINK_DECODER_AAC  1
 #define MPXPLAY_LINK_DECODER_AC3  1
 #define MPXPLAY_LINK_DECODER_ALAC 1
 #if (defined(__i386__) || defined(__WATCOMC__))
 #define MPXPLAY_LINK_DECODER_MPX  1
 #endif
 #define MPXPLAY_LINK_DECODER_VORBIS 1

 #define MPXPLAY_LINK_DECODER_FFMPG 1 // FLAC,WMA
 #define MPXPLAY_LINK_DECODER_FLAC 1  //
 #define MPXPLAY_LINK_DECODER_WMA  1  //

 //#define MPXPLAY_LINK_VIDEO        1
#else
 #define MPXPLAY_LINK_INFILE_AAC  1
 #if (defined(__i386__) || defined(__WATCOMC__))
 #define MPXPLAY_LINK_INFILE_MPX  1
 #endif
 #define MPXPLAY_LINK_INFILE_MPC  1

 #define MPXPLAY_LINK_INFILE_MP4  1
 #define MPXPLAY_LINK_INFILE_OGG  1
 #define MPXPLAY_LINK_INFILE_WAV  1

 #define MPXPLAY_LINK_DECODER_AAC 1
 #if (defined(__i386__) || defined(__WATCOMC__))
 #define MPXPLAY_LINK_DECODER_MPX 1
 #endif
 #define MPXPLAY_LINK_DECODER_VORBIS 1
#endif

typedef float MPXPLAY_PCMOUT_FLOAT_T;
typedef short MPXPLAY_PCMOUT_INT16_T;

#define MPXPLAY_TIME_BASE 1000 // ms timebase/resolution

// header filetypes in pei->entrytype and frp->filetype
#define HFT_NULL          0
#define HFT_DFT       (1<<0)  // directory & playlist
#define HFT_FILE_INT  (1<<1)  // file handling (internal by Mpxplay)
#define HFT_FILE_EXT  (1<<2)  // non-built-in file handling (QT video)
#define HFT_STREAM    (1<<3)  // live stream (non-seekable, endless)
#define HFT_FILE_DLL  (1<<4)  // file handling (via DLL)  // FIXME: out of pei->entrytype (not problem yet)
#define HFT_FILE_BUILTIN  (HFT_FILE_INT | HFT_FILE_DLL)  // built-in filetype
#define HFT_FILE_ALL      (HFT_FILE_BUILTIN | HFT_FILE_EXT)

#define MPXPLAY_HFT_PUT(mph) ((unsigned long)(mph)<<28) // put HFT number to pei->entrytype (!!! max (1<<3) )
#define GET_HFT(mph) ((mph)>>28) // get HFT number from pei->entrytype

//adi->channelcfg (mp3,ogg)
#define CHM_LEFT    0  // -cl
#define CHM_STEREO  1  // normal (default)
#define CHM_DOWNMIX 2  // -cm (stereo to mono)
#define CHM_USERCNTRL_MASK (CHM_LEFT|CHM_STEREO|CHM_DOWNMIX)

//clearbuff seektypes
#define MPX_SEEKTYPE_NORM   1 // seek in file
#define MPX_SEEKTYPE_BOF    2 // seek to begin of file (reset buffers)
#define MPX_SEEKTYPE_PAUSE  4 // seek in pause mode (can be used with BOF and NORM type)
#define MPX_SEEKTYPES_CLEARBUF (MPX_SEEKTYPE_NORM|MPX_SEEKTYPE_BOF|MPX_SEEKTYPE_PAUSE)
//fseek seektypes
//#define MPX_SEEKTYPE_TIMEMPOS 8  // seek to timepos (in millisecs)
#define MPX_SEEKTYPE_VIDEO    16 // seek to video (key)frame (else seek to audio frame)
#define MPX_SEEKTYPE_FRAME    32 // seek to (video) frame (usually with relative seek)
#define MPX_SEEKTYPE_RELATIVE 64 // -/+ value (else absolute value)
#define MPX_SEEKTYPE_BACKWARD 128 // else forward
#define MPX_SEEKTYPE_PRECISE  256 // not implemented yet
#define MPX_SEEKTYPES_FSEEK   (MPX_SEEKTYPE_VIDEO|MPX_SEEKTYPE_FRAME|MPX_SEEKTYPE_RELATIVE|MPX_SEEKTYPE_BACKWARD|MPX_SEEKTYPE_PRECISE)

//adi->infobits
//info
#define ADI_FLAG_FLOATOUT        (1 << 0) // float audio decoder output (else integer)
#define ADI_FLAG_FPUROUND_CHOP   (1 << 1) // roundtype at float output (else round to nearest)
#define ADI_FLAG_OWN_SPECTANAL   (1 << 2) // own spectrum analiser
#define ADI_FLAG_BITSTREAMOUT    (1 << 3) // bitstream out is enabled/supported (controlled by ADI_CNTRLBIT_BITSTREAMOUT)
#define ADI_FLAG_BITSTREAMNOFRH  (1 << 4) // bitstream out "no-frame-headers" is supported
#define ADI_FLAG_BITSTREAMHEAD   (1 << 5) // write a header at the begin of file (ie: AAC,OGG) (controlled by ADI_CNTRLBIT_BITSTREAMOUT)
#define ADI_FLAG_FLOAT64OUT      (1 << 6) // 64-bit float output
#define ADI_FLAG_FLOAT80OUT      (ADI_FLAG_FLOATOUT | ADI_FLAG_FLOAT64OUT) // 80-bit float output ???
#define ADI_FLAG_FLOATMASK       (ADI_FLAG_FLOATOUT | ADI_FLAG_FLOAT64OUT)
//control
#define ADI_CNTRLBIT_DECODERSPECINF  (1 <<  7) // bitstream (init) contains a decoder specific info
#define ADI_CNTRLBIT_BITSTREAMOUT    (1 <<  8) // bitstream out (don't decode) (write into file or soundcard decoding) (except: APE,CDW,MPC,WAV)
#define ADI_CNTRLBIT_BITSTREAMNOFRH  (1 <<  9) // cut frame headers (usually 4 bytes) with the frame (so don't cut it) (usually: file out with header, soundcard out without header)
#define ADI_CNTRLBIT_SILENTBLOCK     (1 << 10) // frame contains no sound (to disable soundlimitvol)

// for displaying
#define MPXPLAY_ADITEXTSIZE_LONGNAME  8
#define MPXPLAY_ADITEXTSIZE_SHORTNAME 3
#define MPXPLAY_ADITEXTSIZE_BITRATE   8
#define MPXPLAY_ADITEXTSIZE_FREQ      7
#define MPXPLAY_ADITEXTSIZE_CHANNEL   8

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

// infile/decoder error codes
#define MPXPLAY_ERROR_INFILE_OK           0
#define MPXPLAY_ERROR_INFILE_NODATA      -1 // no input data
#define MPXPLAY_ERROR_INFILE_EOF         -2 // end of file (finish playing)
#define MPXPLAY_ERROR_INFILE_SYNCLOST    -3 // sync lost
#define MPXPLAY_ERROR_INFILE_SYNC_IN     -4 // decoder is in sync state (after seeking, or locking frp mutex)
#define MPXPLAY_ERROR_INFILE_RESYNC      -5 // decoder needs resync (seek to next frame)(at wrong bitstream)
#define MPXPLAY_ERROR_INFILE_CANTOPEN   -10 // can't open file as media
#define MPXPLAY_ERROR_INFILE_MEMORY     -11 // memory exhausted
#define MPXPLAY_ERROR_INFILE_FILEOPEN   -12 // can't open low level file
#define MPXPLAY_ERROR_INFILE_FILETYPE   -13 // file has invalid type (no extension)
#define MPXPLAY_ERROR_INFILE_SPIBUFSIZE -14 // mpxplay_streampacket_info bufsize is small
#define MPXPLAY_ERROR_INFILE_READONLYFS -20 // read only filesystem / filetype
#define MPXPLAY_ERROR_INFILE_WRITETAG_FILETYPE   -21 // not supported filetype (dts,wav)
#define MPXPLAY_ERROR_INFILE_WRITETAG_TAGTYPE    -22 // not supported tagtype (APEv2)
#define MPXPLAY_ERROR_INFILE_WRITETAG_NOSPACE    -23 // not enough space to store id3 infos
#define MPXPLAY_ERROR_INFILE_WRITETAG_NOSPMMRQ   -24 // no space, move-meta request
#define MPXPLAY_ERROR_INFILE_WRITETAG_UNKNOWN    -29 // unknown error

#define MPXPLAY_WRITETAG_CNTRL_TRIMTAGS (1<<0) // trim/cut tags if there's not enough space to store them
#define MPXPLAY_WRITETAG_CNTRL_DUPFILE  (1<<1) // duplicate file and write all tags
#define MPXPLAY_WRITETAG_CNTRL_MOVEMETA (1<<2) // move metadata to the end of file (MP4 only)
#define MPXPLAY_WRITETAG_CNTRL_NOPADD   (1<<9) // don't add padding (vorbiscomment)

// *************************************************************************
// audio decoder

#define MPXPLAY_SPI_STREAMTYPE_UNKNOWN  0
#define MPXPLAY_SPI_STREAMTYPE_AUDIO    1
#define MPXPLAY_SPI_STREAMTYPE_VIDEO    2
#define MPXPLAY_SPI_STREAMTYPE_SUBTITLE 3
#define MPXPLAY_SPI_STREAMTYPE_DATA     4

#define MPXPLAY_SPI_FLAG_NEED_DECODER   (1 <<  0) // search for a decoder by wave_id or fourcc
#define MPXPLAY_SPI_FLAG_NEED_PARSING   (1 <<  1) // parse audio infos by adf->parse_frame (get informations from the stream, passed in spi->bitstreambuf)
#define MPXPLAY_SPI_FLAG_CONTAINER      (1 <<  2) // stream data is in container (run a longer parsing)
#define MPXPLAY_SPI_FLAG_BS_ERROR       (1 <<  4) // bitstream error detected in demuxer
#define MPXPLAY_SPI_FLAG_SILENTBLOCK    (1 <<  5) // pcm frame contains no sound (to disable soundlimitvol)

#define MPXPLAY_SPI_MINBSREADSIZE          32 // ???
#define MPXPLAY_SPI_MAXBSREADSIZE_AUDIO 32768 // pre-init sizes
#define MPXPLAY_SPI_MAXBSREADSIZE_VIDEO 65536

#define MPXPLAY_SPI_EXTRADATA_PADDING 8

typedef struct mpxplay_streampacket_info_s{
 unsigned int  flags;
 //config
 unsigned int  stream_select;  // for this streamtype
 //info
 unsigned int  streamtype;     // audio/video/other
 unsigned int  wave_id;        // or fourcc in 4C_LE32
 unsigned int  nb_streams;     // number of streams for this streamtype
 unsigned int  block_align;    // WAV,WMA

 mpxp_uint8_t  *extradata;     // decoder extra data (use as a pointer, don't alloc directly to it)
 unsigned long  extradata_size;

 //decoding
 mpxp_uint8_t  *bitstreambuf;  // from demuxer/parser to decoder
 unsigned long  bs_bufsize;    //
 unsigned long  bs_framesize;  // adf->maxbsframesize or set by parser or set by Mpxplay (MAXBSREADSIZE)
 unsigned long  bs_readsize;   // used by (raw) parsers (mp3,ac3,dts) (usually =bs_framesize)
 unsigned long  bs_leftbytes;  //
 unsigned long  bs_usedbytes;  //

 //struct mpxplay_streampacket_info_s *next_demuxerdata;
 //void                               *next_demuxerfunc;
 unsigned char reserved[6*4];
}mpxplay_streampacket_info_s; // 20*4=80 bytes

typedef struct mpxplay_audio_decoder_info_s{
 void *private_data;         // decoder
 //info
 unsigned int infobits;      // flags
 unsigned int wave_id;       // audio type (wav:0x0001,mp3:0x0055,ac3:0x2000)
 unsigned int freq;          // frequency (44100,48000,...)
 unsigned int filechannels;  // number of channels in file
 unsigned int outchannels;   // decoded (used) channels, comes out from the decoder
 mpxp_uint8_t *chanmatrix;   // output channel matrix (ie: 5.1)
 unsigned int bits;          // 8,16 ... (scalebits at float,filebits at integer output)
 unsigned int bytespersample;// used in au_mixer
 unsigned int bitrate;       // in kbit/s (lossy formats)(ie: mp3,ogg,ac3,dts)
 unsigned int pcm_framelen;  // comes out from the decoder (samplenum without channels!)(=bytes/bytespersample)
 mpxp_float_t replaygain;    // not used yet
 char *shortname;            // set by decoder (3 chars), can static ("MP3","OGG","AC3","WMA")
 char *bitratetext;          // set by decoder (8 chars), else displays bitrate (if exists) or bits
 char *freqtext;             // set by decoder (7 chars), else displays freq
 char *channeltext;          // set by decoder (8 chars), can static ("msStereo","i-Stereo","DualChan","c-Stereo","5.1 chan") else displays 1ch->"mono",2ch->"stereo",Nch->"n-chan")
 //control
 unsigned int  channelcfg;    // configure output channels (eq to channelmode)
 //pcm out
 mpxp_uint8_t *pcm_bufptr;    // head ptr in pcm_buffer
 unsigned int  pcm_samplenum; // got back from decoder (with ch)
 unsigned char reserved[5*4];
}mpxplay_audio_decoder_info_s;// 24*4=96 bytes

typedef struct mpxplay_audio_decoder_type_s{
 unsigned int wave_id;  // 0x0055,????,0x2000,0x0160
 char *wave_name;       // "MP3","OGG","AC3","WMA"
}mpxplay_audio_decoder_type_s;

typedef struct mpxplay_audio_decoder_func_s{
 unsigned long flags;

 void (*preinit)(void); // initialize static data fields (mp3)
 void (*deinit) (void); // close audio decoder (cdw)
 void (*config_decoder)(void *notused); // not used yet

 int  (*open)  (struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi);
 void (*close) (struct mpxplay_audio_decoder_info_s *adi);
 int  (*parse_extra) (struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi);
 int  (*parse_frame) (struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi);
 int  (*decode)(struct mpxplay_audio_decoder_info_s *adi,struct mpxplay_streampacket_info_s *spi);
 void (*clearbuf)(struct mpxplay_audio_decoder_info_s *adi,unsigned int mpx_seektype);

 void (*get_analiser_bands)(struct mpxplay_audio_decoder_info_s *adi,unsigned int bandnum,unsigned long *banddataptr);
 void (*set_eq)(struct mpxplay_audio_decoder_info_s *adi,unsigned int bandnum,unsigned long *band_freqs,float *band_powers);

 unsigned long maxbsframesize;  // max. size of input (bitstream) frames
 unsigned long maxpcmblocksamplenum; // max. pcm block (frame) size (!!! have to multiply with channelnum and bytes_per_sample)
 mpxplay_audio_decoder_type_s decodertypes[];
}mpxplay_audio_decoder_func_s;

#ifdef __cplusplus
}
#endif

#endif // mpxplay_in_file_h
