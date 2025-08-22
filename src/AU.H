#ifndef au_h
#define au_h

/* interface to the mpxplay card handler */

#ifdef __cplusplus
extern "C" {
#endif

#define MIXER_SETMODE_RELATIVE 0
#define MIXER_SETMODE_ABSOLUTE 1

typedef int8_t  PCM_CV_TYPE_SC;
typedef uint8_t PCM_CV_TYPE_UC;
typedef int16_t PCM_CV_TYPE_S;

extern int  AU_init( struct globalvars * );
extern int  AU_getirq( int );
extern int  AU_getfreq( int );
extern char *AU_getshortname( int );
extern int  AU_isirq( int );
extern void AU_setoutbytes( int );
//extern void AU_setsamplenum( int );
//extern void AU_prestart( int );
extern void AU_start( int );
extern void AU_stop( int );
extern void AU_close( int );
extern int  AU_setrate( int , int, int, int );
extern void AU_setmixer_init( int );
//extern void AU_setmixer_one( int, unsigned int mixch, unsigned int setmode, int value );
extern void AU_setmixer_outs( int, unsigned int setmode, int newvalue );
//extern void AU_setmixer_all( int );
//extern void AU_clearbuffs( int );
extern unsigned int AU_cardbuf_space( int );
extern int  AU_writedata( int, int, void * );

#ifdef __cplusplus
}
#endif

#endif // au_h
