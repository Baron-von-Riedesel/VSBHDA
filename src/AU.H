#ifndef au_h
#define au_h

/* interface to the mpxplay card handler */

#ifdef __cplusplus
extern "C" {
#endif

enum MIXMODE {
    MIXER_SETMODE_RELATIVE = 0,
    MIXER_SETMODE_ABSOLUTE = 1
};

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

enum MIXFUNC {
    AU_MIXCHANFUNC_VOLUME = 0,  // volume control (of master,pcm,etc.)
    AU_MIXCHANFUNC_MUTE,        // mute switch (of master,pcm,etc.)
    AU_MIXCHANFUNCS_NUM         // number of mixchanfuncs
};

typedef int8_t  PCM_CV_TYPE_SC;
typedef uint8_t PCM_CV_TYPE_UC;
typedef int16_t PCM_CV_TYPE_S;

extern void *AU_init( struct globalvars * );
extern int  AU_getirq( void * );
extern int  AU_getfreq( void * );
extern char *AU_getshortname( void * );
extern int  AU_isirq( void * );
extern void AU_setoutbytes( void * );
//extern void AU_setsamplenum( void * );
//extern void AU_prestart( void * );
extern void AU_start( void * );
extern void AU_stop( void * );
extern void AU_close( void * );
extern int  AU_setrate( void * , int, int, int );
extern void AU_setmixer_init( void * );
extern void AU_setmixer_one( void *, unsigned int channel, int function, unsigned int setmode, int value );
extern void AU_setmixer_outs( void *, unsigned int setmode, int newvalue );
//extern void AU_setmixer_all( void * );
//extern void AU_clearbuffs( void * );
extern unsigned int AU_cardbuf_space( void * );
extern int  AU_writedata( void *, int, void * );

#ifdef __cplusplus
}
#endif

#endif // au_h
