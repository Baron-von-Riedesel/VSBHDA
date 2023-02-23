#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef __GNUC__ //make vscode happy
#define __attribute__(x)
#endif
#include <DPMI/DBGUTIL.H>
#include <UNTRAPIO.H>

#include "HDPMIPT.H"

#define HDPMIPT_SWITCH_STACK 1 //TODO: debug
#define HDPMIPT_STACKSIZE 16384

BOOL _hdpmi_InstallISR(uint8_t i, void(*ISR)(void), DPMI_ISR_HANDLE* outputp handle, void* pStack );
BOOL _hdpmi_UninstallISR( DPMI_ISR_HANDLE* inputp handle );
BOOL _hdpmi_InstallInt31( DPMI_ISR_HANDLE* inputp handle );
BOOL _hdpmi_UninstallInt31( void );

typedef struct
{
    uint32_t edi;
    uint16_t es;
}HDPMIPT_ENTRY;

extern uint32_t __djgpp_stack_top;

static const char* VENDOR_HDPMI = "HDPMI";    //vendor string
static HDPMIPT_ENTRY HDPMIPT_Entry;
/* stored DS segment value */

#if HDPMIPT_SWITCH_STACK
void SwitchStackIO( uint32_t(*pFunc)(void), int mode, uint32_t[] );
uint32_t HDPMIPT_NewStack[4];
#endif

static QEMM_IODT_LINK HDPMIPT_IODT_header;
static QEMM_IODT_LINK* HDPMIPT_IODT_Link = &HDPMIPT_IODT_header;

#if 1
void HDPMI_PrintPorts( void )
{
    QEMM_IODT_LINK* link = HDPMIPT_IODT_header.next;
    for ( ; link; link = link->next ) {
        printf( "ports: " );
        for ( int i = 0, start = 0; i < link->count; i++ ) {
            if ( i == (link->count - 1) || link->iodt[i].port != (link->iodt[i+1].port - 1) ) {
                if ( i == start )
                    printf( "%X ", link->iodt[start].port );
                else
                    printf( "%X-%X ", link->iodt[start].port, link->iodt[i].port );
                start = i + 1;
            }
        }
        printf( "\n" );
    }
    return;
}
#endif

static uint16_t HDPMIPT_GetDS()
{
    uint16_t ds;
    asm("mov %%ds, %0":"=r"(ds));
    return ds;
}

static uint32_t __attribute__((noinline)) HDPMIPT_TrapHandler()
{
    uint32_t port = 0, flags = 0, value = 0;
    asm(
        "mov %%edx, %0 \n\t"
        "mov %%ecx, %1 \n\t"
        "mov %%eax, %2 \n\t"
    :"=m"(port),"=m"(flags),"=m"(value)
    :
    :"memory"
    );

    //if(port >= 0 && port <= 0xF)
        //_LOG("Trapped PM: %s %x\n", out ? "out" : "in", port);
    QEMM_IODT_LINK* link = HDPMIPT_IODT_header.next;
    while(link)
    {
        for(int i = 0; i < link->count; ++i)
            if(link->iodt[i].port == port)
                return link->iodt[i].handler(port, value, flags & 1);
        link = link->next;
    }
    return value;
}

static void __attribute__((naked)) HDPMIPT_TrapHandlerWrapperIn()
{
    SwitchStackIO( &HDPMIPT_TrapHandler, 0, HDPMIPT_NewStack );
    asm("lret"); //retf
}

static void __attribute__((naked)) HDPMIPT_TrapHandlerWrapperOut()
{
    SwitchStackIO( &HDPMIPT_TrapHandler, 1, HDPMIPT_NewStack );
    asm("lret"); //retf
}

