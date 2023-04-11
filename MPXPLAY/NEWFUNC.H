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

#include <stddef.h> // for offsetof

#ifdef __cplusplus
extern "C" {
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

// note LE: lowest byte first, highest byte last
#define PDS_GETB_8S(p)   *((int8_t *)(p))               // signed 8 bit (1 byte)
#define PDS_GETB_8U(p)   *((uint8_t *)(p))              // unsigned 8 bit (1 byte)
#define PDS_GETB_LE16(p) *((int16_t *)(p))              // 2bytes LE to short
#define PDS_GETB_LEU16(p)*((uint16_t *)(p))             // 2bytes LE to unsigned short
#define PDS_GETB_LE32(p) *((int32_t *)(p))              // 4bytes LE to long
#define PDS_GETB_LEU32(p) *((uint32_t *)(p))            // 4bytes LE to unsigned long
#define PDS_GETB_LE24(p) ((PDS_GETB_LEU32(p))&0x00ffffff)
#define PDS_GETB_LE64(p) *((int64_t *)(p))              // 8bytes LE to int64
#define PDS_GETB_LEU64(p) *((uint64_t *)(p))            // 8bytes LE to uint64
#define PDS_GET4C_LE32(a,b,c,d) ((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
#define PDS_GETS_LE32(p) ((char *)&(p))                 // unsigned long to 4 bytes string

#define PDS_PUTB_8S(p,v)   *((int8_t *)(p))=(v)         //
#define PDS_PUTB_8U(p,v)   *((uint8_t *)(p))=(v)        //
#define PDS_PUTB_LE16(p,v) *((int16_t *)(p))=(v)        //
#define PDS_PUTB_LEU16(p,v) *((uint16_t *)(p))=(v)      //
#define PDS_PUTB_LE24(p,v) *((uint8_t *)(p))=((v)&0xff); PDS_PUTB_LE16(((uint8_t*)p+1),((v)>>8))
#define PDS_PUTB_LE32(p,v) *((int32_t *)(p))=(v)        // long to 4bytes LE
#define PDS_PUTB_LE64(p,v) *((int64_t *)(p))=(v)        // int64 to 8bytes LE

typedef struct xmsmem_t{
 unsigned short remap;
 unsigned short xms;
 unsigned short handle;
 char *physicalptr;
 char *linearptr;
}xmsmem_t;

typedef xmsmem_t cardmem_t;
#define pds_cardmem_physicalptr(cardmem, ptr) ((cardmem)->physicalptr + ((char*)(ptr) - (cardmem)->linearptr))

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
extern void *pds_calloc(unsigned int nitems,unsigned int itemsize);
extern void pds_free(void *bufptr);

//time.c
extern unsigned long pds_gettimeh(void); // clock time in hsec
extern int64_t pds_gettimem(void);  // clock time in msec
extern int64_t pds_gettimeu(void);  // clock time in usec
extern void pds_delay_10us(unsigned int ticks);
extern void pds_mdelay(unsigned long msec);

#ifdef __cplusplus
}
#endif

#endif // mpxplay_newfunc_h
