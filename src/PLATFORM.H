#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef DJGPP

#define FPUSRINLINE 1 /* 1=use the strange djgpp assembly inline feature */

static inline unsigned int BSF(unsigned int x)
{
	unsigned int i;
	asm("bsf %1, %0" : "=r" (i) : "rm" (x));
	return i;
}
static inline unsigned long long rdtsc(void)
{
	unsigned long long tsc;
	asm( "rdtsc" : "=r" (tsc) );
	return tsc;
}
# if FPUSRINLINE 
static inline void fpu_save(void * buffer) { asm( "fsave (%%eax)"::"a"(buffer) ) ; }
static inline void fpu_restore(void * buffer) { asm( "frstor (%%eax)"::"a"(buffer) ) ; }
# else
void fpu_save(void * buffer);
void fpu_restore(void * buffer);
# endif
#define FPU_SRSIZE 108
static inline void _disable_ints(void) { asm("movw $0x900, %%ax\n\t" "int $0x31" ::: "eax" ); }
static inline void  _enable_ints(void) { asm("movw $0x901, %%ax\n\t" "int $0x31" ::: "eax" ); }

#define _my_psp() _go32_info_block.linear_address_of_original_psp
#define _my_rmstack() _go32_info_block.linear_address_of_transfer_buffer
#define _my_rmstksiz() _go32_info_block.size_of_transfer_buffer

#else

extern void (*oldint31)();

unsigned int BSF(unsigned int);
#pragma aux BSF = \
	"bsf eax, edx" \
	parm [edx] \
	modify exact [eax edx]

static  unsigned long long rdtsc(void);
#pragma aux rdtsc = \
	"rdtsc" \
	parm[]  \
	modify exact[eax edx]

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

#define FPU_SRSIZE 108

void _disable_ints(void);
void  _enable_ints(void);

#ifdef NOTFLAT
#pragma aux _disable_ints = \
	"mov ax, 900h" \
	"pushf" \
	"db 66h" \
	"call fword ptr cs:[oldint31]" \
	parm[] \
	modify exact [ax]

#pragma aux _enable_ints = \
	"mov ax, 901h" \
	"pushf" \
	"db 66h" \
	"call fword ptr cs:[oldint31]" \
	parm[] \
	modify exact [ax]
#else
#pragma aux _disable_ints = \
	"mov ax, 900h" \
	"pushfd" \
	"call fword ptr cs:[oldint31]" \
	parm[] \
	modify exact [ax]

#pragma aux _enable_ints = \
	"mov ax, 901h" \
	"pushfd" \
	"call fword ptr cs:[oldint31]" \
	parm[] \
	modify exact [ax]
#endif

unsigned int _my_ds();
#pragma aux _my_ds = \
	"mov eax, ds" \
	parm [] \
	modify exact [ eax ]

#ifdef _DEBUG
unsigned int _my_esp();
#pragma aux _my_esp = \
	"mov eax, esp" \
	parm [] \
	modify exact [ eax ]
#endif

extern unsigned int _linear_psp;
extern unsigned int _linear_rmstack;
//extern unsigned int _size_rmstack;
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
