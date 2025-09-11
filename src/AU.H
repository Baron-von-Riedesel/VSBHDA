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
//extern void AU_setmixer_one( void *, unsigned int mixch, unsigned int setmode, int value );
extern void AU_setmixer_outs( void *, unsigned int setmode, int newvalue );
//extern void AU_setmixer_all( void * );
//extern void AU_clearbuffs( void * );
extern unsigned int AU_cardbuf_space( void * );
extern int  AU_writedata( void *, int, void * );

#ifdef __cplusplus
}
#endif

#endif // au_h
