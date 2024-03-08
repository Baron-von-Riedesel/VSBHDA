
/* port trapping
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <dos.h>    /* includes pc.h; for outp() */
//#include <fcntl.h>  /* for _dos_open() */
#include <assert.h>
#ifdef DJGPP
#include <go32.h>
#include <sys/ioctl.h>
#else
#include <conio.h>  /* contains outp()/inp() in OW */
#endif

#include "CONFIG.H"
#include "PLATFORM.H"
#include "LINEAR.H"
#include "PTRAP.H"
#include "VOPL3.H"
#include "VDMA.H"
#include "VIRQ.H"
#include "VSB.H"
#include "HAPI.H"

// next 2 defines must match EQUs in rmwrap.asm!
#define HANDLE_IN_388H_DIRECTLY 1
#define RMPICTRAPDYN 0 /* trap PIC for v86-mode dynamically when needed */

uint32_t _hdpmi_rmcbIO( void(*Fn)( __dpmi_regs *), __dpmi_regs *reg, __dpmi_raddr * );
void _hdpmi_CliHandler( void );
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

struct HDPMIAPI_ENTRY HDPMIAPI_Entry; /* vendor API entry */

void    (*UntrappedIO_OUT_Handler)(uint16_t port, uint8_t value) = (void (*)(uint16_t, uint8_t))&outp;
uint8_t (*UntrappedIO_IN_Handler)(uint16_t port) = (uint8_t (*)(uint16_t))&inp;

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



