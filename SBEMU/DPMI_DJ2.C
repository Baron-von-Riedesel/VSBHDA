
#include "DPMI_.H"
#if defined(__DJ2__)
#include <conio.h>
#include <stdlib.h>
#include <go32.h>
#include <sys/farptr.h>
#include <sys/segments.h>
#include <sys/exceptn.h>
#include <crt0.h>
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

#define dbgprintf

extern DPMI_ADDRESSING DPMI_Addressing;

int _crt0_startup_flags = _CRT0_FLAG_PRESERVE_FILENAME_CASE | _CRT0_FLAG_KEEP_QUOTES;

static uint32_t DPMI_DSBase = 0;
static uint32_t DPMI_DSLimit = 0;
static BOOL DPMI_TSR_Inited = 0;
static uint16_t DPMI_Selector4G;

typedef struct _AddressMap
{
    uint32_t Handle;
    uint32_t LinearAddr;
    uint32_t PhysicalAddr;
    uint32_t Size;
}AddressMap;

#define ADDRMAP_TABLE_SIZE (256 / sizeof(AddressMap))

static AddressMap AddresMapTable[ADDRMAP_TABLE_SIZE];

static void AddAddressMap(const __dpmi_meminfo* info, uint32_t PhysicalAddr)
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
{
    for(int i = 0; i < ADDRMAP_TABLE_SIZE; ++i)
    {
        if(AddresMapTable[i].LinearAddr == linearaddr)
            return i;
    }
    return -1;
}

static void DPMI_Shutdown(void);

#define NEW_IMPL 1

#if NEW_IMPL
//extern uint32_t DPMI_InitTSR(uint32_t base, uint32_t newbase, uint32_t* poffset, uint32_t* psize);
//extern BOOL DPMI_ShutdownTSR();
static uint32_t XMS_Bias;
#else
static __dpmi_meminfo XMS_Info;
#endif

#define ONLY_MSPACES 1
#define NO_MALLOC_STATS 1
#define USE_LOCKS 1
#define LACKS_SCHED_H 1
#define HAVE_MMAP 0
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-conversion"

static void sig_handler(int signal)
{
    dbgprintf("SIGNAL: %x\n", signal);
    exit(-1);   //perform DPMI clean up on atexit
}

static void DPMI_InitFlat()
{
    DPMI_Selector4G = (uint16_t)__dpmi_allocate_ldt_descriptors(1);
    __dpmi_set_segment_base_address(DPMI_Selector4G, 0);
    __dpmi_set_segment_limit(DPMI_Selector4G, 0xFFFFFFFF);
    DPMI_Addressing.selector = DPMI_Selector4G;
    DPMI_Addressing.physical = FALSE;

    __dpmi_get_segment_base_address(_my_ds(), &DPMI_DSBase);
    DPMI_DSLimit = __dpmi_get_segment_limit(_my_ds());
}

void DPMI_Init(void)
{
    atexit(&DPMI_Shutdown);
    signal(SIGINT, sig_handler);
    //signal(SIGABRT, sig_handler);

    DPMI_InitFlat();

    __dpmi_meminfo info;    //1:1 map DOS memory. (0~640K). TODO: get 640K~1M mapping from VCPI
    info.handle = -1;
    info.address = 1024;    //skip IVT and expose NULL ptr
    info.size = 640L*1024L - 1024;
    AddAddressMap(&info, 1024);
}

static void DPMI_Shutdown(void)
{

    dbgprintf("Free mapped space...\n");
    for(int i = 0; i < ADDRMAP_TABLE_SIZE; ++i)
    {
        AddressMap* map = &AddresMapTable[i];
        if(!map->Handle)
            continue;
        if(map->Handle == ~0UL)//XMS mapped
            continue;
        __dpmi_meminfo info;
        info.handle = map->Handle;
        info.address = map->LinearAddr;
        info.size = map->Size;
        __dpmi_free_physical_address_mapping(&info);
    }
    dbgprintf("DPMI_Shutdown done.\n");
}

