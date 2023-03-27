#ifndef _PLATFORM_H_
#define _PLATFORM_H_

//compiler specific preprossor directive

#if 1 //defined(__DJ2__)

#include <stdint.h>
#include <stddef.h>

#define __NAKED __attribute__((naked))
#define __CDECL __attribute__((cdecl))
#define __INLINE inline

//looks ugly. only if we can work preprocessing with raw string literals (R"()")
//raw string can work with preprocessor using gcc -E or cpp in the traditional way. need a special pass for file with asm
#define _ASM_BEGIN asm __volatile__(".intel_syntax noprefix\n\t" 
#define _ASM_END ".att_syntax noprefix");
#define _ASM(...) #__VA_ARGS__"\n\t"
#define _ASMLBL _ASM
#define _ASM2 _ASM
#define _ASM_OPERAND_SIZE _ASM(.byte 0x66)  //switch operand size, 32bit to 16bit or 16 to 32

#define _ASM_BEGIN16 _ASM_BEGIN ".code16\n\t" 
#define _ASM_END16 ".code32\n\t" _ASM_END
#define _ASM_BEGIN32 _ASM_BEGIN
#define _ASM_END32 _ASM_END

#define NOP() asm __volatile__("nop")
#define CLI() asm __volatile__("cli")
#define STI() asm __volatile__("sti")
static inline uint32_t PLTFM_BSF(uint32_t x) {uint32_t i; asm("bsf %1, %0" : "=r" (i) : "rm" (x)); return i;} //386+
static inline uint16_t PLTFM_CPU_FLAGS_ASM(void) { uint32_t flags = 0; asm("pushf\n\t" "pop %0\n\t" : "=r"(flags)); return (uint16_t)flags; }
static inline uint16_t PLTFM_CPU_FLAGS() { uint16_t (* volatile VFN)(void) = &PLTFM_CPU_FLAGS_ASM; return VFN();} //prevent optimization, need get FLAGS every time

#define memcpy_c2d memcpy

#endif //compiler specific preprossor directive


//general preprossor directive

#ifdef __cplusplus
#define __EXTERN extern "C"
#else
#define __EXTERN extern
#endif

#define nullable
#define outputp
#define inoutp
#define inputp

#define unused(x) (void)x

//align down
#define align(x,a) ((uint32_t)((x)+(a)-1)&(uint32_t)(~((a)-1)))

#define countof(x) (sizeof(x)/sizeof(x[0]))

typedef enum {FALSE, TRUE}BOOLEAN;
typedef int BOOL;

#undef min
#undef max
#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))

static __INLINE uint16_t EndianSwap16(uint16_t x) {return (uint16_t)((x<<8) | (x>>8)); }
static __INLINE uint32_t EndianSwap32(uint32_t x) {return (x<<24) | ((x<<8)&0xFF0000UL) | ((x>>8)&0xFF00UL) | (x>>24); }

#define CPU_CFLAG 0x0001    //carry flag (CF)
#define CPU_IFLAG 0x0200    //interrupt flag (IF)
#define CPU_ZFLAG 0x0040    //zero flag

#define BSF PLTFM_BSF
#define CPU_FLAGS() PLTFM_CPU_FLAGS()

#endif
