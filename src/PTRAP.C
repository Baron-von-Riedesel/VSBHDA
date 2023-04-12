
/* port trapping
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <dos.h>
#include <fcntl.h>
#include <assert.h>
#include <dpmi.h>
#include <go32.h>
#ifdef DJGPP
#include <sys/ioctl.h>
#endif

#include "CONFIG.H"
#include "PLATFORM.H"
#include "DPMIHLP.H"
#include "PTRAP.H"
#include "VOPL3.H"
#include "VDMA.H"
#include "VIRQ.H"
#include "VSB.H"

// next 2 defines must match EQUs in rmwrap.asm!
#define HANDLE_IN_388H_DIRECTLY 1
#define RMPICTRAPDYN 0 /* trap PIC for v86-mode dynamically when needed */

uint32_t _hdpmi_rmcbIO( void(*Fn)( DPMI_REG*), DPMI_REG* reg, __dpmi_raddr * );
bool _hdpmi_CliHandler( void );
void SwitchStackIOIn(  void );
void SwitchStackIOOut( void );

static __dpmi_raddr QPI_Entry;

static uint16_t QPI_OldCallbackIP;
static uint16_t QPI_OldCallbackCS;
static __dpmi_raddr rmcb;

static int maxports;
static int PICIndex;

#if HANDLE_IN_388H_DIRECTLY || !RMPICTRAPDYN
extern void PTRAP_RM_Wrapper( void );
extern void PTRAP_RM_WrapperEnd( void );
#endif

static uint32_t traphdl[9] = {0}; /* hdpmi32i trap handles */

struct HDPMIPT_ENTRY {
    uint32_t edi;
    uint16_t es;
};

/* struct expected by HDPMI port trapping API ax=0006 in DS:ESI */

static struct __attribute__((packed)) _traphandler {
    uint32_t ofsIn;
    uint16_t segIn;
    uint32_t ofsOut;
    uint16_t segOut;
} traphandler;

static const char* VENDOR_HDPMI = "HDPMI"; /* vendor string */
static struct HDPMIPT_ENTRY HDPMIPT_Entry; /* vendor API entry */

void (*UntrappedIO_OUT_Handler)(uint16_t port, uint8_t value) = &outp;
uint8_t (*UntrappedIO_IN_Handler)(uint16_t port) = &inp;

static const uint8_t ChannelPageMap[] = { 0x87, 0x83, 0x81, 0x82, -1, 0x8b, 0x89, 0x8a };

struct PortDispatchTable {
    uint16_t    port;
    uint16_t    flags;
    PORT_TRAP_HANDLER handler;
};

#define tport( port, proc ) TPORT_ ## port,
#define tportx( port, proc, range ) TPORT_ ## port,
enum TrappedPorts {
#include "PORTS.H"
#undef tport
#undef tportx
    NUM_TPORTS
};

#define tport( port, proc ) { port, 0, proc },
#define tportx( port, proc, range ) { port, 0, proc },
static struct PortDispatchTable PDispTab[] = {
#include "PORTS.H"
#undef tport
#undef tportx
};

#define tport( port, proc )
#define tportx( port, proc, range )  range ## _PDT,
/* order of port ranges - must match order in ports.h */
enum PortRangeStartIndex {
#include "PORTS.H"
    END_PDT
};
#undef tport
#undef tportx

#define tport( port, proc )
#define tportx( port, proc, range ) TPORT_ ## port,
static int portranges[] = {
#include "PORTS.H"
    NUM_TPORTS
};
#undef tport
#undef tportx



