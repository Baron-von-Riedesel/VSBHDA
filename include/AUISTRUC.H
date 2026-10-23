#ifndef auistruc_h
#define auistruc_h

#ifdef __cplusplus
extern "C" {
#endif

/* interface definitions vsbhda <-> drivers:
 * 1. struct audioout_info_s
 * 2. struct sndcard_info_s
 */

/* r/w access for cards */
enum card_ctrlbits {
    AUI_CARDCTRLBIT_DMACLEAR = 1, /* run AU_clearbuffs */
};

/* r/o access for cards? */
enum card_infobits {
    AUI_CARDINFOBIT_PLAYING = 1,
    //AUI_CARDINFOBIT_DMAUNDERRUN = 8, /* dma buffer dried out */
    //AUI_CARDINFOBIT_DMAFULL = 16,    /* dma buffer is full */
};

/* struct sndcard_info_s->infobits */
enum scinfobits {
    SCINFO_CARDBUF_SPACE = 1, /* cardbuf_getpos() returns "free" buffer space, not the buffer position */
};

/* sound card PCM format */
enum pcmformat {
    //WAVEID_UNKNOWN = 0,
    WAVEID_PCM_SLE = 1, /* signed little endian */
    WAVEID_PCM_FLOAT = 3, /* 32/64-bit float */
};

//au_cards mixer channels - see AC97MIX.C, aucards_ac97_chan_mixerset[]
enum MIXCHAN {
    AU_MIXCHAN_MASTER = 0,  // master out
    AU_MIXCHAN_PCM,         // pcm out
    AU_MIXCHAN_HEADPHONE,   // headphone out
    AU_MIXCHAN_SPDIFOUT,    // digital out
    AU_MIXCHAN_SYNTH,       // midi/synth out (not used)

    AU_MIXCHAN_MICIN,       // MIC input
    AU_MIXCHAN_LINEIN,      // LINE in
    AU_MIXCHAN_CDIN,        // CD in
    AU_MIXCHAN_AUXIN,       // AUX in
    AU_MIXCHAN_BASS,        // (not used)
    AU_MIXCHAN_TREBLE,      // (not used)
    AU_MIXCHANS_NUM
};

// aucards_mixchandata_s->channeltype
enum MIXFUNC {
    AU_MIXCHANFUNC_VOLUME = 0,  // volume control (of master,pcm,etc.)
    AU_MIXCHANFUNC_MUTE,        // mute switch (of master,pcm,etc.)
    AU_MIXCHANFUNCS_NUM         // number of mixchanfuncs
};

#define AU_MIXCHAN_MAX_VALUE_VOLUME    100  // master, pcm, ...
#define AU_MIXCHAN_MAX_VALUE_TONE      200  // bass, treble

//for verifying
#define AU_MIXERCHAN_MAX_SUBCHANNELS    8 // this is enough for a 7.1 setting too :)
#define AU_MIXERCHAN_MAX_REGISTER   65535 // check this again at future cards (2^bits)
#define AU_MIXERCHAN_MAX_BITS          32 //
//#define AU_MIXERCHAN_MAX_VALUE 0xffffffff // 2^32-1

typedef int32_t  PCM_CV_TYPE_I;  // max. 32 bit input/output pcm format (integer)
typedef uint32_t PCM_CV_TYPE_UI; //
typedef float    PCM_CV_TYPE_F;  // 32-bit float part of mixer
typedef int16_t  PCM_CV_TYPE_S;  // 16-bit integer part of mixer
typedef int8_t   PCM_CV_TYPE_C;  // 8-bit signed
typedef uint8_t  PCM_CV_TYPE_UC; // 8-bit unsigned
typedef PCM_CV_TYPE_F PCM_CV_TYPE_MAX;// largest datatype (currently)

enum MIXMODE {
    MIXER_SETMODE_RELATIVE = 0, /* not used */
    MIXER_SETMODE_ABSOLUTE = 1,
    //MIXER_SETMODE_RESET    = 2
};

struct sndcard_info_s;

/* v2.1: if one of the first 3 members ( pointers ) are changed,
 *       it may be necessary to adjust struct AUI in loaddrv.asm!
 */
struct audioout_info_s {
    void *card_private_data;         /* card's private data - size defined by sndcard_info_s.private_data_size */
    const struct sndcard_info_s *card_handler; /* card's info block - v2.1: now here to simplify dynamic loading */
    char         *card_pDmaBuffer;   /* set if the card driver doesn't want to handle PCM out data on its own */
    unsigned int  freq_card;
    unsigned char chan_card;
    unsigned char bits_card;
    unsigned char sampleshift_card;  /* samples << sampleshift_card = size of samples in bytes */
    unsigned char card_irq;
    unsigned char card_bytespersign; /* bytespersample_card * chan_card */
    unsigned char card_buffers;      /* value of option /B */
    unsigned char card_devidx;       /* value of option /DEV */
    signed char card_outdev;         /* value of option /O - used by HDA and SBLIVE */
    unsigned char bOMode;            /* print output flags - to be used by external drivers */
    enum pcmformat card_wave_id;     /* PCM format - usually 16-bit signed */
    enum card_ctrlbits card_ctrlbits;
    enum card_infobits card_infobits;
    unsigned int  card_period_size;  /* value of option /PS */
    unsigned int  card_dmasize;      /* card's dma buffer size in bytes; set in dmabuff.c */
    unsigned int  card_dmalastput;   /* current dma buffer write offset; updated in AU_writedata() */
    unsigned int  card_dmaspace;     /* "free" dma buffer size in bytes; set in AU_get_bufpos_space() */
                                     /* should usually be set to ~period size if no IRQs got lost */
    unsigned int  card_buffer_protection; /* v2.0: gap between read/write buffer pointer in bytes */
    short card_master_volume;
    short card_mixer_values[AU_MIXCHANS_NUM]; // -1, 0-100
    //int card_mixer_values[AU_MIXCHANS_NUM][AU_MIXCHANFUNCS_NUM]; // -1, 0-100
};

enum SubMixChFlags {
    SUBMIXCH_INFOBIT_REVERSEDVALUE = 1,  /* reversed value */
    SUBMIXCH_INFOBIT_CARD_SETVOL   = 4,  /* let the card driver handle volume settings */
};

/* soundcard mixer structures.
 * if SUBMIXCH_INFOBIT_CARD_SETVOL is set, then
 * fields _register, _bits and _shift are free to use by the driver code;
 * i.e. field submixch_register for HDA contains a node id.
 */
struct aucards_submixerchan_s {
    unsigned long submixch_register; // register-address of channel
    uint8_t submixch_bits;           // no of bits for max value & mask
    uint8_t submixch_shift;          // bit-shift from 0.
    enum SubMixChFlags submixch_infobits;
};

struct aucards_mixerchan_s {
    enum MIXCHAN channel;   // master,pcm,etc.
    enum MIXFUNC function;  // volume or mute
    uint16_t subchannelnum; // # of sub-channels in submixerchans[]
    struct aucards_submixerchan_s submixerchans[]; // infos of 1-n subchannels
};

struct sndcard_info_s {
    char *shortname;
    enum scinfobits infobits;
    int   (*card_detect)(struct audioout_info_s *); // try to autodetect the card
    void   (*card_start)(struct audioout_info_s *); // start playing
    void    (*card_stop)(struct audioout_info_s *); // stop playing (immediately)
    void   (*card_close)(struct audioout_info_s *); // close soundcard
    void (*card_setrate)(struct audioout_info_s *); // set freqency,channels,bits

    void      (*cardbuf_writedata)(struct audioout_info_s *, char *buffer, unsigned int bytes); // write data into the buffer
    unsigned int (*cardbuf_getpos)(struct audioout_info_s *); // get the buffer write pointer ( or "free" space )
    int             (*irq_routine)(); // check if IRQ is caused by card and acknowledge

    void          (*card_writemixer)(struct audioout_info_s *, unsigned long mixreg, unsigned long value);
    unsigned long  (*card_readmixer)(struct audioout_info_s *, unsigned long mixreg);
    const struct aucards_mixerchan_s **card_mixerchans;
    unsigned int private_data_size;
};

#ifdef __cplusplus
}
#endif

#endif // auistruc_h
