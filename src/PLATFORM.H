#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#ifdef DJGPP
static inline uint32_t BSF(uint32_t x) {uint32_t i; asm("bsf %1, %0" : "=r" (i) : "rm" (x)); return i;} //386+
#define my_psp() _go32_info_block.linear_address_of_original_psp
#else
static inline int my_ds(void) { _asm mov eax, ds; }
extern uint32_t linear_psp;
#define my_psp() linear_psp
#endif

//align down
#define align(x,a) ((uint32_t)((x)+(a)-1)&(uint32_t)(~((a)-1)))

#define countof(x) (sizeof(x)/sizeof(x[0]))

#undef min
#undef max
#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))

#define CPU_CFLAG 0x0001    //carry flag (CF)

#endif