static void RM_TrapHandler( DPMI_REG * regs)
////////////////////////////////////////////
{
    uint16_t port = regs->w.dx;
    uint8_t val = regs->h.al;
    uint8_t out = regs->h.cl;

    for ( int i = 0; i < maxports; i++ ) {
        if( PDispTab[i].port == port) {
            regs->w.flags &= ~CPU_CFLAG;
            //uint8_t val2 = PDispTab[i].handler( port, val, out );
            //regs->h.al = out ? regs->h.al : val2;
            regs->h.al = PDispTab[i].handler( port, val, out );
            return;
        }
    }

    /* this should never be reached. */

    dbgprintf("RM_TrapHandler: unhandled port=%x val=%x out=%x (OldCB=%x:%x)\n", port, val, out, QPI_OldCallbackCS, QPI_OldCallbackIP );
    //regs->w.flags |= CPU_CFLAG;
    if ( QPI_OldCallbackCS ) {
        DPMI_REG r = *regs;
        r.w.cs = QPI_OldCallbackCS;
        r.w.ip = QPI_OldCallbackIP;
        DPMI_CallRealModeRETF(&r);
        regs->w.flags |= r.w.flags & CPU_CFLAG;
        regs->h.al = r.h.al;
    }
}

//https://www.cs.cmu.edu/~ralf/papers/qpi.txt
//https://fd.lod.bz/rbil/interrup/memory/673f_cx5145.html
//http://mirror.cs.msu.ru/oldlinux.org/Linux.old/docs/interrupts/int-html/rb-7414.htm

uint16_t PTRAP_GetQEMMVersion(void)
///////////////////////////////////
{
    //http://mirror.cs.msu.ru/oldlinux.org/Linux.old/docs/interrupts/int-html/rb-2830.htm
    int fd = 0;
    unsigned int result = _dos_open("QEMM386$", O_RDONLY, &fd);
    uint32_t entryfar = 0;
    //ioctl - read from character device control channel
    DPMI_REG r = {0};
    if (result == 0) { //QEMM detected?
        int count = ioctl(fd, DOS_RCVDATA, 4, &entryfar);
        _dos_close(fd);
        if(count == 4) {
            r.w.ip = entryfar & 0xFFFF;
            r.w.cs = entryfar >> 16;
        }
    }
    /* if Qemm hasn't been found, try Jemm's QPIEMU ... */
    if ( r.w.cs == 0 ) {
        /* QPIEMU installation check;
         * getting the entry point of QPIEMU is non-trivial in protected-mode, since
         * the int 2Fh must be executed as interrupt ( not just "simulated" ). Here
         * a small ( 3 bytes ) helper proc is constructed on the fly, at PSP:005Ch:
         * a INT 2Fh, followed by an RETF.
         */
        uint32_t dosmem = _go32_info_block.linear_address_of_original_psp + 0x5C;
        uint8_t int2frm[] = { 0xCD, 0x2F, 0xCB };
        DPMI_CopyLinear( dosmem, DPMI_PTR2L(&int2frm), 3 );
        r.w.ax = 0x1684;
        r.w.bx = 0x4354;
        r.w.cs = _go32_info_block.linear_address_of_original_psp >> 4;
        r.w.ip = 0x5C;
        if( DPMI_CallRealModeRETF(&r) != 0 || (r.w.ax & 0xff))
            return 0;
        r.w.ip = r.w.di;
        r.w.cs = r.w.es;
    }
    r.h.ah = 0x03; /* get version */
    if( DPMI_CallRealModeRETF(&r) == 0 ) {
        QPI_Entry.offset16 = r.w.ip;
        QPI_Entry.segment  = r.w.cs;
        return r.w.ax;
    }
    return 0;
}