static int HDPMIPT_GetVendorEntry(HDPMIPT_ENTRY* entry)
{
    int result = 0;
    asm(
    "push %%es \n\t"
    "push %%esi \n\t"
    "push %%edi \n\t"
    "xor %%edi, %%edi \n\t"
    "mov %%edi, %%es \n\t"
    "mov $0x168A, %%ax \n\t"
    "mov %3, %%esi \n\t"
    "int $0x2F \n\t"
    "mov %%es, %%ecx \n\t"  //entry->es & entry->edi may use register esi & edi
    "mov %%edi, %%edx \n\t" //save edi to edx and pop first
    "pop %%edi \n\t"
    "pop %%esi \n\t"
    "pop %%es \n\t"
    "mov %%eax, %0 \n\t"
    "mov %%cx, %1 \n\t"
    "mov %%edx, %2 \n\t"
    : "=r"(result),"=m"(entry->es), "=m"(entry->edi)
    : "m"(VENDOR_HDPMI)
    : "eax", "ecx", "edx","memory"
    );
    return (result&0xFF) == 0; //al=0 to succeed
}

/* struct expected by HDPMI port trapping API ax=0006 in DS:ESI */

static struct __attribute__((packed)) _traphandler {
    uint32_t ofsIn;
    uint16_t segIn;
    uint32_t ofsOut;
    uint16_t segOut;
} traphandler;

static uint32_t HDPMI_Internal_InstallTrap(const HDPMIPT_ENTRY* entry, int start, int end, void(*handlerIn)(void), void(*handlerOut)(void) )
{
    uint32_t handle = 0;
    int count = end - start + 1;
    const HDPMIPT_ENTRY ent = *entry; //avoid gcc using ebx
    traphandler.ofsIn  = (uint32_t)handlerIn;
    traphandler.ofsOut = (uint32_t)handlerOut;
    asm(
        "push %%ebx \n\t"
        "push %%esi \n\t"
        "push %%edi \n\t"
        "mov %1, %%edx \n\t"  //DX: starting port
        "mov %2, %%ecx \n\t"  //CX: port count
        "lea %3, %%esi \n\t"  //ESI: handler addr, IN, OUT
        "movw %%cs, %5 \n\t"
        "movw %%cs, %6 \n\t"
        "mov $6, %%eax \n\t"  //AX=6, install port trap
        "lcall *%4\n\t"
        "movl $0, %0 \n\t"
        "jb 1f \n\t"
        "mov %%eax, %0 \n\t"
        "1: pop %%edi \n\t"
        "pop %%esi \n\t"
        "pop %%ebx \n\t"
    :"=m"(handle)
    :"m"(start),"m"(count),"m"(traphandler),"m"(ent),"m"(traphandler.segIn),"m"(traphandler.segOut)
    :"eax","ebx","ecx","edx","memory"
    );
    return handle;
}

static BOOL HDPMI_Internal_UninstallTrap(const HDPMIPT_ENTRY* entry, uint32_t handle)
{
    BOOL result = FALSE;
    asm(
    "mov %2, %%edx \n\t"  //EDX=handle
    "mov $7, %%eax \n\t" //ax=7, unistall port trap
    "lcall *%1\n\t"
    "jc 1f \n\t"
    "mov $1, %%eax \n\t"
    "mov %%eax, %0 \n\t"
    "1: nop \n\t"
    :"=m"(result)
    :"m"(*entry),"m"(handle)
    :"eax","ecx","edx","memory"
    );
    return result;
}

BOOL HDPMIPT_Detect()
{
    HDPMIPT_ENTRY entry;
    BOOL result = HDPMIPT_GetVendorEntry(&entry);
    return result && (entry.edi || entry.es);
}

