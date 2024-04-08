#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef DJGPP

static inline unsigned int BSF(unsigned int x)
{
	unsigned int i;
	asm("bsf %1, %0" : "=r" (i) : "rm" (x));
	return i;
}
#define _my_psp() _go32_info_block.linear_address_of_original_psp
#define _my_rmstack() _go32_info_block.linear_address_of_transfer_buffer
#define _my_rmstksiz() _go32_info_block.size_of_transfer_buffer

static inline void fpu_save(void * buffer) {
	asm("fsave %0":: "m"(buffer));
}

static inline void fpu_restore(void * buffer) {
	asm("frstor %0":: "m"(buffer));
}

#else

unsigned int BSF(unsigned int);
#pragma aux BSF = \
	"bsf eax, edx" \
	parm [edx] \
    modify exact [eax edx]

void fpu_save(void *buffer);
#pragma aux fpu_save = \
	"fsave [edx]" \
	parm [edx] \
    modify exact [edx]

void fpu_restore(void *buffer);
#pragma aux fpu_restore = \
	"frstor [edx]" \
	parm [edx] \
    modify exact [edx]

unsigned int _my_ds();
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

#ifdef __cplusplus
}
#endif

#endif
