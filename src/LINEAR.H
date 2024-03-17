#ifndef _LINEAR_H_
#define _LINEAR_H_

//#include <dpmi.h>
#include "DJDPMI.H"

#ifdef __cplusplus
extern "C"
{
#endif

//linear memory access

extern uint32_t DSBase;

#if defined(DJGPP) || defined(NOTFLAT)
static inline uint32_t LinearAddr( void* ptr ) { return DSBase + (uint32_t)ptr; }
static inline void    *NearPtr( uint32_t linaddr ) { return (void *)(linaddr - DSBase); }
static inline uint16_t ReadLinearW(uint32_t addr) { return *(uint16_t*)NearPtr(addr); }
static inline void     WriteLinearW(uint32_t addr, uint16_t val) { *(uint16_t*)NearPtr(addr) = val; }
static inline uint32_t ReadLinearD(uint32_t addr) { return *(uint32_t*)NearPtr(addr); }
static inline void     WriteLinearD(uint32_t addr, uint32_t val) { *(uint32_t*)NearPtr(addr) = val; }
#else
static inline uint32_t LinearAddr( void* ptr ) { return (uint32_t)ptr; }
static inline void    *NearPtr( uint32_t linaddr ) { return (void *)linaddr; }
static inline uint16_t ReadLinearW(uint32_t addr) { return *(uint16_t*)NearPtr(addr); }
static inline void     WriteLinearW(uint32_t addr, uint16_t val) { *(uint16_t*)NearPtr(addr) = val; }
static inline uint32_t ReadLinearD(uint32_t addr) { return *(uint32_t*)NearPtr(addr); }
static inline void     WriteLinearD(uint32_t addr, uint32_t val) { *(uint32_t*)NearPtr(addr) = val; }
#endif

#ifdef __cplusplus
}
#endif

#endif
