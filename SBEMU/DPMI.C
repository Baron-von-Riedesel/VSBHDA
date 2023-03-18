
#include "DPMI_.H"
#include <conio.h>
#include <stdlib.h>

DPMI_ADDRESSING DPMI_Addressing;

#define UNMAP_ADDR(addr) (addr)

//#define LOAD_DS() _ASM_BEGIN _ASM(push ds) _ASM(push dword ptr _DPMI_Addressing) _ASM(pop ds) _ASM_END
#define LOAD_DS() _ASM_BEGIN _ASM(push ds) _ASM(mov ds, _DPMI_Addressing) _ASM_END
#define RESTORE_DS() _ASM_BEGIN _ASM(pop ds) _ASM_END


uint8_t DPMI_LoadB(uint32_t addr)
{
    uint8_t ret;
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    ret = *(uint8_t*)addr;
    RESTORE_DS();
    return ret;
}

void DPMI_StoreB(uint32_t addr, uint8_t val)
{
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    *(uint8_t*)addr = val;
    RESTORE_DS();
}

void DPMI_MaskB(uint32_t addr, uint8_t mand, uint8_t mor)
{
    uint8_t val;
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    val = *(uint8_t*)addr;
    val &= mand;
    val |= mor;
    *(uint8_t*)addr = val;
    RESTORE_DS();
}

uint16_t DPMI_LoadW(uint32_t addr)
{
    uint16_t ret;
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    ret = *(uint16_t*)addr;
    RESTORE_DS();
    return ret;
}

void DPMI_StoreW(uint32_t addr, uint16_t val)
{
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    *(uint16_t*)addr = val;
    RESTORE_DS();
}

void DPMI_MaskW(uint32_t addr, uint16_t mand, uint16_t mor)
{
    uint16_t val;
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    val = *(uint16_t*)addr;
    val &= mand;
    val |= mor;
    *(uint16_t*)addr = val;
    RESTORE_DS();
}

uint32_t DPMI_LoadD(uint32_t addr)
{
    uint32_t ret;
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    ret = *(uint32_t*)addr;
    RESTORE_DS();
    return ret;
}

void DPMI_StoreD(uint32_t addr, uint32_t val)
{
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    *(uint32_t*)addr = val;
    RESTORE_DS();
}

void DPMI_MaskD(uint32_t addr, uint32_t mand, uint32_t mor)
{
    uint32_t val;
    addr = UNMAP_ADDR(addr);
    LOAD_DS();
    val = *(uint32_t*)addr;
    val &= mand;
    val |= mor;
    *(uint32_t*)addr = val;
    RESTORE_DS();
}

void DPMI_CopyLinear(uint32_t dest, uint32_t src, uint32_t size)
{

	LOAD_DS();
	asm(
		"push %%esi \n\t"
		"push %%edi \n\t"
		"push %%es \n\t"
		"push %%ds \n\t"
		"pop %%es \n\t"
		"mov %0, %%edi \n\t"
		"mov %1, %%esi \n\t"
		"mov %2, %%ecx \n\t"
		"cld \n\t"
		"rep movsb \n\t"
		"pop %%es \n\t"
		"pop %%edi \n\t"
		"pop %%esi \n\t"
		::"m"(dest),"m"(src),"m"(size)
		:"ecx"
	);
	RESTORE_DS();
}

void DPMI_SetLinear(uint32_t dest, uint8_t val, uint32_t size)
{
    LOAD_DS()
	asm(
		"push %%edi \n\t"
		"push %%es \n\t"
		"push %%ds \n\t"
		"pop %%es \n\t"
		"mov %0, %%edi \n\t"
		"mov %1, %%al \n\t"
		"mov %2, %%ecx \n\t"
		"cld \n\t"
		"rep stosb \n\t"
		"pop %%es \n\t"
		"pop %%edi \n\t"
		::"m"(dest),"m"(val),"m"(size)
		:"ecx", "eax"
	);
    RESTORE_DS()
}

static uint32_t DPMI_DOSUMB(uint32_t input, BOOL alloc, BOOL UMB)
{
    uint32_t result = 0;
    uint16_t UMBlinkstate;
    uint16_t strategy;
    DPMI_REG r = {0};

    //try DOS alloc with UMB link
    r.h.ah = 0x58;
    r.h.al = 0x02;  //get
    DPMI_CallRealModeINT(0x21, &r);
    UMBlinkstate = r.h.al;  //backup state

    r.h.ah = 0x58;
    r.h.al = 0x03;  //set
    r.w.bx = (uint16_t)UMB; //unlink UMB (LH will set UMB need set it back)
    DPMI_CallRealModeINT(0x21, &r);

    r.h.ah = 0x58;
    r.h.al = 0x0;   //get
    DPMI_CallRealModeINT(0x21, &r);
    strategy = r.w.ax; //back strategy

    r.h.ah = 0x58;
    r.h.al = 0x01;  //set
    //http://mirror.cs.msu.ru/oldlinux.org/Linux.old/docs/interrupts/int-html/rb-3008.htm
    r.w.bx = 0x82;  //try hi memory then low memory, last fit. DOS5.0+
    DPMI_CallRealModeINT(0x21, &r);
    if(alloc)
        result = DPMI_DOSMalloc((uint16_t)input);
    else
        DPMI_DOSFree(input);

    r.h.ah = 0x58;
    r.h.al = 0x01;
    r.w.bx = strategy;
    DPMI_CallRealModeINT(0x21, &r); //restore strategy

    r.h.ah = 0x58;
    r.h.al = 0x03;
    r.w.bx = UMBlinkstate;
    DPMI_CallRealModeINT(0x21, &r);
    return result;
}

uint32_t DPMI_HighMalloc(uint16_t size, BOOL UMB)
{
    return DPMI_DOSUMB(size, TRUE, UMB);
}

void DPMI_HighFree(uint32_t segment)
{
    DPMI_DOSUMB(segment, FALSE, TRUE);
}
