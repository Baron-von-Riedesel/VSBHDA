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

#if defined(WIN32) || defined(__WINDOWS_386__) || defined(__NT__) || defined(_WIN32) || defined(_WIN64)
#define MPXPLAY_WIN32 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MPXPLAY_UTF8
#define MAX_PATHNAMELEN 300*3
#define MAX_PATHNAMEU08 MAX_PATHNAMELEN // utf8
#define MAX_PATHNAMEU16 300             // utf16
#else
#define MAX_PATHNAMELEN 300
#define MAX_PATHNAMEU08 MAX_PATHNAMELEN*3
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

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
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
/*void pds_ftoi(mpxp_double_t,mpxp_int32_t *); // rather use -zro option at wcc386
 #pragma aux pds_ftoi parm [8087][esi] = "fistp dword ptr [esi]"
 void pds_fto64i(mpxp_double_t,mpxp_int64_t *);
 #pragma aux pds_fto64i parm [8087][esi] = "fistp qword ptr [esi]"*/
#define pds_ftoi(ff,ii)   (*(ii)=(mpxp_int32_t)(ff))
#define pds_fto64i(ff,ii) (*(ii)=(mpxp_int64_t)(ff))

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

#define pds_newfunc_regp_clear(regp) pds_memset(regp,0,sizeof(union REGPACK))
#define pds_newfunc_regs_clear(regs) pds_memset(regs,0,sizeof(union REGS))
#define pds_newfunc_sregs_clear(sregs) pds_memset(sregs,0,sizeof(struct SREGS))

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

typedef struct pds_fdate_t{
 unsigned short twosecs : 5;
 unsigned short minutes : 6;
 unsigned short hours   : 5;
 unsigned short day     : 5;
 unsigned short month   : 4;
 unsigned short year    : 7;
}pds_fdate_t;

//bitstrm.c
typedef struct mpxplay_bitstreambuf_s{
 unsigned long bitpos;     // we assume that we never store more than 4Gbits
 unsigned long storedbits; // detto
 unsigned char *buffer;    // begin of buffer
 unsigned long bufsize;
}mpxplay_bitstreambuf_s;

extern struct mpxplay_bitstreambuf_s *mpxplay_bitstream_alloc(unsigned int required_bufsize);
extern void mpxplay_bitstream_free(struct mpxplay_bitstreambuf_s *bs);
extern void mpxplay_bitstream_init(struct mpxplay_bitstreambuf_s *bs,unsigned char *data,unsigned int bytes);
extern void mpxplay_bitstream_consolidate(struct mpxplay_bitstreambuf_s *bs,unsigned int do_bytealign);
//extern int  mpxplay_bitstream_fill(struct mpxplay_bitstreambuf_s *bs,struct mpxplay_filehand_buffered_func_s *fbfs,void *fbds,unsigned int needbytes); // fill with a file-read
extern unsigned int mpxplay_bitstream_putbytes(struct mpxplay_bitstreambuf_s *bs,unsigned char *srcbuf,unsigned int newbytes); // fill with (new) bytes from a buffer
extern void mpxplay_bitstream_reset(struct mpxplay_bitstreambuf_s *bs);

extern unsigned char *mpxplay_bitstream_getbufpos(struct mpxplay_bitstreambuf_s *bs);
extern int  mpxplay_bitstream_lookbytes(struct mpxplay_bitstreambuf_s *bs,unsigned char *destbuf,unsigned int needbytes);
extern int  mpxplay_bitstream_readbytes(struct mpxplay_bitstreambuf_s *bs,unsigned char *destbuf,unsigned int needbytes);
extern int  mpxplay_bitstream_skipbytes(struct mpxplay_bitstreambuf_s *bs,int skipbytes);
extern long mpxplay_bitstream_leftbytes(struct mpxplay_bitstreambuf_s *bs);

