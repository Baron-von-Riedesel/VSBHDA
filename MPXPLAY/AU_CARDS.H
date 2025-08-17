#ifndef au_cards_h
#define au_cards_h

#ifdef __cplusplus
extern "C" {
#endif

//au_infos->card_controlbits
//#define AUINFOS_CARDCTRLBIT_TESTCARD         1
//#define AUINFOS_CARDCTRLBIT_DOUBLEDMA        2
//#define AUINFOS_CARDCTRLBIT_MIDASMANUALCFG   4
#define AUINFOS_CARDCTRLBIT_DMACLEAR         8 // run AU_clearbuffs
//#define AUINFOS_CARDCTRLBIT_DMADONTWAIT     16 // don't wait for card-buffer space (at seeking)
//#define AUINFOS_CARDCTRLBIT_AUTOTAGGING    256 // copy tags (id3infos) from infile to outfile (if possible)
//#define AUINFOS_CARDCTRLBIT_AUTOTAGLFN     512 // create filename from id3infos (usually "NN. Artist - Title.ext"
//#define AUINFOS_CARDCTRLBIT_UPDATEFREQ    1024 // change/update soundcard freq

//au_infos->card_infobits
#define AUINFOS_CARDINFOBIT_PLAYING          1
//#define AUINFOS_CARDINFOBIT_IRQRUN           2
//#define AUINFOS_CARDINFOBIT_IRQSTACKBUSY     4
#define AUINFOS_CARDINFOBIT_DMAUNDERRUN      8 // dma buffer is empty (set by dma-monitor)
#define AUINFOS_CARDINFOBIT_DMAFULL         16 // dma buffer is full (set by AU_writedata)
//#define AUINFOS_CARDINFOBIT_HWTONE          32 // ???

//struct sndcard_info_s->infobits
//#define SNDCARD_SELECT_ONLY     1 // program doesn't try to use automatically (ie: wav output)
//#define SNDCARD_INT08_ALLOWED   2 // use of INT08 (and interrupt decoder) is allowed
#define SNDCARD_CARDBUF_SPACE   4 // routine gives back the cardbuf space, not the bufpos
//#define SNDCARD_SPECIAL_IRQ     8 // card has a special (long) irq routine (requires stack & irq protection)
//#define SNDCARD_SETRATE        16 // always call setrate before each song (special wav-out and test-out flag!)
//#define SNDCARD_LOWLEVELHAND   32 // native soundcard handling (PCI)
//#define SNDCARD_IGNORE_STARTUP 64 // ignore startup (do not restore songpos) (ie: wav out)
//#define SNDCARD_FLAGS_DISKWRITER (SNDCARD_SELECT_ONLY|SNDCARD_SETRATE|SNDCARD_IGNORE_STARTUP)

//au_cards mixer channels - see AC97.C, aucards_ac97_chan_mixerset[]
#define AU_MIXCHAN_MASTER       0  // master out
#define AU_MIXCHAN_PCM          1  // pcm out
#define AU_MIXCHAN_HEADPHONE    2  // headphone out
#define AU_MIXCHAN_SPDIFOUT     3  // digital out
#define AU_MIXCHAN_SYNTH        4  // midi/synth out

#define AU_MIXCHAN_MICIN        5  // MIC input
#define AU_MIXCHAN_LINEIN       6  // LINE in
#define AU_MIXCHAN_CDIN         7  // CD in
#define AU_MIXCHAN_AUXIN        8  // AUX in
#define AU_MIXCHAN_BASS         9  // !!! default: -1 in au_cards, +50 in au_mixer
#define AU_MIXCHAN_TREBLE      10  // -"-
#define AU_MIXCHANS_NUM        11

// aucards_mixchandata_s->channeltype
#define AU_MIXCHANFUNC_VOLUME 0  // volume control (of master,pcm,etc.)
#define AU_MIXCHANFUNC_MUTE   1  // mute switch (of master,pcm,etc.)
#define AU_MIXCHANFUNCS_NUM   2  // number of mixchanfuncs

#define AU_MIXCHANFUNCS_FUNCSHIFT 8
#define AU_MIXCHANFUNCS_FUNCMASK  0xff
#define AU_MIXCHANFUNCS_CHANMASK  ((1<<AU_MIXCHANFUNCS_FUNCSHIFT)-1)
#define AU_MIXCHANFUNCS_PACK(chan,func) (((func) << AU_MIXCHANFUNCS_FUNCSHIFT)|(chan))
#define AU_MIXCHANFUNCS_GETCHAN(c) ((c) & AU_MIXCHANFUNCS_CHANMASK)
#define AU_MIXCHANFUNCS_GETFUNC(c) (((c) >> AU_MIXCHANFUNCS_FUNCSHIFT) & AU_MIXCHANFUNCS_FUNCMASK)

#define AU_MIXCHAN_MAX_VALUE_VOLUME    100  // mater, pcm, etc
#define AU_MIXCHAN_MAX_VALUE_TONE      200  // bass, treble

//for verifying
#define AU_MIXERCHAN_MAX_SUBCHANNELS    8 // this is enough for a 7.1 setting too :)
#define AU_MIXERCHAN_MAX_REGISTER   65535 // check this again at future cards (2^bits)
#define AU_MIXERCHAN_MAX_BITS          32 //
#define AU_MIXERCHAN_MAX_VALUE 0xffffffff // 2^32-1

typedef int32_t  PCM_CV_TYPE_I;  // max. 32 bit input/output pcm format (integer)
typedef uint32_t PCM_CV_TYPE_UI; //
typedef float    PCM_CV_TYPE_F;  // 32-bit float part of mixer
typedef int16_t  PCM_CV_TYPE_S;  // 16-bit integer part of mixer
typedef int8_t   PCM_CV_TYPE_C;  // 8-bit signed
typedef uint8_t  PCM_CV_TYPE_UC; // 8-bit unsigned
typedef PCM_CV_TYPE_F PCM_CV_TYPE_MAX;// largest datatype (currently)

#define MIXER_SETMODE_RELATIVE 0
#define MIXER_SETMODE_ABSOLUTE 1
#define MIXER_SETMODE_RESET    2

struct sndcard_info_s;

struct audioout_info_s {
 short *pcm_sample;
 unsigned int  samplenum;
 unsigned char bytespersample_card;
 unsigned int  freq_set;  /* ??? */
 unsigned int  freq_card; /* set in AU_setrate */
 unsigned int  chan_set;  /* ??? */
 unsigned char chan_card; /* set in AU_setrate */
 unsigned int  bits_set;  /* ??? */
 unsigned char bits_card; /* set in AU_setrate */

 unsigned int   card_wave_id;    // 0x0001,0x0003,0x0055,0x2000,etc.
 unsigned long  card_controlbits;  // card control flags
 unsigned long  card_infobits;     // card info flags
 unsigned long  card_outbytes;     // samplenum * bytespersample_card
 unsigned long  card_dmasize;      /* card's dma buffer size in bytes; set in dmairq.c */
 unsigned long  card_dmalastput;
 unsigned long  card_dmaspace;     /* should be "free" dma buffer size in bytes, but changed for sbemu... */
 unsigned long  card_dmafilled;    /* ??? */
 unsigned long  card_dma_lastgoodpos;
 unsigned int   card_bytespersign; // bytespersample_card * chan_card
 char          *card_DMABUFF;      /* identical with the card's pcmout_buffer */

 const struct sndcard_info_s *card_handler; // function structure of the card
 const struct globalvars *gvars;
 void *card_private_data;        // extra private datas can be pointed here (with malloc)
 unsigned char  card_irq;
 int card_master_volume;
 int card_mixer_values[AU_MIXCHANS_NUM]; // -1, 0-100
 //int card_mixer_values[AU_MIXCHANS_NUM][AU_MIXCHANFUNCS_NUM]; // -1, 0-100
};

//aucards_submixerchan_s->infobits
#define SUBMIXCH_INFOBIT_REVERSEDVALUE  1 // reversed value
//#define SUBMIXCH_INFOBIT_SUBCH_SWITCH   2 // set register if value!=submixch_max
//new for vsbhda
#define SUBMIXCH_INFOBIT_CARD_SETVOL    4 // let the card software handle volume settings

/* soundcard mixer structures.
 * vsbhda: fields submixch_max and submixch_shift aren't used if SUBMIXCH_INFOBIT_CARD_SETVOL is set,
 * which is always true for HDA.
 * field submixch_register for HDA contains a node id, btw.
 */
struct aucards_submixerchan_s {
 unsigned long submixch_register; // register-address of channel
 unsigned long submixch_max;      // max value (and mask) (must be 2^n-1 (ie:1,3,7,15,31,63,127,255))
 unsigned long submixch_shift;    // bit-shift from 0.
 unsigned long submixch_infobits; //
};

struct aucards_mixerchan_s {
 unsigned long mixchan;       // master,pcm,etc. & volume,mute-sw
 unsigned long subchannelnum; // sub-channels (mono (1) or left&right (2))
 struct aucards_submixerchan_s submixerchans[]; // infos of 1 or 2 subchannels (reg,max,shift,flag)
};

struct sndcard_info_s {
 char *shortname;
 unsigned long infobits;

 int  (*card_config)(struct audioout_info_s *); // not used yet
 int  (*card_init)(struct audioout_info_s *);   // always NULL here
 int  (*card_detect)(struct audioout_info_s *); // try to autodetect the card
 void (*card_shinfo)(struct audioout_info_s *); // show card infos
 void (*card_start)(struct audioout_info_s *);  // start playing
 void (*card_stop)(struct audioout_info_s *);   // stop playing (immediately)
 void (*card_close)(struct audioout_info_s *);  // close soundcard
 void (*card_setrate)(struct audioout_info_s *);// set freqency,channels,bits

 void (*cardbuf_writedata)(struct audioout_info_s *,char *buffer,unsigned long bytes); // write output data into the card's buffer
 long (*cardbuf_getpos)(struct audioout_info_s *);  // get the buffer (playing) position (usually the DMA buffer get-position)(returns negative number on error)
 void (*cardbuf_clear)(struct audioout_info_s *);// clear the soundcard buffer (usually the DMA buffer)
 //void (*cardbuf_int_monitor)(struct audioout_info_s *); // interrupt (DMA) monitor function
 int (*irq_routine)(struct audioout_info_s *);  // vsbhda: check if IRQ is caused by card and acknowledge

 void (*card_writemixer)(struct audioout_info_s *,unsigned long mixreg,unsigned long value);
 unsigned long (*card_readmixer)(struct audioout_info_s *,unsigned long mixreg);
 const struct aucards_mixerchan_s **card_mixerchans;
};

#ifdef NOTFLAT
#define FAREXP _far
#else
#define FAREXP
#endif

//main soundcard routines
extern int  FAREXP AU_init( const struct globalvars * );
extern int  FAREXP AU_getirq( struct audioout_info_s * );
//extern void FAREXP AU_prestart( struct audioout_info_s * );
extern void FAREXP AU_start( struct audioout_info_s * );
extern void FAREXP AU_stop( struct audioout_info_s * );
extern void FAREXP AU_close( struct audioout_info_s * );
extern int  FAREXP AU_setrate( struct audioout_info_s * , int, int, int );
extern void FAREXP AU_setmixer_init( struct audioout_info_s * );
extern void FAREXP AU_setmixer_one( struct audioout_info_s *, unsigned int mixch, unsigned int setmode, int value );
extern void FAREXP AU_setmixer_outs( struct audioout_info_s *, unsigned int setmode, int newvalue );
extern void FAREXP AU_setmixer_all( struct audioout_info_s * );
static void AU_clearbuffs( struct audioout_info_s * );
extern unsigned int FAREXP AU_cardbuf_space( struct audioout_info_s * );
extern int  FAREXP AU_writedata( struct audioout_info_s *, int, void * );

#ifdef __cplusplus
}
#endif

#endif // au_cards_h
