//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2012 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: routines in newfunc.lib

#ifndef mpxplay_newfunc_h
#define mpxplay_newfunc_h

#include <stdio.h>
#include <stddef.h> // for offsetof

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int   mpxp_bool_t;
typedef long long      mpxp_int64_t;
typedef unsigned long long mpxp_uint64_t;
typedef long           mpxp_int32_t;
typedef unsigned long  mpxp_uint32_t;
typedef short          mpxp_int16_t;
typedef unsigned short mpxp_uint16_t;
typedef signed char    mpxp_int8_t;
typedef unsigned char  mpxp_uint8_t;
typedef float          mpxp_float_t;
typedef double         mpxp_double_t;
#ifdef MPXPLAY_UTF8
typedef mpxp_uint8_t   mpxp_char_t;
typedef mpxp_uint16_t  mpxp_wchar_t;
#else
typedef char           mpxp_char_t;
typedef char           mpxp_wchar_t;
#endif
#ifdef MPXPLAY_ARCH_X64
typedef mpxp_int64_t  mpxp_ptrsize_t;
#else
typedef int           mpxp_ptrsize_t;
#endif
#ifdef MPXPLAY_FSIZE64
 typedef mpxp_int64_t  mpxp_filesize_t;
 #define __WATCOM_INT64__ 1
#else
 typedef long          mpxp_filesize_t;
#endif

#ifndef offsetof
#define offsetof(T, F) ((unsigned int)((char *)&((T *)0)->F))
#endif

#ifndef max
#define max(a,b) (((a)>(b))? (a):(b))
#endif

#ifndef min
#define min(a,b) (((a)<(b))? (a):(b))
#endif

#if defined(SBEMU) && defined(DJGPP)
#include <pc.h>
#define inpd inportl
#define outpd outportl
#endif

 typedef long double mpxp_float80_t; // !!! 64bit in WATCOMC, 96bit in GNU
#define pds_float80_put(p,v) *(p)=(v)
#define pds_float80_get(p)   (*((mpxp_float80_t *)(p)))
#define pds_bswap16(a) ((((a)&0xff)<<8)|(((a)&0xff00)>>8))
#define pds_bswap32(a) ((((a)&0xff)<<24)|(((a)&0xff00)<<8)|(((a)&0xff0000)>>8)|(((a)>>24)&0xff))
#define pds_cpu_hlt

#if defined(__GNUC__) || defined(DJGPP)
 #define pds_swapv32(a) __asm__ ( "bswapl %0\n" : "=r" (a) : "0" (a) )
#elif defined (_WIN32) && !defined(_WIN32_WCE)
 #define pds_swapv32(a) __asm mov eax,a __asm bswap eax __asm mov a, eax
#endif

#define funcbit_test(var,bit)       ((var)&(bit))
#define funcbit_enable(var,bit)     ((var)|=(bit))
#define funcbit_disable(var,bit)    ((var)&=~(bit))
#define funcbit_inverse(var,bit)    ((var)^=(bit))
//#define funcbit_copy(var1,var2,bit) ((var1)|=(var2)&(bit))
#define funcbit_copy(var1,var2,bit) ((var1)=((var1)&(~(bit)))|((var2)&(bit)))

// note LE: lowest byte first, highest byte last
#define PDS_GETB_8S(p)   *((mpxp_int8_t *)(p))               // signed 8 bit (1 byte)
#define PDS_GETB_8U(p)   *((mpxp_uint8_t *)(p))              // unsigned 8 bit (1 byte)
#define PDS_GETB_LE16(p) *((mpxp_int16_t *)(p))              // 2bytes LE to short
#define PDS_GETB_LEU16(p)*((mpxp_uint16_t *)(p))             // 2bytes LE to unsigned short
#define PDS_GETB_BE16(p) pds_bswap16(*((mpxp_uint16_t *)(p)))// 2bytes BE to unsigned short
#define PDS_GETB_LE32(p) *((mpxp_int32_t *)(p))              // 4bytes LE to long
#define PDS_GETB_LEU32(p) *((mpxp_uint32_t *)(p))            // 4bytes LE to unsigned long
#define PDS_GETB_BE32(p) pds_bswap32(*((mpxp_uint32_t *)(p)))// 4bytes BE to unsigned long
#define PDS_GETB_LE24(p) ((PDS_GETB_LEU32(p))&0x00ffffff)
#define PDS_GETB_BE24(p) ((PDS_GETB_BE32(p))>>8)
#define PDS_GETB_LE64(p) *((mpxp_int64_t *)(p))              // 8bytes LE to int64
#define PDS_GETB_LEU64(p) *((mpxp_uint64_t *)(p))            // 8bytes LE to uint64
#define PDS_GETB_BEU64(p) ((((mpxp_uint64_t)PDS_GETB_BE32(p))<<32)|((mpxp_uint64_t)PDS_GETB_BE32(((mpxp_uint8_t *)(p)+4))))
#define PDS_GETBD_BEU64(d,p) *(((mpxp_uint32_t *)(d))+1)=PDS_GETB_BE32(p); *((mpxp_uint32_t *)(d))=PDS_GETB_BE32(((mpxp_uint32_t *)(p))+1)
#define PDS_GET4C_LE32(a,b,c,d) ((mpxp_uint32_t)(a) | ((mpxp_uint32_t)(b) << 8) | ((mpxp_uint32_t)(c) << 16) | ((mpxp_uint32_t)(d) << 24))
#define PDS_GETS_LE32(p) ((char *)&(p))                    // unsigned long to 4 bytes string

