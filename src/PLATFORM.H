#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#ifdef DJGPP

static inline uint32_t BSF(uint32_t x) {uint32_t i; asm("bsf %1, %0" : "=r" (i) : "rm" (x)); return i;} //386+
#define _my_psp() _go32_info_block.linear_address_of_original_psp
#define _my_rmstack() _go32_info_block.linear_address_of_transfer_buffer
#define _my_rmstksiz() _go32_info_block.size_of_transfer_buffer

#else

extern uint32_t _my_ds();
uint32_t BSF(uint32_t);
#pragma aux BSF = \
	"bsf eax, edx" \
	parm [edx] \
    modify exact [eax edx]
#pragma aux _my_ds = \
	"mov eax, ds" \
	parm [] \
	modify exact [ eax ]
extern unsigned int _linear_psp;
extern unsigned int _linear_rmstack;
extern unsigned int _size_rmstack;
#define _my_psp()      _linear_psp
#define _my_rmstack()  _linear_rmstack
#define _my_rmstksiz() 0x400

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