bool PTRAP_Prepare_RM_PortTrap()
////////////////////////////////
{
    static DPMI_REG TrapHandlerREG; /* static RMCS for RMCB */
    DPMI_REG r = {0};
    r.w.ip = QPI_Entry.offset16;
    r.w.cs = QPI_Entry.segment;
    r.w.ax = 0x1A06;
    /* get current trap handler */
    if(DPMI_CallRealModeRETF(&r) != 0 || (r.w.flags & CPU_CFLAG))
        return false;
    QPI_OldCallbackIP = r.w.es;
    QPI_OldCallbackCS = r.w.di;
    dbgprintf("PTRAP_Prepare_RM_PortTrap: old callback=%x:%x\n",r.w.es, r.w.di);

    /* get a realmode callback */
    if ( _hdpmi_rmcbIO( &RM_TrapHandler, &TrapHandlerREG, &rmcb ) == 0 )
        return false;

#if HANDLE_IN_388H_DIRECTLY || !RMPICTRAPDYN
    /* copy 16-bit code to DOS memory (PSP:80h)
     * structure:
     * 0-3: realmode callback
     * 4-5: data
     * 6-7: unused
     * 8-B: Qemm/Jemm entry
     * C-x: code
     */
    uint32_t dosmem = _go32_info_block.linear_address_of_original_psp + 0x80;
    DPMI_CopyLinear( dosmem, DPMI_PTR2L(&rmcb), 4 );
    DPMI_StoreD( dosmem + 4, 0 );
#if !RMPICTRAPDYN
    DPMI_CopyLinear( dosmem + 8, DPMI_PTR2L(&QPI_Entry), 4 );
#endif
    DPMI_CopyLinear( dosmem + 4 + 4 + 4, DPMI_PTR2L( &PTRAP_RM_Wrapper ), &PTRAP_RM_WrapperEnd - &PTRAP_RM_Wrapper );

    /* set new trap handler ES:DI */
    r.w.es = dosmem >> 4;
    r.w.di = 4+4+4;
#else
    r.w.es = rmcb.segment;
    r.w.di = rmcb.offset16;
#endif
    r.w.ax = 0x1A07;
    if( DPMI_CallRealModeRETF(&r) != 0 || (r.w.flags & CPU_CFLAG))
        return false;
    return true;
}


static bool Install_RM_PortTrap( uint16_t start, uint16_t end )
///////////////////////////////////////////////////////////////
{
    DPMI_REG r = {0};

    r.w.ip = QPI_Entry.offset16;
    r.w.cs = QPI_Entry.segment;
    for( int i = start; i < end; i++ ) {
        if ( QPI_OldCallbackCS ) {
            /* this is unreliable, since if the port was already trapped, there's no
             * guarantee that the previous handler can actually handle it.
             * so it might be safer to ignore the old state and - on exit -
             * untrap the port in any case!
             */
            r.w.ax = 0x1A08;
            r.w.dx = PDispTab[i].port;
            DPMI_CallRealModeRETF(&r);
            PDispTab[i].flags |= (r.h.bl) << 8; //previously trapped state
        }
        r.w.ax = 0x1A09;
        r.w.dx = PDispTab[i].port;
        DPMI_CallRealModeRETF(&r); /* trap port */
        PDispTab[i].flags |= PDT_FLGS_RMINST;
    }
    return true;
}

bool PTRAP_Install_RM_PortTraps( void )
///////////////////////////////////////
{
    int max = countof(portranges) - 1;
    maxports = portranges[max];

    for ( int i = 0; i < max; i++ ) {
#if RMPICTRAPDYN
        if ( PDispTab[portranges[i]].port == 0x20 ) {
            PICIndex = portranges[i];
            continue;
        }
#endif
        Install_RM_PortTrap( portranges[i], portranges[i+1] );
    }
    return true;
}

void PTRAP_SetPICPortTrap( int bSet )
/////////////////////////////////////
{
    /* might be called even if support for v86 is disabled */
    if ( QPI_Entry.segment ) {
#if RMPICTRAPDYN
        DPMI_REG r = {0};
        r.w.ip = QPI_Entry.offset16;
        r.w.cs = QPI_Entry.segment;
        r.w.dx = PDispTab[PICIndex].port;
        if ( bSet ) {
            r.w.ax = 0x1A09; /* trap */
            PDispTab[PICIndex].flags |= PDT_FLGS_RMINST;
        } else {
            r.w.ax = 0x1A0A; /* untrap */
            PDispTab[PICIndex].flags &= ~PDT_FLGS_RMINST;
        }
        DPMI_CallRealModeRETF(&r); /* trap port */
#else
        /* patch the 16-bit real-mode code stored in the PSP.
         * see rmwrap.asm, proc PTRAP_RM_Wrapper
         */
        uint32_t dosmem = _go32_info_block.linear_address_of_original_psp + 0x80;
        if ( bSet ) {
            DPMI_StoreW( dosmem+12+2, -1 );  /* cmp dx,0xffff */
        } else {
            DPMI_StoreW( dosmem+12+2, 0x20); /* cmp dx,0x0020 */
        }
#endif
    }
    return;
}