extern mpxp_uint32_t mpxplay_bitstream_get_byte(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint32_t mpxplay_bitstream_get_le16(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint32_t mpxplay_bitstream_get_le32(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint64_t mpxplay_bitstream_get_le64(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint32_t mpxplay_bitstream_get_be16(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint32_t mpxplay_bitstream_get_be32(struct mpxplay_bitstreambuf_s *bs);
extern mpxp_uint64_t mpxplay_bitstream_get_be64(struct mpxplay_bitstreambuf_s *bs);

extern int  mpxplay_bitstream_getbit1_be(struct mpxplay_bitstreambuf_s *bs);
extern long mpxplay_bitstream_getbits_be24(struct mpxplay_bitstreambuf_s *bs,unsigned int bits);
extern mpxp_uint32_t mpxplay_bitstream_getbits_ube32(struct mpxplay_bitstreambuf_s *bs,unsigned int bits);
extern mpxp_int64_t mpxplay_bitstream_getbits_be64(struct mpxplay_bitstreambuf_s *bs,unsigned int bits);
extern int mpxplay_bitstream_getbit1_le(struct mpxplay_bitstreambuf_s *bs);
extern long mpxplay_bitstream_getbits_le24(struct mpxplay_bitstreambuf_s *bs,unsigned int bits);
extern mpxp_int64_t mpxplay_bitstream_getbits_le64(struct mpxplay_bitstreambuf_s *bs,unsigned int bits);
extern int  mpxplay_bitstream_skipbits(struct mpxplay_bitstreambuf_s *bs,int bits); // bits can be negative, return MPXPLAY_ERROR_MPXINBUF_nnn
extern long mpxplay_bitstream_leftbits(struct mpxplay_bitstreambuf_s *bs);

//cpu.c
#define CPU_X86_STDCAP_MSR     (1<< 5) // Model-Specific Registers, RDMSR, WRMSR
#define CPU_X86_STDCAP_MTRR    (1<<12) // Memory Type Range Registers
#define CPU_X86_STDCAP_PGE     (1<<13) // Page Global Enable
#define CPU_X86_STDCAP_PAT     (1<<16) // Page Attribute Table
#define CPU_X86_STDCAP_MMX     (1<<23)
#define CPU_X86_STDCAP_SSE1    (1<<25)
#define CPU_X86_STDCAP_SSE2    (1<<26)
#define CPU_X86_EXTCAP_MMXEXT  (1<<23) // ??? 22 or 23
#define CPU_X86_EXTCAP_3DNOW2  (1<<30)
#define CPU_X86_EXTCAP_3DNOW1  (1<<31)
#define CPU_X86_MMCAP_MMX      0x0001 // standard MMX
#define CPU_X86_MMCAP_MMXEXT   0x0002 // SSE integer functions or AMD MMX ext
#define CPU_X86_MMCAP_3DNOW    0x0004 // AMD 3DNOW
#define CPU_X86_MMCAP_SSE      0x0008 // SSE functions
#define CPU_X86_MMCAP_SSE2     0x0010 // PIV SSE2 functions
#define CPU_X86_MMCAP_3DNOWEXT 0x0020 // AMD 3DNowExt

extern int mpxplay_cpu_capables_std,mpxplay_cpu_capables_ext,mpxplay_cpu_capables_mm;
extern void newfunc_cpu_init(void);
extern unsigned int pds_cpu_cpuid_test(void);
extern int pds_cpu_cpuid_get(unsigned int eax_p,int *ebx_p,int *ecx_p,int *edx_p);
extern unsigned int pds_cpu_mtrr_enable_wc(unsigned long phys_ptr,unsigned long size_kb);
extern void pds_cpu_mtrr_disable_wc(unsigned long phys_ptr);
extern void pds_fpu_setround_near(void);
extern void pds_fpu_setround_chop(void);

//dpmi.c
#ifndef MPXPLAY_WIN32

#if defined(DJGPP)
#include <sys/segments.h>
#define far
#define __far
#define __interrupt
#define __loadds
typedef struct _farptr {unsigned long off;unsigned short sel;} farptr;//DJGPP has no 16:32 flat model far ptr.
typedef farptr int_handler_t;
static farptr pds_fardata(void* d) { farptr ptr = {(long)d, _my_ds()}; return ptr;}
static void pds_call_int_handler(int_handler_t h) { asm("pushf\n\t lcall *%0" ::"m"(h));} //simulate a iret frame for handler
static int_handler_t pds_int_handler(void (*f)(void)) { int_handler_t h = {(long)f, _my_cs()}; return h;}
static int pds_valid_int_handler(int_handler_t h) {return h.off && h.sel;}
#else
typedef void* far farptr;
typedef void (__far __interrupt *int_handler_t)();
#define pds_fardata(d) (d)
static void pds_call_int_handler(int_handler_t h) {h();}
#define pds_int_handler(f) (f)
#define pds_valid_int_handler(h) (h)
#endif//DJGPP

#define PDS_INT2X_DOSMEM_SIZE  512
extern long pds_dpmi_segment_to_selector(unsigned int segment);
extern void far *pds_dpmi_getrmvect(unsigned int intno);
extern void pds_dpmi_setrmvect(unsigned int intno, unsigned int segment,unsigned int offset);
#if defined(DJGPP)
extern farptr pds_dpmi_getexcvect(unsigned int intno);
extern void pds_dpmi_setexcvect(unsigned int intno, farptr vect);

extern int  pds_dpmi_xms_allocmem(xmsmem_t *,unsigned int size);
extern void pds_dpmi_xms_freemem(xmsmem_t *);
#else
extern void far *pds_dpmi_getexcvect(unsigned int intno);
extern void pds_dpmi_setexcvect(unsigned int intno, void far *vect);
#endif
extern int  pds_dpmi_dos_allocmem(dosmem_t *,unsigned int size);
extern void pds_dpmi_dos_freemem(dosmem_t *);
extern void pds_dpmi_realmodeint_call(unsigned int intnum,struct rminfo *rmi);
extern unsigned long pds_dpmi_map_physical_memory(unsigned long phys_addr,unsigned long memsize);
extern void pds_dpmi_unmap_physical_memory(unsigned long linear_address);
#define pds_dpmi_rmi_clear(rmi) pds_memset(rmi,0,sizeof(struct rminfo))
#endif // MPXPLAY_WIN32

extern unsigned int pds_int2x_dosmems_allocate(void);
extern void pds_int2x_dosmems_free(void);

//errorhnd.c
extern void newfunc_errorhnd_int24_init(void);
extern void newfunc_error_handlers_init(void);
extern void newfunc_error_handlers_close(void);
extern void newfunc_exception_handlers_close(void);
//extern void pds_mswin_previousinstance_close(void);

//memory.c
extern void newfunc_memory_init(void);
#include <string.h>
#define pds_memset(t,v,l)   memset(t,v,l)
#define pds_memcpy(t,s,l)   memcpy(t,s,l)
extern void *pds_malloc(unsigned int bufsize);
extern void *pds_calloc(unsigned int nitems,unsigned int itemsize);
extern void *pds_realloc(void *bufptr,unsigned int bufsize);
extern void pds_free(void *bufptr);

//string.c
extern unsigned int pds_strcpy(char *,char *); // returns the length of string!
extern unsigned int pds_strmove(char *dest,char *src); // returns the length of string
extern unsigned int pds_strncpy(char *dest,char *src,unsigned int maxlen); // returns the length of string!
extern unsigned int pds_strcat(char *,char *); // returns the lenght of string!
extern int  pds_strcmp(char *strp1,char *strp2);
extern int  pds_stricmp(char *,char *);
extern unsigned int pds_stri_compare(char *strp1,char *strp2); // returns 1 if equal, else 0 (no pointer check!)
extern int  pds_strricmp(char *,char *);
extern int  pds_strlicmp(char *,char *);
extern int  pds_strncmp(char *,char *,unsigned int);
extern int  pds_strnicmp(char *strp1,char *strp2,unsigned int counter);
extern unsigned int pds_strlen(char *strp);
extern unsigned int pds_strlenc(char *strp,char seek);
//extern unsigned int pds_strlencn(char *strp,char seek,unsigned int maxlen);
extern char *pds_strchr(char *,char);
extern char *pds_strrchr(char *,char);
extern char *pds_strnchr(char *strp,char seek,unsigned int len);
extern char *pds_strstr(char *s1,char *s2);
extern char *pds_strstri(char *s1,char *s2);
extern unsigned int pds_strcutspc(char *src); // returns (new)len
extern void pds_str_url_decode(char *src);
extern unsigned int pds_str_clean(char *str); // returns (new)len
extern void pds_str_conv_forbidden_chars(char *str,char *fromchars,char *tochars);
extern unsigned int pds_str_extendc(char *str,unsigned int newlen,char c); // returns len
extern unsigned int pds_str_fixlenc(char *str,unsigned int newlen,char c); // returns (new)len
extern int pds_str_limitc(char *src, char *dest, unsigned int newlen, char limit_c); // returns (new)len
extern char *pds_str_getwordn(char *str, unsigned int wordcount); // returns pointer
extern unsigned int pds_chkstr_letters(char *str);
extern unsigned int pds_chkstr_uppercase(char *str);
extern void pds_str_uppercase(char *str);
extern void pds_str_lowercase(char *str);
extern unsigned int pds_log10(long value);
extern void  pds_ltoa(int,char *);
//extern void  pds_ltoa16(int,char *);
extern long  pds_atol(char *);
extern mpxp_int64_t pds_atoi64(char *);
extern long  pds_atol16(char *);
extern void pds_str_to_hexs(char *src,char *dest,unsigned int destlen);
extern void pds_hexs_to_str(char *src,char *dest,unsigned int destlen);

#define pds_cvchar_CP437_to_UTF16LE(c) (c)
#define pds_strlen_mpxnative(strp) pds_strlen(strp)
#define pds_strpos_mpxnative(strp,pos) ((pds_strlen(strp) >= pos)? (pds_strlen(strp) - 1) : pos)  // FIXME: a faster way?

#define pds_textdisplay_printf(text) { fprintf(stdout, "%s", (text)); fprintf(stdout, "\n"); fflush(stdout); }

//time.c
extern unsigned long pds_gettimeh(void); // clock time in hsec
extern mpxp_int64_t pds_gettimem(void);  // clock time in msec
extern mpxp_int64_t pds_gettimeu(void);  // clock time in usec
extern void pds_delay_10us(unsigned int ticks);
extern void pds_mdelay(unsigned long msec);

//timer.c
extern unsigned long mpxplay_timer_secs_to_counternum(unsigned long secs);
extern unsigned long mpxplay_timer_msecs_to_counternum(unsigned long msecs);

// safe funcbit handling for multi processors
#define funcbit_smp_test(var,bit)       funcbit_test(var,bit)
#define funcbit_smp_enable(var,bit)     funcbit_enable(var,bit)
#define funcbit_smp_disable(var,bit)    funcbit_disable(var,bit)
#define funcbit_smp_inverse(var,bit)    funcbit_inverse(var,bit)
#define funcbit_smp_copy(var1,var2,bit) funcbit_copy(var1,var2,bit)

#define funcbit_smp_int32_get(var)       (var)
#define funcbit_smp_int32_put(var,val)   (var)=(val)

#define mpxplay_debugf(...)

#ifdef __cplusplus
}
#endif

#endif // mpxplay_newfunc_h