BOOL HDPMIPT_Install_IOPortTrap(uint16_t start, uint16_t end, QEMM_IODT* inputp iodt, uint16_t count, QEMM_IOPT* outputp iopt)
{
    assert(iopt);
    if(HDPMIPT_IODT_header.next == NULL)
    {
#if HDPMIPT_SWITCH_STACK
        HDPMIPT_NewStack[0] = (uintptr_t)malloc( HDPMIPT_STACKSIZE ) + HDPMIPT_STACKSIZE - 8;
        HDPMIPT_NewStack[1] = HDPMIPT_GetDS();
#endif

        if(!HDPMIPT_GetVendorEntry(&HDPMIPT_Entry))
        {
            HDPMIPT_Entry.es = 0;
            HDPMIPT_Entry.edi = 0;
            puts("Failed to get HDPMI Vendor entry point.\n");
            return FALSE;
        }
        _LOG("HDPMI vendor entry: %04x:%08x\n", HDPMIPT_Entry.es, HDPMIPT_Entry.edi);
    }

    uint32_t handle = HDPMI_Internal_InstallTrap(&HDPMIPT_Entry, start, end, &HDPMIPT_TrapHandlerWrapperIn, &HDPMIPT_TrapHandlerWrapperOut);
    if(!handle)
    {
        return FALSE;
    }
    
    QEMM_IODT* Iodt = (QEMM_IODT*)malloc(sizeof(QEMM_IODT)*count);
    memcpy(Iodt, iodt, sizeof(QEMM_IODT)*count);

    QEMM_IODT_LINK* newlink = (QEMM_IODT_LINK*)malloc(sizeof(QEMM_IODT_LINK));
    newlink->iodt = Iodt;
    newlink->count = count;
    newlink->prev = HDPMIPT_IODT_Link;
    newlink->next = NULL;
    HDPMIPT_IODT_Link->next = newlink;
    HDPMIPT_IODT_Link = newlink;
    iopt->memory = (uintptr_t)newlink;
    iopt->handle = handle;
    return TRUE;
}

BOOL HDPMIPT_Uninstall_IOPortTrap(QEMM_IOPT* inputp iopt)
{
    CLIS();
    QEMM_IODT_LINK* link = (QEMM_IODT_LINK*)iopt->memory;
    link->prev->next = link->next;
    if(link->next) link->next->prev = link->prev;
    if(HDPMIPT_IODT_Link == link)
        HDPMIPT_IODT_Link = link->prev;
    STIL();
    HDPMI_Internal_UninstallTrap(&HDPMIPT_Entry, iopt->handle);
    free(link->iodt);
    free(link);
    
    if(HDPMIPT_IODT_header.next == NULL)
    {
        #if HDPMIPT_SWITCH_STACK
        free((void*)(HDPMIPT_NewStack[0] - HDPMIPT_STACKSIZE + 8));
        #endif
    }
    return TRUE;
}

BOOL HDPMIPT_InstallISR( uint8_t interrupt, void(*ISR)(void), DPMI_ISR_HANDLE* outputp handle )
{
    if ( _hdpmi_InstallISR( interrupt, ISR, handle, (void *)(HDPMIPT_NewStack[0] - HDPMIPT_STACKSIZE/2) ) ) {
        return ( _hdpmi_InstallInt31( handle ) );
    }
    return FALSE;
}

BOOL HDPMIPT_UninstallISR( DPMI_ISR_HANDLE* inputp handle )
{
    /* first uninstall int 31h, then ISR! */
    _hdpmi_UninstallInt31();
    return ( _hdpmi_UninstallISR( handle ) );
}

void HDPMIPT_UntrappedIO_Write(uint16_t port, uint8_t value)
{
    if(HDPMIPT_Entry.es == 0)
    {
        if(!HDPMIPT_GetVendorEntry(&HDPMIPT_Entry))
            return;
    }

    asm(
    "push %%ebx \n\t"
    "mov %1, %%dx \n\t"     //dx=port
    "mov %2, %%cl \n\t"     //cl=value to write
    "mov $1, %%bl \n\t"     //bl=mode; 1=out dx, al
    "mov $0x08, %%ax \n\t"  //function no.
    "lcall *%0\n\t"
    "pop %%ebx \n\t"
    :
    :"m"(HDPMIPT_Entry),"m"(port),"m"(value)
    :"eax","ecx","edx"
    );
}

uint8_t HDPMIPT_UntrappedIO_Read(uint16_t port)
{
    if(HDPMIPT_Entry.es == 0)
    {
        if(!HDPMIPT_GetVendorEntry(&HDPMIPT_Entry))
            return 0;
    }
    uint8_t result = 0;
    asm(
    "push %%ebx \n\t"
    "mov %2, %%dx \n\t"     //dx=port
    "mov $0, %%bl \n\t"   //bl=mode; 0=in al, dx
    "mov $0x08, %%ax \n\t"  //function no.
    "lcall *%1\n\t"
    "pop %%ebx \n\t"
    "mov %%al, %0 \n\t"
    :"=m"(result)
    :"m"(HDPMIPT_Entry),"m"(port)
    :"eax","ecx","edx"
    );
    return result;
}