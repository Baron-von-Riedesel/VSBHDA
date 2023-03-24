
#include <conio.h>
#include <stdlib.h>
#include <go32.h>
#include <sys/farptr.h>
#include <sys/segments.h>
#include <sys/exceptn.h>
#include <crt0.h>
#include <assert.h>
//#include <signal.h>
//#include <stdio.h>
#include <string.h>
#include "DPMI_.H"

int _crt0_startup_flags = _CRT0_FLAG_PRESERVE_FILENAME_CASE | _CRT0_FLAG_KEEP_QUOTES;

static uint32_t DPMI_DSBase = 0;
//static uint32_t DPMI_DSLimit = 0;
static uint16_t DPMI_Selector4G;

typedef struct _AddressMap
{
    uint32_t Handle;
    uint32_t LinearAddr;
    uint32_t PhysicalAddr;
    uint32_t Size;
}AddressMap;

#define ADDRMAP_TABLE_SIZE (64 / sizeof(AddressMap))

static AddressMap AddresMapTable[ADDRMAP_TABLE_SIZE];

typedef struct
{
    uint16_t selector; //selector created from outside
    uint16_t physical; //1: physical addr instead of virtual. used when not called through DPMI/DPMI, and context unavailable, and no paging
} DPMI_ADDRESSING;

static DPMI_ADDRESSING DPMI_Addressing;

#define LOAD_DS() asm("push %%ds\n\t" "mov %0, %%ds" : : "m"(DPMI_Addressing.selector) );
#define RESTORE_DS() asm("pop %ds");

uint8_t DPMI_LoadB(uint32_t addr)
/////////////////////////////////
{
    uint8_t ret;
    LOAD_DS();
    ret = *(uint8_t*)addr;
    RESTORE_DS();
    return ret;
}

void DPMI_StoreB(uint32_t addr, uint8_t val)
////////////////////////////////////////////
{
    LOAD_DS();
    *(uint8_t*)addr = val;
    RESTORE_DS();
}

void DPMI_MaskB(uint32_t addr, uint8_t mand, uint8_t mor)
/////////////////////////////////////////////////////////
{
    uint8_t val;
    LOAD_DS();
    val = *(uint8_t*)addr;
    val &= mand;
    val |= mor;
    *(uint8_t*)addr = val;
    RESTORE_DS();
}

uint16_t DPMI_LoadW(uint32_t addr)
//////////////////////////////////
{
    uint16_t ret;
    LOAD_DS();
    ret = *(uint16_t*)addr;
    RESTORE_DS();
    return ret;
}

void DPMI_StoreW(uint32_t addr, uint16_t val)
/////////////////////////////////////////////
{
    LOAD_DS();
    *(uint16_t*)addr = val;
    RESTORE_DS();
}

void DPMI_MaskW(uint32_t addr, uint16_t mand, uint16_t mor)
///////////////////////////////////////////////////////////
{
    uint16_t val;
    LOAD_DS();
    val = *(uint16_t*)addr;
    val &= mand;
    val |= mor;
    *(uint16_t*)addr = val;
    RESTORE_DS();
}

uint32_t DPMI_LoadD(uint32_t addr)
//////////////////////////////////
{
    uint32_t ret;
    LOAD_DS();
    ret = *(uint32_t*)addr;
    RESTORE_DS();
    return ret;
}

void DPMI_StoreD(uint32_t addr, uint32_t val)
/////////////////////////////////////////////
{
    LOAD_DS();
    *(uint32_t*)addr = val;
    RESTORE_DS();
}

void DPMI_MaskD(uint32_t addr, uint32_t mand, uint32_t mor)
///////////////////////////////////////////////////////////
{
    uint32_t val;
    LOAD_DS();
    val = *(uint32_t*)addr;
    val &= mand;
    val |= mor;
    *(uint32_t*)addr = val;
    RESTORE_DS();
}

void DPMI_CopyLinear(uint32_t dest, uint32_t src, uint32_t size)
////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////
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

static void AddAddressMap(const __dpmi_meminfo* info, uint32_t PhysicalAddr)
////////////////////////////////////////////////////////////////////////////
{
    for(int i = 0; i < ADDRMAP_TABLE_SIZE; ++i)
    {
        if(AddresMapTable[i].Handle == 0)
        {
            AddressMap* map = &AddresMapTable[i];
            map->Handle = info->handle;
            map->LinearAddr = info->address;
            map->PhysicalAddr = PhysicalAddr;
            map->Size = info->size;
        }
    }
}

