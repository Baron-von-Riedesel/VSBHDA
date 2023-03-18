#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifndef __GNUC__ //make vscode happy
#define __attribute__(x)
#endif
#include "SBEMUCFG.H"
#include "UNTRAPIO.H"
#include "DPMI_.H"
#include "HDPMIPT.H"

#define HDPMIPT_SWITCH_STACK 1 /* must be 1 */
#define HDPMIPT_STACKSIZE 16384

BOOL _hdpmi_InstallISR( uint8_t i, int(*ISR)(void), void* pStack );
BOOL _hdpmi_UninstallISR( void );
BOOL _hdpmi_InstallInt31( uint8_t );
BOOL _hdpmi_UninstallInt31( void );

static QEMM_IODT *pIodt;
static int maxports;

static uint32_t traphdl[9] = {0}; /* hdpmi32i trap handles */

typedef struct
{
    uint32_t edi;
    uint16_t es;
}HDPMIPT_ENTRY;

/* struct expected by HDPMI port trapping API ax=0006 in DS:ESI */

static struct __attribute__((packed)) _traphandler {
    uint32_t ofsIn;
    uint16_t segIn;
    uint32_t ofsOut;
    uint16_t segOut;
} traphandler;

static const char* VENDOR_HDPMI = "HDPMI";    //vendor string
static HDPMIPT_ENTRY HDPMIPT_Entry;

#if HDPMIPT_SWITCH_STACK
void SwitchStackIO( uint32_t(*pFunc)(uint32_t, uint32_t, uint32_t), int mode, uint32_t[] );
uint32_t HDPMIPT_NewStack[4]; /* index 2+3 are used to store old ss:esp! */
#endif

#ifdef _DEBUG
void HDPMIPT_PrintPorts( QEMM_IODT *iodt, int max )
///////////////////////////////////////////////////
{
	int start = 0;
	dbgprintf( "ports:\n" );
	for ( int i = 0; i < max; i++ ) {
		if ( i < (max -1) && ( iodt[i+1].port != iodt[i].port+1 || iodt[i+1].flags != iodt[i].flags )) {
			if ( i == start )
				dbgprintf( "%X (%X)\n", iodt[start].port, iodt[start].flags );
			else
				dbgprintf( "%X-%X (%X)\n", iodt[start].port, iodt[i].port, iodt[start].flags );
			start = i + 1;
		}
	}
    return;
}
#endif

static uint16_t HDPMIPT_GetDS()
///////////////////////////////
{
    uint16_t ds;
    asm("mov %%ds, %0":"=r"(ds));
    return ds;
}

static uint32_t __attribute__((noinline)) HDPMIPT_TrapHandler( uint32_t port, uint32_t flags, uint32_t value )
//////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    for(int i = 0; i < maxports; i++ )
        if( (pIodt+i)->port == port)
            return (pIodt+i)->handler(port, value, flags & 1);

    /* ports that are trapped, but not handled; this may happen, since
     * hdpmi32i's support for port trapping is limited to 8 ranges.
     */
    if ( flags )
        UntrappedIO_OUT( port, value );
    else
        value = UntrappedIO_IN( port );
    return value;
}

static void __attribute__((naked)) HDPMIPT_TrapHandlerWrapperIn()
/////////////////////////////////////////////////////////////////
{
#if HDPMIPT_SWITCH_STACK
    SwitchStackIO( &HDPMIPT_TrapHandler, 0, HDPMIPT_NewStack );
#else
    uint32_t port;
    uint32_t value;
    asm("mov %%eax, %0\n\t"
        "mov %%edx, %1\n\t"
        :"=m"(value), "=m"(port)
    );
    HDPMIPT_TrapHandler( port, 0, value );
#endif
    asm("lret"); //retf
}

static void __attribute__((naked)) HDPMIPT_TrapHandlerWrapperOut()
//////////////////////////////////////////////////////////////////
{
#if HDPMIPT_SWITCH_STACK
    SwitchStackIO( &HDPMIPT_TrapHandler, 1, HDPMIPT_NewStack );
#else
    uint32_t port;
    uint32_t value;
    asm("mov %%eax, %0\n\t"
        "mov %%edx, %1\n\t"
        :"=m"(value), "=m"(port)
    );
    HDPMIPT_TrapHandler( port, 1, value );
#endif
    asm("lret"); //retf
}

static int HDPMIPT_GetVendorEntry( void )
/////////////////////////////////////////
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
    : "=r"(result),"=m"(HDPMIPT_Entry.es), "=m"(HDPMIPT_Entry.edi)
    : "m"(VENDOR_HDPMI)
    : "eax", "ecx", "edx","memory"
    );
    return (result&0xFF) == 0; //al=0 to succeed
}

BOOL HDPMIPT_Detect()
/////////////////////
{
    BOOL result = HDPMIPT_GetVendorEntry();
    return( result && HDPMIPT_Entry.es );
}