#if 0
uint32_t DPMI_L2P(uint32_t vaddr)
{
    for(int i = 0; i < ADDRMAP_TABLE_SIZE; ++i)
    {
        AddressMap* map = &AddresMapTable[i];
        if(!map->Handle)
            continue;
        if(map->LinearAddr <= vaddr && vaddr <= map->LinearAddr + map->Size)
        {
            int32_t offset = vaddr - map->LinearAddr;
            return map->PhysicalAddr + offset;
        }
    }
    //dbgprintf("Error mapping linear address to physical: %08lx (%08lx,%08lx).\n", vaddr, DPMI_DSBase, DPMI_DSBase+DPMI_DSLimit);
    //dbgprintf("Exit\n");
    //exit(1);
    return 0; //make compiler happy
}

uint32_t DPMI_P2L(uint32_t paddr)
{
    for(int i = 0; i < ADDRMAP_TABLE_SIZE; ++i)
    {
        AddressMap* map = &AddresMapTable[i];
        if(!map->Handle)
            continue;
        if(map->PhysicalAddr <= paddr && paddr <= map->PhysicalAddr + map->Size)
        {
            int32_t offset = paddr - map->PhysicalAddr;
            return map->LinearAddr + offset;
        }
    }
    //dbgprintf("Error mapping physical address to linear: %08lx.\n", paddr);
    //assert(FALSE);
    //exit(1);
    return 0; //make compiler happy
}
#endif

uint32_t DPMI_PTR2L(void* ptr)
{
    return ptr ? DPMI_DSBase + (uint32_t)ptr : 0;
}

void* DPMI_L2PTR(uint32_t addr)
{
    return addr > DPMI_DSBase ? (void*)(addr - DPMI_DSBase) : NULL;
}


uint32_t DPMI_MapMemory(uint32_t physicaladdr, uint32_t size)
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

uint32_t DPMI_DOSMalloc(uint16_t size)
{
    int selector = 0;
    uint16_t segment = (uint16_t)__dpmi_allocate_dos_memory(size, &selector);
    if(segment != -1)
        return (selector << 16) | segment;
    else
        return 0;
}

void DPMI_DOSFree(uint32_t segment)
{
    __dpmi_free_dos_memory((uint16_t)(segment>>16));
}

uint16_t DPMI_CallRealModeRETF(DPMI_REG* reg)
{
    reg->d._reserved = 0;
    return (uint16_t)__dpmi_simulate_real_mode_procedure_retf((__dpmi_regs*)reg);
}

uint16_t DPMI_CallRealModeINT(uint8_t i, DPMI_REG* reg)
{
    reg->d._reserved = 0;
    return (uint16_t)__dpmi_simulate_real_mode_interrupt(i, (__dpmi_regs*)reg);
}

uint16_t DPMI_CallRealModeIRET(DPMI_REG* reg)
{
    reg->d._reserved = 0;
    return (uint16_t)__dpmi_simulate_real_mode_procedure_iret((__dpmi_regs*)reg);
}

uint16_t DPMI_AllocateRMCB_RETF(void(*Fn)(void), DPMI_REG* reg, __dpmi_raddr *rmcb )
{
    _go32_dpmi_seginfo info;
    info.pm_selector = (uint16_t)_my_cs();
    info.pm_offset = (uintptr_t)Fn;
    if(_go32_dpmi_allocate_real_mode_callback_retf(&info, (_go32_dpmi_registers*)reg) == 0)
    {
        rmcb->offset16 = info.rm_offset;
        rmcb->segment  = info.rm_segment;
        return 1;
    }
    else
        return 0;
}

uint16_t DPMI_FreeRMCB( __dpmi_raddr *rmcb )
{
    if(__dpmi_free_real_mode_callback( rmcb ) == 0)
        return ( 1 );
    else
        return 0;
}


uint8_t DPMI_DisableInterrupt()
{
    return __dpmi_get_and_disable_virtual_interrupt_state();
}

void DPMI_RestoreInterrupt(uint8_t state)
{
    __dpmi_get_and_set_virtual_interrupt_state(state);
}

#endif