static void RM_TrapHandler( __dpmi_regs * regs)
///////////////////////////////////////////////
{
    uint16_t port = regs->x.dx;
    uint8_t val = regs->h.al;
    uint8_t out = regs->h.cl;
    int i;

    for ( i = 0; i < maxports; i++ ) {
        if( PDispTab[i].port == port) {
            regs->x.flags &= ~CPU_CFLAG;
            //uint8_t val2 = PDispTab[i].handler( port, val, out );
            //regs->h.al = out ? regs->h.al : val2;
            regs->h.al = PDispTab[i].handler( port, val, out );
            return;
        }
    }

    /* this should never be reached. */

    dbgprintf(("RM_TrapHandler: unhandled port=%x val=%x out=%x (OldCB=%x:%x)\n", port, val, out, QPI_OldCallbackCS, QPI_OldCallbackIP ));
    //regs->w.flags |= CPU_CFLAG;
    if ( QPI_OldCallbackCS ) {
        __dpmi_regs r = *regs;
        r.x.cs = QPI_OldCallbackCS;
        r.x.ip = QPI_OldCallbackIP;
        __dpmi_simulate_real_mode_procedure_retf(&r);
        regs->x.flags |= r.x.flags & CPU_CFLAG;
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
    __dpmi_regs r = {0};
#if 0 /* OW doesn't know ioctl() */
    uint32_t entryfar = 0;
    int fd = 0;
    unsigned int result = _dos_open("QEMM386$", O_RDONLY, &fd);
    //ioctl - read from character device control channel
    if (result == 0) { //QEMM detected?
        int count = ioctl(fd, DOS_RCVDATA, 4, &entryfar);
        _dos_close(fd);
        if(count == 4) {
            r.x.ip = entryfar & 0xFFFF;
            r.x.cs = entryfar >> 16;
        }
    }
#else
    if ( ReadLinearD( 0x67*4 ) ) { /* int 67h initialized? */
        r.x.cx = 0x5145; /* "QE" */
        r.x.dx = 0x4d4d; /* "MM" */
        r.x.flags = 0x202;
        r.x.ax = 0x3f00;
        __dpmi_simulate_real_mode_interrupt(0x67, &r);
        if ( r.h.ah == 0 && r.x.es ) {
            r.x.ip = r.x.di;
            r.x.cs = r.x.es;
        }
    }
#endif
    /* if Qemm hasn't been found, try Jemm's QPIEMU ... */
    if ( r.x.cs == 0 ) {
        /* QPIEMU installation check;
         * getting the entry point of QPIEMU is non-trivial in protected-mode, since
         * the int 2Fh must be executed as interrupt ( not just "simulated" ). Here
         * a small ( 3 bytes ) helper proc is constructed on the fly, at PSP:005Ch:
         * a INT 2Fh, followed by an RETF.
         */
        uint8_t int2frm[] = { 0xCD, 0x2F, 0xCB };
        uint32_t dosmem = _my_psp() + 0x5C;
        memcpy( NearPtr(dosmem), int2frm, 3 );
        r.x.ax = 0x1684;
        r.x.bx = 0x4354;
        r.x.cs = _my_psp() >> 4;
        r.x.ip = 0x5C;
        if( __dpmi_simulate_real_mode_procedure_retf(&r) != 0 || r.h.al )
            return 0;
        r.x.ip = r.x.di;
        r.x.cs = r.x.es;
    }
    r.h.ah = 0x03; /* get version */
    if( __dpmi_simulate_real_mode_procedure_retf(&r) == 0 ) {
        QPI_Entry.offset16 = r.x.ip;
        QPI_Entry.segment  = r.x.cs;
        return r.x.ax;
    }
    return 0;
}

bool PTRAP_Prepare_RM_PortTrap()
////////////////////////////////
{
    static __dpmi_regs TrapHandlerREG; /* static RMCS for RMCB */
    __dpmi_regs r = {0};
#if HANDLE_IN_388H_DIRECTLY || !RMPICTRAPDYN
    uint32_t dosmem;
#endif

    r.x.ip = QPI_Entry.offset16;
    r.x.cs = QPI_Entry.segment;
    r.x.ax = 0x1A06;
    /* get current trap handler */
    if(__dpmi_simulate_real_mode_procedure_retf(&r) != 0 || (r.x.flags & CPU_CFLAG))
        return false;
    QPI_OldCallbackIP = r.x.es;
    QPI_OldCallbackCS = r.x.di;
    dbgprintf(("PTRAP_Prepare_RM_PortTrap: old callback=%x:%x\n",r.x.es, r.x.di));

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
    dosmem = _my_psp() + 0x80;

    /* OW refuses to subtract two function addresses */
    //memcpy( NearPtr(dosmem), &PTRAP_RM_Wrapper, &PTRAP_RM_WrapperEnd - &PTRAP_RM_Wrapper );
    memcpy( NearPtr(dosmem), &PTRAP_RM_Wrapper, 0x80 );

    /* the first 12 bytes are variables, now to be initialized */

    memcpy( NearPtr(dosmem), &rmcb, 4 );
#if !RMPICTRAPDYN
    memcpy( NearPtr(dosmem + 8), &QPI_Entry, 4 );
#endif

    /* set new trap handler ES:DI */
    r.x.es = dosmem >> 4;
    r.x.di = 3*4;
#else
    r.x.es = rmcb.segment;
    r.x.di = rmcb.offset16;
#endif
    r.x.ax = 0x1A07; /* set trap handler */
    if( __dpmi_simulate_real_mode_procedure_retf(&r) != 0 || (r.x.flags & CPU_CFLAG))
        return false;
	return true;
}


static bool Install_RM_PortTrap( uint16_t start, uint16_t end )
///////////////////////////////////////////////////////////////
{
    __dpmi_regs r = {0};
    int i;

    r.x.ip = QPI_Entry.offset16;
    r.x.cs = QPI_Entry.segment;
    for( i = start; i < end; i++ ) {
        if ( QPI_OldCallbackCS ) {
            /* this is unreliable, since if the port was already trapped, there's no
             * guarantee that the previous handler can actually handle it.
             * so it might be safer to ignore the old state and - on exit -
             * untrap the port in any case!
             */
            r.x.ax = 0x1A08; /* get port status */
            r.x.dx = PDispTab[i].port;
            __dpmi_simulate_real_mode_procedure_retf(&r);
            PDispTab[i].flags |= (r.h.bl) << 8; //previously trapped state
        }
        r.x.ax = 0x1A09; /* trap port */
        r.x.dx = PDispTab[i].port;
        __dpmi_simulate_real_mode_procedure_retf(&r); /* trap port */
        PDispTab[i].flags |= PDT_FLGS_RMINST;
    }
    return true;
}

bool PTRAP_Install_RM_PortTraps( void )
///////////////////////////////////////
{
    int max = countof(portranges) - 1;
    int i;
    maxports = portranges[max];

    for ( i = 0; i < max; i++ ) {
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

/* set PIC port trap when a SB IRQ is emulated.
 * Since the SB IRQ is emulated, no EOI must be sent to the PIC.
 */

void PTRAP_SetPICPortTrap( int bSet )
/////////////////////////////////////
{
    /* might be called even if support for v86 is disabled */
    if ( QPI_Entry.segment ) {
#if RMPICTRAPDYN
        __dpmi_regs r = {0};
        r.x.ip = QPI_Entry.offset16;
        r.x.cs = QPI_Entry.segment;
        r.x.dx = PDispTab[PICIndex].port;
        if ( bSet ) {
            r.x.ax = 0x1A09; /* trap */
            PDispTab[PICIndex].flags |= PDT_FLGS_RMINST;
        } else {
            r.x.ax = 0x1A0A; /* untrap */
            PDispTab[PICIndex].flags &= ~PDT_FLGS_RMINST;
        }
        __dpmi_simulate_real_mode_procedure_retf(&r); /* trap port */
#else
        /* patch the 16-bit real-mode code stored in the PSP.
         * see rmwrap.asm, proc PTRAP_RM_Wrapper.
         * 6 is the offset where the trapped port (PIC) in RMWRAP.ASM is stored!
         */
        uint32_t dosmem = _my_psp() + 0x80 + 6;
        WriteLinearW( dosmem, bSet ? 0xffff : 0x0020 );
#endif
    }
    return;
}

bool PTRAP_Uninstall_RM_PortTraps( void )
/////////////////////////////////////////
{
    int max = portranges[END_PDT];
    int i;
    __dpmi_regs r = {0};

    r.x.ip = QPI_Entry.offset16;
    r.x.cs = QPI_Entry.segment;
    for( i = 0; i < max; ++i ) {
        if ( !( PDispTab[i].flags & 0xff00 )) {
            if( PDispTab[i].flags & PDT_FLGS_RMINST ) {
                r.x.ax = 0x1A0A; /* clear port trap */
                r.x.dx = PDispTab[i].port;
                __dpmi_simulate_real_mode_procedure_retf(&r);
                PDispTab[i].flags &= ~PDT_FLGS_RMINST;
                //dbgprintf(("PTRAP_Uninstall_RM_PortTraps: port %X untrapped\n", PDispTab[i].port ));
            }
        }
    }
    r.x.ax = 0x1A07; /* set trap handler */
    r.x.es = QPI_OldCallbackCS;
    r.x.di = QPI_OldCallbackIP;
    if( __dpmi_simulate_real_mode_procedure_retf(&r) != 0) //restore old handler
        return false;

    __dpmi_free_real_mode_callback( &rmcb );

    return true;
}

/////////////////////////////////////////////////////////

uint32_t PTRAP_PM_TrapHandler( uint32_t port, uint32_t flags, uint32_t value )
//////////////////////////////////////////////////////////////////////////////
{
    int i;
    for( i = 0; i < maxports; i++ )
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

bool PTRAP_DetectHDPMI()
////////////////////////
{
    uint8_t result = _get_hdpmi_vendor_api(&HDPMIAPI_Entry);
    return (result == 0 && HDPMIAPI_Entry.seg);
}

static uint32_t PTRAP_Int_Install_PM_Trap( int start, int end, void(*handlerIn)(void), void(*handlerOut)(void) )
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    struct _hdpmi_traphandler traphandler;
    traphandler.ofsIn  = (uint32_t)handlerIn;
    traphandler.ofsOut = (uint32_t)handlerOut;
    return _hdpmi_install_trap( start, end - start + 1, &traphandler );
}

bool PTRAP_Install_PM_PortTraps( void )
///////////////////////////////////////
{
    int max = countof(portranges) - 1;
    int i;
    int start, end;
    maxports = portranges[max];

    /* reset hdpmi=32 option in case it is set */
    _hdpmi_set_context_mode( 0 );

    /* install CLI handler */
    _hdpmi_set_cli_handler( _hdpmi_CliHandler );

    for ( i = 0; i < max; i++ ) {
        if ( portranges[i+1] > portranges[i] ) { /* skip if range is empty */
            start = PDispTab[portranges[i]].port;
            end = PDispTab[portranges[i+1]-1].port;
            dbgprintf(("PTRAP_Install_PM_PortTraps: %X-%X\n", start, end ));
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
    int i;
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
        for( i = portranges[SB_PDT]; i < portranges[SB_PDT+1]; i++ )
            PDispTab[i].port += sbaddr - 0x220;

    /* if no OPL3 emulation, skip ports 0x388-0x38b and 0x220-0x223 */
    if ( !opl ) {
        PDT_DelEntries( portranges[OPL3_PDT], portranges[END_PDT], 4 );
        PDT_DelEntries( portranges[SB_PDT], portranges[END_PDT], 4 );
    }
}

bool PTRAP_Uninstall_PM_PortTraps( void )
/////////////////////////////////////////
{
    int i;
    for ( i = 0; traphdl[i]; i++ )
        _hdpmi_uninstall_trap( traphdl[i] );

    /* uninstall CLI trap handler */
    _hdpmi_set_cli_handler( NULL );

    return true;
}

void PTRAP_UntrappedIO_OUT(uint16_t port, uint8_t value)
////////////////////////////////////////////////////////
{
    _hdpmi_simulate_byte_out( port, value );
    return;
}

uint8_t PTRAP_UntrappedIO_IN(uint16_t port)
///////////////////////////////////////////
{
    return _hdpmi_simulate_byte_in( port );
}

#ifdef _DEBUG
void PTRAP_PrintPorts( void )
/////////////////////////////
{
    int start = 0;
    int i;
    dbgprintf(( "ports:\n" ));
    for ( i = 0; i < maxports; i++ ) {
        if ( i < (maxports -1) && ( PDispTab[i+1].port != PDispTab[i].port+1 || PDispTab[i+1].flags != PDispTab[i].flags )) {
            if ( i == start )
                dbgprintf(( "%X (%X)\n", PDispTab[start].port, PDispTab[start].flags ));
            else
                dbgprintf(( "%X-%X (%X)\n", PDispTab[start].port, PDispTab[i].port, PDispTab[start].flags ));
            start = i + 1;
        }
    }
    return;
}
#endif
