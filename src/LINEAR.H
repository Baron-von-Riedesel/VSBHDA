#ifndef _LINEAR_H_
#define _LINEAR_H_

#include <dpmi.h>

#ifdef __cplusplus
extern "C"
{
#endif

//#define _disable_ints() asm("mov $0x900, %%ax\n\t" "int $0x31" ::: "eax" );
static inline void _disable_ints(void) { asm("mov $0x900, %%ax\n\t" "int $0x31" ::: "eax" ); }
//#define _enable_ints() asm("mov $0x901, %%ax\n\t" "int $0x31" ::: "eax" );
static inline void _enable_ints(void) { asm("mov $0x901, %%ax\n\t" "int $0x31" ::: "eax" ); }

//linear memory access

extern uint32_t DSBase;

static inline uint32_t LinearAddr( void* ptr ) { return DSBase + (uint32_t)ptr; }
static inline void    *NearPtr( uint32_t linaddr ) { return (void *)(linaddr - DSBase); }
static inline uint16_t ReadLinearW(uint32_t addr) { return *(uint16_t*)NearPtr(addr); }
static inline void     WriteLinearW(uint32_t addr, uint16_t val) { *(uint16_t*)NearPtr(addr) = val; }
static inline uint32_t ReadLinearD(uint32_t addr) { return *(uint32_t*)NearPtr(addr); }
static inline void     WriteLinearD(uint32_t addr, uint32_t val) { *(uint32_t*)NearPtr(addr) = val; }

#ifdef __cplusplus
}
#endif

#endif