#define PDS_PUTB_8S(p,v)   *((mpxp_int8_t *)(p))=(v)               //
#define PDS_PUTB_8U(p,v)   *((mpxp_uint8_t *)(p))=(v)              //
#define PDS_PUTB_LE16(p,v) *((mpxp_int16_t *)(p))=(v)              //
#define PDS_PUTB_LEU16(p,v) *((mpxp_uint16_t *)(p))=(v)            //
#define PDS_PUTB_BEU16(p,v) *((mpxp_uint16_t *)(p))=pds_bswap16((v))//
#define PDS_PUTB_LE24(p,v) *((mpxp_uint8_t *)(p))=((v)&0xff); PDS_PUTB_LE16(((mpxp_uint8_t*)p+1),((v)>>8))
#define PDS_PUTB_LE32(p,v) *((mpxp_int32_t *)(p))=(v)              // long to 4bytes LE
#define PDS_PUTB_BEU32(p,v) *((mpxp_uint32_t *)(p))=pds_bswap32((v)) // long to 4bytes BE
#define PDS_PUTB_LE64(p,v) *((mpxp_int64_t *)(p))=(v)              // int64 to 8bytes LE
#define PDS_PUTB_BEU64(p,v) *((mpxp_uint32_t *)(p)+1)=pds_bswap32((v)&0xffffffff); *((mpxp_uint32_t *)(p))=pds_bswap32((mpxp_uint64_t)(v)>>32)

//#define pds_newfunc_regp_clear(regp) pds_memset(regp,0,sizeof(union REGPACK))
//#define pds_newfunc_regs_clear(regs) pds_memset(regs,0,sizeof(union REGS))
//#define pds_newfunc_sregs_clear(sregs) pds_memset(sregs,0,sizeof(struct SREGS))

#define TEXTCURSORSHAPE_NORMAL  0x0607
#define TEXTCURSORSHAPE_FULLBOX 0x0007
#define TEXTCURSORSHAPE_HIDDEN  0x2000

//uselfn
#define USELFN_ENABLED         1 // lfn is enabled
#define USELFN_AUTO_SFN_TO_LFN 2 // auto sfn to lfn conversion at playlists

typedef struct dosmem_t{
 unsigned short selector;
 unsigned short segment;
 char *linearptr;
}dosmem_t;

#if defined(DJGPP)
typedef struct xmsmem_t{
 unsigned short remap;
 unsigned short xms;
 unsigned short handle;
 char *physicalptr;
 char *linearptr;
}xmsmem_t;
typedef xmsmem_t cardmem_t;
#define pds_cardmem_physicalptr(cardmem, ptr) ((cardmem)->physicalptr + ((char*)(ptr) - (cardmem)->linearptr))
#else
typedef dosmem_t cardmem_t;
#define pds_cardmem_physicalptr(cardmem, ptr) (ptr)
#endif

#if 0
//for DOS interrupt callings
typedef struct rminfo{
 long EDI;
 long ESI;
 long EBP;
 long reserved_by_system;
 long EBX;
 long EDX;
 long ECX;
 long EAX;
 short flags;
 short ES,DS,FS,GS,IP,CS,SP,SS;
}rminfo;

#define RMINFO_FLAG_CARRY 1
#endif

typedef struct pds_fdate_t{
 unsigned short twosecs : 5;
 unsigned short minutes : 6;
 unsigned short hours   : 5;
 unsigned short day     : 5;
 unsigned short month   : 4;
 unsigned short year    : 7;
}pds_fdate_t;

//dpmi.c

#if defined(DJGPP)
#include <sys/segments.h>
#endif
extern int  pds_dpmi_xms_allocmem(xmsmem_t *,unsigned int size);
extern void pds_dpmi_xms_freemem(xmsmem_t *);
extern unsigned long pds_dpmi_map_physical_memory(unsigned long phys_addr,unsigned long memsize);
extern void pds_dpmi_unmap_physical_memory(unsigned long linear_address);

//memory.c
//extern void newfunc_memory_init(void);
#include <string.h>
#define pds_memset(t,v,l)   memset(t,v,l)
#define pds_memcpy(t,s,l)   memcpy(t,s,l)
extern void *pds_malloc(unsigned int bufsize);
extern void *pds_calloc(unsigned int nitems,unsigned int itemsize);
//extern void *pds_realloc(void *bufptr,unsigned int bufsize);
extern void pds_free(void *bufptr);

//time.c
extern unsigned long pds_gettimeh(void); // clock time in hsec
extern mpxp_int64_t pds_gettimem(void);  // clock time in msec
extern mpxp_int64_t pds_gettimeu(void);  // clock time in usec
extern void pds_delay_10us(unsigned int ticks);
extern void pds_mdelay(unsigned long msec);

//timer.c
//extern unsigned long mpxplay_timer_secs_to_counternum(unsigned long secs);
//extern unsigned long mpxplay_timer_msecs_to_counternum(unsigned long msecs);

// safe funcbit handling for multi processors
//#define funcbit_smp_test(var,bit)       funcbit_test(var,bit)
#define funcbit_smp_enable(var,bit)     funcbit_enable(var,bit)
#define funcbit_smp_disable(var,bit)    funcbit_disable(var,bit)
//#define funcbit_smp_inverse(var,bit)    funcbit_inverse(var,bit)
//#define funcbit_smp_copy(var1,var2,bit) funcbit_copy(var1,var2,bit)

//#define funcbit_smp_int32_get(var)       (var)
#define funcbit_smp_int32_put(var,val)   (var)=(val)

#ifdef __cplusplus
}
#endif

#endif // mpxplay_newfunc_h
