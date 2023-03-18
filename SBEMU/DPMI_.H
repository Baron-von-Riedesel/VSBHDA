#ifndef _DPMI_H_
#define _DPMI_H_
#include "PLATFORM.H"
#include <dpmi.h>

//a DPMI wrapper
//the purpose of this file is to hide all __dpmi* or int 31h calls from outside

typedef union {
struct
{
    uint16_t di, hdi;
    uint16_t si, hsi;
    uint16_t bp, hbp;
    uint16_t _reservedl, _reservedh;
    uint16_t bx, hbx;
    uint16_t dx, hdx;
    uint16_t cx, hcx;
    uint16_t ax, hax;
    uint16_t flags;
    uint16_t es;
    uint16_t ds;
    uint16_t fs;
    uint16_t gs;
    uint16_t ip;
    uint16_t cs;
    uint16_t sp;
    uint16_t ss;
}w; //BC doesn't support unamed struct.

struct
{
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t _reserved;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
}d;

struct
{
    uint8_t edi4b[4];
    uint8_t esi4b[4];
    uint8_t ebp4b[4];
    uint8_t _reserved4b[4];
    uint8_t bl, bh, eb2, eb3;
    uint8_t dl, dh, ed2, ed3;
    uint8_t cl, ch, ec2, ec3;
    uint8_t al, ah, ea2, ea3;
}h;
}DPMI_REG;

typedef struct
{
    uint16_t selector; //selector created from outside
    uint16_t physical; //1: physical addr instead of virtual. used when not called through DPMI/DPMI, and context unavailable, and no paging
} DPMI_ADDRESSING;

#ifdef __cplusplus
extern "C"
{
#endif

//note: DJGPP use paged flat mode linear address.

//convert a linear (virtual) addr to physical addr
//only working with memory allocated with DPMI_MapMemory
//uint32_t DPMI_L2P(uint32_t vaddr);

//convert a physical addr to linear addr
//only working with memory allocated with DPMI_MapMemory
//uint32_t DPMI_P2L(uint32_t paddr);

//convert a ptr to linear (page mapped) addr
uint32_t DPMI_PTR2L(void* ptr);

void* DPMI_L2PTR(uint32_t addr);

//convert ptr to physical addr
//#define DPMI_PTR2P(ptr) DPMI_L2P(DPMI_PTR2L(ptr))
//convert physical addr to ptr
//#define DPMI_P2PTR(addr) DPMI_L2PTR(DPMI_P2L(addr))

//return virtual mapped memory in linear space, usually for device IO
//get the device base address in physical space and map it to the linear space to access it
//input physical addr must < 1M (0x100000)
//http://www.delorie.com/djgpp/v2faq/faq18_7.html
uint32_t DPMI_MapMemory(uint32_t physicaladdr, uint32_t size);
BOOL DPMI_UnmapMemory(uint32_t mappedaddr);

//allocate DOS conventional memory (below 640K)
//input: size in paragraphs (16 bytes)
//return: high 16: selector, low 16: segment base address.
//allocated memory is 1:1 mapped (physical==linear)
uint32_t DPMI_DOSMalloc(uint16_t size);
void DPMI_DOSFree(uint32_t segment);

//RM call. return 0 on succeed.
uint16_t DPMI_CallRealModeINT(uint8_t i, DPMI_REG* reg);
uint16_t DPMI_CallRealModeRETF(DPMI_REG* reg);
uint16_t DPMI_CallRealModeIRET(DPMI_REG* reg);
//install interrupt service routine. ISR use normal return, no need to do IRET.
//return 0 if succeed

//allocate realmode callback. return: hiword: segment, lowword: offset. return 0 if fail
//input: Fn is a common function with normal return. the RMCB will use RETF finally
uint16_t DPMI_AllocateRMCB_RETF(void(*Fn)(void), DPMI_REG* reg, __dpmi_raddr * );
uint32_t DPMI_AllocateRMCB_IRET(void(*Fn)(void), DPMI_REG* reg);
uint16_t DPMI_FreeRMCB( __dpmi_raddr * );

//virtual interrupt states
uint8_t DPMI_DisableInterrupt();
void DPMI_RestoreInterrupt(uint8_t state);

//switch addressing mode for linear functions, should use it temporarily and interrupt disabled.
//void DPMI_SetAddressing(DPMI_ADDRESSING* inputp newaddr, DPMI_ADDRESSING* outputp oldaddr);

void DPMI_Init(void);

//linear memory access
uint8_t DPMI_LoadB(uint32_t addr);
void DPMI_StoreB(uint32_t addr, uint8_t val);
void DPMI_MaskB(uint32_t addr, uint8_t mand, uint8_t mor);

uint16_t DPMI_LoadW(uint32_t addr);
void DPMI_StoreW(uint32_t addr, uint16_t val);
void DPMI_MaskW(uint32_t addr, uint16_t mand, uint16_t mor);

uint32_t DPMI_LoadD(uint32_t addr);
void DPMI_StoreD(uint32_t addr, uint32_t val);
void DPMI_MaskD(uint32_t addr, uint32_t mand, uint32_t mor);

void DPMI_CopyLinear(uint32_t dest, uint32_t src, uint32_t size);
void DPMI_SetLinear(uint32_t dest, uint8_t val, uint32_t size);

//topdown allocation of DPMI_DOSMalloc, from high address (640K/1024K downwards).
//input: size in paragraphs (16 bytes)
//UMB: use UMB memory (640K~1024K), memory may not be 1:1 mapped.
//output: segment address in low 16 bit.
//note: only conventional 640K is 1:1 mapped.
//UMB returned addr not essentially equals to physical addr
//do not use UMB if physical addr (DPMI_L2P) is needed.
uint32_t DPMI_HighMalloc(uint16_t size, BOOL UMB);
void DPMI_HighFree(uint32_t segment);

//convert real mode far pointer to linear addr
//#define DPMI_FP2L(f32) ((((f32)>>12)&0xFFFF0)+((f32)&0xFFFF))
//convert far pointer (seg:off) to linear
//#define DPMI_SEGOFF2L(seg, off) ((((uint32_t)((seg)&0xFFFF))<<4) + ((off)&0xFFFF))
//convert far pointer (seg:off) to 32bit far ptr
//#define DPMI_MKFP(seg, off) ((((uint32_t)((seg)&0xFFFF))<<16) | ((off)&0xFFFF))

#ifdef __cplusplus
}
#endif

#endif
