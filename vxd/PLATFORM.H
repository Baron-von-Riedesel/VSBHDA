/* PLATFORM.H for VSBVXD (found before src/PLATFORM.H): ring 0 versions of
 * the platform helpers - cli/sti instead of DPMI 0900h/0901h, FPU save
 * with CR0.TS cleared for the time of the use. */

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#ifdef __cplusplus
extern "C"
{
#endif

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

/* the buffer holds the FPU state (108 bytes) and CR0 */
#define FPU_SRSIZE 112
void fpu_save(void *buffer);
#pragma aux fpu_save = \
	"mov eax, cr0" \
	"mov [edx+108], eax" \
	"clts" \
	"fnsave [edx]" \
	"fninit" \
	parm [edx] \
	modify exact [eax]
void fpu_restore(void *buffer);
#pragma aux fpu_restore = \
	"frstor [edx]" \
	"mov eax, [edx+108]" \
	"mov cr0, eax" \
	parm [edx] \
	modify exact [eax]

void _disable_ints(void);
#pragma aux _disable_ints = "cli" parm[] modify exact []
void _enable_ints(void);
#pragma aux _enable_ints = "sti" parm[] modify exact []

#define align(x,a) ((uint32_t)((x)+(a)-1)&(uint32_t)(~((a)-1)))
#define countof(x) (sizeof(x)/sizeof(x[0]))
#undef min
#undef max
#define min(x,y) ((x)<(y)?(x):(y))
#define max(x,y) ((x)>(y)?(x):(y))

#define CPU_CFLAG 0x0001

#ifdef __cplusplus
}
#endif

#endif
