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

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t DPMI_PTR2L(void* ptr);/* convert a (near) ptr to linear addr */
void* DPMI_L2PTR(uint32_t addr); /* convert a linear addr to a (near) ptr */

/* map physical memory to linear address range, usually for device IO */
uint32_t DPMI_MapMemory(uint32_t physicaladdr, uint32_t size);

/* free linear address range with mapped physical memory */
BOOL DPMI_UnmapMemory(uint32_t mappedaddr);

//RM call. return 0 on succeed.
uint16_t DPMI_CallRealModeINT(uint8_t i, DPMI_REG* reg);
uint16_t DPMI_CallRealModeRETF(DPMI_REG* reg);
uint16_t DPMI_CallRealModeIRET(DPMI_REG* reg);

//allocate realmode callback. return: hiword: segment, lowword: offset. return 0 if fail
//input: Fn is a common function with normal return. the RMCB will use RETF finally
//uint16_t DPMI_AllocateRMCB_RETF(void(*Fn)(void), DPMI_REG* reg, __dpmi_raddr * );
//uint32_t DPMI_AllocateRMCB_IRET(void(*Fn)(void), DPMI_REG* reg);
uint16_t DPMI_FreeRMCB( __dpmi_raddr * );

//virtual interrupt states
uint8_t DPMI_DisableInterrupt();
void DPMI_RestoreInterrupt(uint8_t state);

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

#ifdef __cplusplus
}
#endif

#endif