static int FindAddressMap(uint32_t linearaddr)
//////////////////////////////////////////////
{
    for(int i = 0; i < ADDRMAP_TABLE_SIZE; ++i)
    {
        if(AddresMapTable[i].LinearAddr == linearaddr)
            return i;
    }
    return -1;
}

#if 0
static void sig_handler(int signal)
{
    dbgprintf("SIGNAL: %x\n", signal);
    exit(-1);   //perform DPMI clean up on atexit
}
#endif

void DPMI_Init(void)
////////////////////
{
    //signal(SIGINT, sig_handler);
    //signal(SIGABRT, sig_handler);

    DPMI_Selector4G = (uint16_t)__dpmi_allocate_ldt_descriptors(1);
    __dpmi_set_segment_base_address(DPMI_Selector4G, 0);
    __dpmi_set_segment_limit(DPMI_Selector4G, 0xFFFFFFFF);
    DPMI_Addressing.selector = DPMI_Selector4G;
    DPMI_Addressing.physical = FALSE;

    __dpmi_get_segment_base_address(_my_ds(), &DPMI_DSBase);
    //DPMI_DSLimit = __dpmi_get_segment_limit(_my_ds());
}

uint32_t DPMI_PTR2L(void* ptr)
//////////////////////////////
{
    return ptr ? DPMI_DSBase + (uint32_t)ptr : 0;
}

void* DPMI_L2PTR(uint32_t addr)
///////////////////////////////
{
    return addr > DPMI_DSBase ? (void*)(addr - DPMI_DSBase) : NULL;
}


uint32_t DPMI_MapMemory(uint32_t physicaladdr, uint32_t size)
/////////////////////////////////////////////////////////////
{
    __dpmi_meminfo info;
    info.address = physicaladdr;
    info.size = size;
    if( __dpmi_physical_address_mapping(&info) != -1)
    {
        AddAddressMap(&info, physicaladdr);
        return info.address;
    }
    return 0;
}

BOOL DPMI_UnmapMemory(uint32_t mappedaddr)
//////////////////////////////////////////
{
    int index = FindAddressMap(mappedaddr);
    if(index == -1)
        return FALSE;
    AddressMap* map = &AddresMapTable[index];
    if(map->Handle == 0 || map->Handle == ~0x0UL)
        return FALSE;
    __dpmi_meminfo info;
    info.handle = map->Handle;
    info.address = map->LinearAddr;
    info.size = map->Size;
    __dpmi_free_physical_address_mapping(&info);
    memset(map, 0, sizeof(*map));
    return TRUE;
}

uint16_t DPMI_CallRealModeRETF(DPMI_REG* reg)
/////////////////////////////////////////////
{
    reg->d._reserved = 0;
    return (uint16_t)__dpmi_simulate_real_mode_procedure_retf((__dpmi_regs*)reg);
}

uint16_t DPMI_CallRealModeINT(uint8_t i, DPMI_REG* reg)
///////////////////////////////////////////////////////
{
    reg->d._reserved = 0;
    return (uint16_t)__dpmi_simulate_real_mode_interrupt(i, (__dpmi_regs*)reg);
}

uint16_t DPMI_CallRealModeIRET(DPMI_REG* reg)
/////////////////////////////////////////////
{
    reg->d._reserved = 0;
    return (uint16_t)__dpmi_simulate_real_mode_procedure_iret((__dpmi_regs*)reg);
}

uint16_t DPMI_FreeRMCB( __dpmi_raddr *rmcb )
////////////////////////////////////////////
{
    if(__dpmi_free_real_mode_callback( rmcb ) == 0)
        return ( 1 );
    else
        return 0;
}

#if 0
uint8_t DPMI_DisableInterrupt()
///////////////////////////////
{
    return __dpmi_get_and_disable_virtual_interrupt_state();
}

void DPMI_RestoreInterrupt(uint8_t state)
/////////////////////////////////////////
{
    __dpmi_get_and_set_virtual_interrupt_state(state);
}
#endif