bool PTRAP_Uninstall_RM_PortTraps( void )
/////////////////////////////////////////
{
    int max = portranges[END_PDT];
    DPMI_REG r = {0};

    r.w.ip = QPI_Entry.offset16;
    r.w.cs = QPI_Entry.segment;
    for(int i = 0; i < max; ++i) {
        if ( !( PDispTab[i].flags & 0xff00 )) {
            if( PDispTab[i].flags & PDT_FLGS_RMINST ) {
                r.w.ax = 0x1A0A; //clear trap
                r.w.dx = PDispTab[i].port;
                DPMI_CallRealModeRETF(&r);
                PDispTab[i].flags &= ~PDT_FLGS_RMINST;
                //dbgprintf("PTRAP_Uninstall_RM_PortTraps: port %X untrapped\n", PDispTab[i].port );
            }
        }
    }
    r.w.ax = 0x1A07;
    r.w.es = QPI_OldCallbackCS;
    r.w.di = QPI_OldCallbackIP;
    if( DPMI_CallRealModeRETF(&r) != 0) //restore old handler
        return false;

    DPMI_FreeRMCB( &rmcb );

    return true;
}

/////////////////////////////////////////////////////////

uint32_t PTRAP_PM_TrapHandler( uint32_t port, uint32_t flags, uint32_t value )
//////////////////////////////////////////////////////////////////////////////
{
    for(int i = 0; i < maxports; i++ )
        if( PDispTab[i].port == port)
            return PDispTab[i].handler(port, value, flags & 1);

    /* ports that are trapped, but not handled; this may happen, since
     * hdpmi32i's support for port trapping is limited to 8 ranges.
     */
    if ( flags & 1 )
        UntrappedIO_OUT( port, value );
    else
        value = UntrappedIO_IN( port );
    return value;
}

static int GetHDPMIVendorEntry( void )
//////////////////////////////////////
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
    return (result & 0xFF) == 0; //al=0 to succeed
}

bool PTRAP_DetectHDPMI()
////////////////////////
{
    bool result = GetHDPMIVendorEntry();
    return( result && HDPMIPT_Entry.es );
}

static uint32_t PTRAP_Int_Install_PM_Trap( int start, int end, void(*handlerIn)(void), void(*handlerOut)(void) )
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

bool PTRAP_Install_PM_PortTraps( void )
///////////////////////////////////////
{
    int max = countof(portranges) - 1;
    maxports = portranges[max];
    int start, end;

    /* ensure that hdpmi=32 isn't set */
    asm(
        "push %%ebx \n\t"
        "mov $0, %%bl \n\t"
        "mov $5, %%ax \n\t"
        "lcall *%0\n\t"
        "pop %%ebx"
        ::"m"(HDPMIPT_Entry)
    );

    /* install CLI trap handler */
    asm(
        "push %%ebx \n\t"
        "mov $0, %%bl \n\t"
        "mov $9, %%ax \n\t"
        "mov %%cs, %%ecx \n\t"
        "lea %1, %%edx \n\t"
        "lcall *%0\n\t"
        "pop %%ebx"
        ::"m"(HDPMIPT_Entry), "m"(_hdpmi_CliHandler)
    );

    for ( int i = 0; i < max; i++ ) {
        if ( portranges[i+1] > portranges[i] ) { /* skip if range is empty */
            start = PDispTab[portranges[i]].port;
            end = PDispTab[portranges[i+1]-1].port;
            dbgprintf("HDPMIPT_Install_PM_PortTraps: %X-%X\n", start, end );
            if (!(traphdl[i] = PTRAP_Int_Install_PM_Trap( start, end, &SwitchStackIOIn, &SwitchStackIOOut)))
                return false;
        }
    }
    return true;
}

