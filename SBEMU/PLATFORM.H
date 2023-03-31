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

#define memcpy_c2d memcpy

static inline uint32_t BSF(uint32_t x) {uint32_t i; asm("bsf %1, %0" : "=r" (i) : "rm" (x)); return i;} //386+

#endif //compiler specific preprossor directive


//align down
#define align(x,a) ((uint32_t)((x)+(a)-1)&(uint32_t)(~((a)-1)))

#define countof(x) (sizeof(x)/sizeof(x[0]))

typedef int BOOL;

#undef min
#undef max
#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))

#define CPU_CFLAG 0x0001    //carry flag (CF)

#endif