static uint32_t HDPMI_Internal_InstallTrap( int start, int end, void(*handlerIn)(void), void(*handlerOut)(void) )
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    uint32_t handle = 0;
    int count = end - start + 1;
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
        "mov $6, %%ax \n\t"   //AX: 6=install port trap
        "lcall *%4\n\t"
        "movl $0, %0 \n\t"
        "jb 1f \n\t"
        "mov %%eax, %0 \n\t"
        "1: pop %%edi \n\t"
        "pop %%esi \n\t"
        "pop %%ebx \n\t"
    :"=m"(handle)
    :"m"(start),"m"(count),"m"(traphandler),"m"(HDPMIPT_Entry),"m"(traphandler.segIn),"m"(traphandler.segOut)
    :"eax","ebx","ecx","edx","memory"
    );
    return handle;
}

BOOL HDPMIPT_Install_PortTraps( QEMM_IODT iodt[], int Rangetab[], int max )
///////////////////////////////////////////////////////////////////////////
{
#if HDPMIPT_SWITCH_STACK
	HDPMIPT_NewStack[0] = (uintptr_t)malloc( HDPMIPT_STACKSIZE ) + HDPMIPT_STACKSIZE;
	HDPMIPT_NewStack[1] = HDPMIPT_GetDS();
#endif
    int start, end;
	pIodt = iodt;
	maxports = Rangetab[max];

	/* ensure that hdpmi=32 isn't set */
	asm(
		"push %%ebx \n\t"
		"mov $0, %%bl \n\t"
		"mov $5, %%ax \n\t"
		"lcall *%0\n\t"
		"pop %%ebx"
		::"m"(HDPMIPT_Entry)
	);

	for ( int i = 0; i < max; i++ ) {
		start = iodt[Rangetab[i]].port;
		end = iodt[Rangetab[i+1]-1].port;
		dbgprintf("HDPMIPT_Install_PortTraps: %X-%X\n", start, end );
		if (!(traphdl[i] = HDPMI_Internal_InstallTrap( start, end, &HDPMIPT_TrapHandlerWrapperIn, &HDPMIPT_TrapHandlerWrapperOut)))
			return FALSE;
	}
    return TRUE;
}

static BOOL HDPMI_Internal_UninstallTrap( uint32_t handle )
///////////////////////////////////////////////////////////
{
    BOOL result = FALSE;
    asm(
    "mov %2, %%edx \n\t"  //EDX=handle
    "mov $7, %%ax \n\t"   //ax=7, unistall port trap
    "lcall *%1\n\t"
    "jc 1f \n\t"
    "mov $1, %%eax \n\t"
    "mov %%eax, %0 \n\t"
    "1: nop \n\t"
    :"=m"(result)
    :"m"(HDPMIPT_Entry),"m"(handle)
    :"eax","ecx","edx","memory"
    );
    return result;
}

BOOL HDPMIPT_Uninstall_PortTraps(QEMM_IODT* iodt, int max )
///////////////////////////////////////////////////////////
{

    for ( int i = 0; traphdl[i]; i++ )
        HDPMI_Internal_UninstallTrap( traphdl[i] );

#if HDPMIPT_SWITCH_STACK
	free((void*)(HDPMIPT_NewStack[0] - HDPMIPT_STACKSIZE));
#endif
    return TRUE;
}

BOOL HDPMIPT_InstallISR( uint8_t interrupt, int(*ISR)(void) )
/////////////////////////////////////////////////////////////
{
    if ( _hdpmi_InstallISR( interrupt, ISR, (void *)(HDPMIPT_NewStack[0] - HDPMIPT_STACKSIZE/2) ) ) {
        return ( _hdpmi_InstallInt31( interrupt ) );
    }
    return FALSE;
}

BOOL HDPMIPT_UninstallISR( void )
/////////////////////////////////
{
    /* first uninstall int 31h, then ISR! */
    _hdpmi_UninstallInt31();
    return ( _hdpmi_UninstallISR() );
}

void HDPMIPT_UntrappedIO_OUT(uint16_t port, uint8_t value)
//////////////////////////////////////////////////////////
{
    asm(
        "push %%ebx \n\t"
        "mov %1, %%dx \n\t"     //dx=port
        "mov %2, %%cl \n\t"     //cl=value to write
        "mov $1, %%bl \n\t"     //bl=mode; 1="out dx, al"
        "mov $0x08, %%ax \n\t"  //ax=8; simulate IO
        "lcall *%0\n\t"
        "pop %%ebx \n\t"
        :
        :"m"(HDPMIPT_Entry),"m"(port),"m"(value)
        :"eax","ecx","edx"
    );
}

uint8_t HDPMIPT_UntrappedIO_IN(uint16_t port)
/////////////////////////////////////////////
{
    uint8_t result = 0;

    asm(
        "push %%ebx \n\t"
        "mov %2, %%dx \n\t"     //dx=port
        "mov $0, %%bl \n\t"     //bl=mode; 0="in al, dx"
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