static void PDT_DelEntries( int start, int end, int entries )
/////////////////////////////////////////////////////////////
{
    int i;
    for ( i = start; i < end - entries; i++ ) {
        PDispTab[i].port = PDispTab[i+entries].port;
        PDispTab[i].handler = PDispTab[i+entries].handler;
    }
    for ( i = 0; i < countof(portranges); i++ ) {
        if ( portranges[i] > start )
            portranges[i] -= entries;
    }
}

/* adjust PDispTab[] to current settings of /D, /H, /A, /OPL */

void PTRAP_Prepare( int opl, int sbaddr, int dma, int hdma )
////////////////////////////////////////////////////////////
{
    /* low dma: adjust the entry for DMA channel addr/count */
    PDispTab[portranges[DMA_PDT]].port   = dma * 2;
    PDispTab[portranges[DMA_PDT]+1].port = dma * 2 + 1;
    /* low dma: adjust the entry for DMA page reg */
    PDispTab[portranges[DMAPG_PDT]].port = ChannelPageMap[ dma ];
#if SB16
    if ( hdma ) {
        /* high dma: adjust the entry for DMA channel addr/count */
        PDispTab[portranges[HDMA_PDT]].port    = hdma * 4 + (0xC0-0x10);
        PDispTab[portranges[HDMA_PDT]+1].port  = hdma * 4 + 2 + (0xC0-0x10);
        /* high dma: adjust the entry for DMA page reg */
        PDispTab[portranges[DMAPG_PDT]+1].port = ChannelPageMap[ hdma ];
    } else {
        /* if no SB16 emulation, remove all HDMA ports */
        PDT_DelEntries( portranges[DMAPG_PDT]+1, portranges[END_PDT], 1 );
        PDT_DelEntries( portranges[HDMA_PDT], portranges[END_PDT], portranges[HDMA_PDT+1] - portranges[HDMA_PDT] );
    }
#endif
    /* adjust the SB ports to the selected base */
    if ( sbaddr != 0x220 )
        for( int i = portranges[SB_PDT]; i < portranges[SB_PDT+1]; i++ )
            PDispTab[i].port += sbaddr - 0x220;

    /* if no OPL3 emulation, skip ports 0x388-0x38b and 0x220-0x223 */
    if ( !opl ) {
        PDT_DelEntries( portranges[OPL3_PDT], portranges[END_PDT], 4 );
        PDT_DelEntries( portranges[SB_PDT], portranges[END_PDT], 4 );
    }
}

static bool PTRAP_Int_Uninstall_PM_Trap( uint32_t handle )
//////////////////////////////////////////////////////////
{
    bool result = false;
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

bool PTRAP_Uninstall_PM_PortTraps( void )
/////////////////////////////////////////
{
    for ( int i = 0; traphdl[i]; i++ )
        PTRAP_Int_Uninstall_PM_Trap( traphdl[i] );

    /* uninstall CLI trap handler */
    asm(
        "push %%ebx \n\t"
        "mov $0, %%bl \n\t"
        "mov $9, %%ax \n\t"
        "xor %%ecx, %%ecx \n\t"
        "xor %%edx, %%edx \n\t"
        "lcall *%0\n\t"
        "pop %%ebx"
        ::"m"(HDPMIPT_Entry)
    );

    return true;
}

void PTRAP_UntrappedIO_OUT(uint16_t port, uint8_t value)
////////////////////////////////////////////////////////
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

uint8_t PTRAP_UntrappedIO_IN(uint16_t port)
///////////////////////////////////////////
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

#ifdef _DEBUG
void PTRAP_PrintPorts( void )
/////////////////////////////
{
    int start = 0;
    dbgprintf( "ports:\n" );
    for ( int i = 0; i < maxports; i++ ) {
        if ( i < (maxports -1) && ( PDispTab[i+1].port != PDispTab[i].port+1 || PDispTab[i+1].flags != PDispTab[i].flags )) {
            if ( i == start )
                dbgprintf( "%X (%X)\n", PDispTab[start].port, PDispTab[start].flags );
            else
                dbgprintf( "%X-%X (%X)\n", PDispTab[start].port, PDispTab[i].port, PDispTab[start].flags );
            start = i + 1;
        }
    }
    return;
}
#endif
