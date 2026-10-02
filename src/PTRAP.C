
/* port trapping */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#ifdef DJGPP
#include <dos.h>    /* includes pc.h; for outp() */
#include <go32.h>
#include <sys/ioctl.h>
#else
#include <conio.h>  /* contains outp()/inp() in OW */
#endif

#include "CONFIG.H"
#include "PLATFORM.H"
#include "LINEAR.H"
#include "DPMI.H"
#include "PTRAP.H"
#include "HAPI.H"

#if IRQONPORTACC
extern void SNDISR_IrqOnPortAcc( void );
#endif

// next 2 defines must match EQUs in rmcode1.asm!
#define HANDLE_IN_388H_DIRECTLY 0
#define RMPICTRAPDYN 0 /* 1=trap PIC for v86-mode dynamically when needed */
#define MAXRANGES 8

extern struct globalvars gvars;
extern  int _init_rmcbIO( void(*Fn)( __dpmi_regs *), __dpmi_regs *reg, __dpmi_raddr * );
extern  int _exit_rmcbIO( __dpmi_raddr * );
extern void _hdpmi_CliHandler( void );
extern void SwitchStackIOIn(  void );
extern void SwitchStackIOOut( void );
#if HANDLE_IN_388H_DIRECTLY || !RMPICTRAPDYN
extern void * copyrmcode( void *, int );
#endif

enum {
    PDT_FLGS_RMINST = 1,
    //PDT_FLGS_PMINST = 2
};

struct ptrap_s {
    __dpmi_regs QPI_regs;   /* used for QPI access (either Qemm's or QPIEMU's) */
    __dpmi_raddr rmcb;      /* real-mode callback for IO port trapping in v86 */
    __dpmi_raddr QPI_OldCallback;
#if RMPICTRAPDYN
    static int PICIndex;
#endif
    int cntranges;  /* no of defined IO port ranges */
    int cntports;   /* no of trapped ports; set only if real-mode port trapping is active */
};

static struct ptrap_s ptrap;

struct PortRange_s {
    short start;
    short end;
    uint32_t portmap;
    const PORT_TRAP_HANDLER *ptfuncs;
    uint32_t traphdl; /* hdpmi32i port range trap handle */
};

static struct PortRange_s portranges[MAXRANGES];
/* state of ports trapped in real-mode */
static uint16_t PortState[48]; /* todo: should be allocated dynamically */

/* public globals */

#if HANDLE_IN_388H_DIRECTLY || !RMPICTRAPDYN
#define DOSMEMSTART 0x60 /* initial value for dosheap variable, offset in PSP, bits 0-3 must be zero */
void * dosheap;
#endif

struct HDPMIAPI_ENTRY HDPMIAPI_Entry; /* vendor API entry (FAR32/FAR16) */

void    (*UntrappedIO_OUT_Handler)(uint16_t port, uint8_t value) = (void (*)(uint16_t, uint8_t))&outp;
uint8_t (*UntrappedIO_IN_Handler)(uint16_t port) = (uint8_t (*)(uint16_t))&inp;


/* real-mode port trap handler;
 * called by SwitchStackIOrmcb().
 */

static void RM_TrapHandler( __dpmi_regs * regs)
///////////////////////////////////////////////
{
    uint16_t port = regs->x.dx;
    int i;

    /* regs.x.cl:
     * bit[2]: 1=out, 0=in;
     * bits 3,4 word/dword access, not used here
     * regs.x.ch:
     * bit[1]: IF
     */
    for ( i = 0; i < ptrap.cntranges; i++ ) {
        if( port >= portranges[i].start && port <= portranges[i].end ) {
            int j,k;
            unsigned int v;
            for ( k = 0,j = portranges[i].start, v = portranges[i].portmap; j < port; j++, k += v & 1, v >>= 1 );
            if ( v & 1) {
                regs->h.al = portranges[i].ptfuncs[k]( port, regs->h.al, regs->x.cx );
                regs->x.flags &= ~CPU_CFLAG; /* clear carry flag, indicates that access was handled */
#if IRQONPORTACC
                /* give the sound HW interrupt a chance to be triggered if:
                 * + interrupts disabled and OUT instr is emulated
                 * + port access isn't ISA DMA or PIC
                 * + no DSP DMA op is running
                 */
                if ( ((regs->x.cx & (TRAPF_IF | TRAPF_OUT)) == TRAPF_OUT) && port >= 0x100 && !VSB_Running() )
                    SNDISR_IrqOnPortAcc();
#endif
                return;
            }
        }
    }

    /* this should never be reached. */

    dbgprintf(("RM_TrapHandler: unhandled port=%x val=%x out=%x (OldCB=%x:%x)\n", regs->x.dx, regs->h.al, regs->h.cl, ptrap.QPI_OldCallback.v86.segment, ptrap.QPI_OldCallback.v86.offset ));
#if 0
    if ( ptrap.QPI_OldCallback.v86.segment ) {
        __dpmi_regs r = *regs;
        r.x.ip = ptrap.QPI_OldCallback.v86.offset;
        r.x.cs = ptrap.QPI_OldCallback.v86.segment;
        __dpmi_simulate_real_mode_procedure_retf(&r);
        regs->x.flags |= r.x.flags & CPU_CFLAG;
        regs->h.al = r.h.al;
    }
#elif 0
    if (regs->h.cl & TRAPF_OUT)
        UntrappedIO_OUT( regs->x.dx, regs->h.al );
    else
        regs->h.al = UntrappedIO_IN( regs->x.dx );
    regs->x.flags &= ~CPU_CFLAG;
#else
    regs->x.flags |= CPU_CFLAG;
#endif
    return;
}

/* protected-mode port trap handler;
 * called by SwitchStackIO();
 */

uint8_t PTRAP_PM_TrapHandler( uint16_t port, uint16_t flags, uint8_t value )
////////////////////////////////////////////////////////////////////////////
{
    int i;
    for ( i = 0; i < ptrap.cntranges; i++ ) {
        if( port >= portranges[i].start && port <= portranges[i].end ) {
            int j,k;
            unsigned int v;
            for ( k = 0,j = portranges[i].start, v = portranges[i].portmap; j < port; j++, k += v & 1, v >>= 1 );
            if ( v & 1)
                return portranges[i].ptfuncs[k](port, value, flags );
            break;
        }
    }

    /* ports that are trapped, but not handled; this may happen, since
     * hdpmi32i's support for port trapping is limited to 8 ranges.
     */
    if ( flags & TRAPF_OUT) {
        UntrappedIO_OUT( port, value );
        return value;
    } else
        return UntrappedIO_IN( port );
}


uint16_t PTRAP_GetQEMMVersion(void)
///////////////////////////////////
{
    __dpmi_regs r;
    r.x.ss = r.x.sp = 0;
    r.x.flags = 0x202;
#if 0 /* OW doesn't know ioctl() */
    uint32_t entryfar = 0;
    int fd = 0;
    unsigned int result = _dos_open("QEMM386$", O_RDONLY, &fd);
    //ioctl - read from character device control channel
    if (result == 0) { //QEMM detected?
        int count = ioctl(fd, DOS_RCVDATA, 4, &entryfar);
        _dos_close(fd);
        if(count == 4) {
            ptrap.QPI_regs.x.ip = entryfar & 0xFFFF;
            ptrap.QPI_regs.x.cs = entryfar >> 16;
        }
    }
#else
    if ( ReadLinearD( 0x67*4 ) ) { /* int 67h initialized? */
        r.x.cx = 0x5145; /* "QE" */
        r.x.dx = 0x4d4d; /* "MM" */
        r.x.ax = 0x3f00;
        __dpmi_simulate_real_mode_interrupt(0x67, &r);
        if ( r.h.ah == 0 && r.x.es ) {
            ptrap.QPI_regs.x.ip = r.x.di;
            ptrap.QPI_regs.x.cs = r.x.es;
        }
    }
#endif
    /* if Qemm hasn't been found, try Jemm's QPIEMU ... */
    if ( ptrap.QPI_regs.x.cs == 0 ) {
        /* QPIEMU installation check;
         * getting the entry point of QPIEMU is non-trivial in protected-mode, since
         * the int 2Fh must be executed as interrupt ( not just "simulated" ). Here
         * a 3 byte helper proc is constructed on the fly, at PSP:005Ch:
         * a INT 2Fh, followed by an RETF.
         */
        uint32_t *dosmem = NearPtr(_my_psp() + 0x5C);
        *dosmem = 0xCB2FCD;  /* INT 2Fh & RETF */
        r.x.ax = 0x1684; /* Int 2Fh, ax=1684: get device entry point */
        r.x.bx = 0x4354; /* device ID of QPIEMU */
        r.x.cs = _my_psp() >> 4;
        r.x.ip = 0x5C; /* real-mode CS:IP = PSP:005Ch */
        if( __dpmi_simulate_real_mode_procedure_retf(&r) != 0 || r.h.al )
            return 0;
        ptrap.QPI_regs.x.ip = r.x.di; /* entry point returned in ES:DI */
        ptrap.QPI_regs.x.cs = r.x.es;
    }
    ptrap.QPI_regs.h.ah = 0x03; /* get version */
    if( __dpmi_simulate_real_mode_procedure_retf(&ptrap.QPI_regs) == 0 ) {
        return ptrap.QPI_regs.x.ax;
    }
    return 0;
}

bool PTRAP_DetectHDPMI()
////////////////////////
{
    uint8_t result = _hdpmi_get_vendor_api(&HDPMIAPI_Entry);
    return (result == 0 && HDPMIAPI_Entry.seg);
}

#if HANDLE_IN_388H_DIRECTLY || !RMPICTRAPDYN

struct rmcode1 {   /* structure must match definitions in rmcode1.asm! */
    __dpmi_raddr rmcb; /* realmode callback */
    uint16_t data; /* port 0x388/0x389 access optimization (not active) */
    uint16_t wPort; /* used for PIC port trapping; contains either 0x0020 or 0xffff */
    __dpmi_raddr qpi;  /* QPI entry */
    uint8_t codev86[]; /* v86 code */
};

#endif

/* set PIC port trap when a SB IRQ is emulated.
 * if RMPICTRAPDYN==0, the PIC port is permanently trapped;
 * to avoid mode switches, the trapping is handled in v86-mode
 * if the port is accessed in v86-mode and SB IEQ isn't virtualized:
 *  - [psp:86h] = -1      activates SB irq virtualization
 *  - [psp:86h] = 0020h deactivates SB irq virtualization
 */

void PTRAP_SetPICPortTrap( int bSet )
/////////////////////////////////////
{
    /* might be called even if support for v86 is disabled */
    if ( ptrap.QPI_regs.x.cs ) {
#if RMPICTRAPDYN
        ptrap.QPI_regs.x.dx = 0x20;
        if ( bSet ) {
            ptrap.QPI_regs.x.ax = 0x1A09; /* trap */
            PortState[PICIndex] |= PDT_FLGS_RMINST;
        } else {
            ptrap.QPI_regs.x.ax = 0x1A0A; /* untrap */
            PortState[PICIndex] &= ~PDT_FLGS_RMINST;
        }
        __dpmi_simulate_real_mode_procedure_retf(&ptrap.QPI_regs); /* trap port */
#else
        /* patch the 16-bit real-mode code stored in the PSP;
         * see rmcode1.asm, wPICp.
         */
        struct rmcode1 *dosmem = NearPtr(_my_psp() + DOSMEMSTART);
        //WriteLinearW( dosmem, bSet ? 0xffff : 0x0020 );
        dosmem->wPort = (bSet ? 0xffff : 0x0020);
#endif
    }
    return;
}

/*
 * init real-mode port trapping.
 * This isn't called if /RM0 has been set or QPI API hasn't been found!
 */

bool PTRAP_Init_RM()
////////////////////
{
    static __dpmi_regs TrapHandlerREG; /* static RMCS for RMCB */
#if HANDLE_IN_388H_DIRECTLY || !RMPICTRAPDYN
    struct rmcode1 *dosmem;
#endif

    ptrap.QPI_regs.x.ax = 0x1A06;
    /* get current trap handler */
    if(__dpmi_simulate_real_mode_procedure_retf(&ptrap.QPI_regs) != 0 || (ptrap.QPI_regs.x.flags & CPU_CFLAG))
        return false;
    ptrap.QPI_OldCallback.v86.offset  = ptrap.QPI_regs.x.di;
    ptrap.QPI_OldCallback.v86.segment = ptrap.QPI_regs.x.es;
    dbgprintf(("PTRAP_Init_RM: QPI old callback=%x:%x\n", ptrap.QPI_OldCallback.v86.segment, ptrap.QPI_OldCallback.v86.segment));

    /* install realmode callback for realmode port trapping */
    if ( _init_rmcbIO( &RM_TrapHandler, &TrapHandlerREG, &ptrap.rmcb ) == 0 )
        return false;

#if HANDLE_IN_388H_DIRECTLY || !RMPICTRAPDYN
    /* copy 16-bit code to DOS memory (PSP:60h) */
    dosmem = NearPtr(_my_psp() + DOSMEMSTART);
    dosheap = copyrmcode( (void *)dosmem, 0 );

    /* the code starts with a rmcode1 struct, now to be initialized...  */
    dosmem->rmcb.segofs = ptrap.rmcb.segofs;
# if !RMPICTRAPDYN
    dosmem->qpi.v86.offset  = ptrap.QPI_regs.x.ip;
    dosmem->qpi.v86.segment = ptrap.QPI_regs.x.cs;
# endif
    /* set new QPI v86-mode trap handler (in ES:DI) */
    ptrap.QPI_regs.x.di = offsetof(struct rmcode1, codev86);
    ptrap.QPI_regs.x.es = (_my_psp() + DOSMEMSTART) >> 4;
#else
    ptrap.QPI_regs.x.di = ptrap.rmcb.v86.offset;
    ptrap.QPI_regs.x.es = ptrap.rmcb.v86.segment;
#endif
    ptrap.QPI_regs.x.ax = 0x1A07; /* set trap handler */
    if( __dpmi_simulate_real_mode_procedure_retf(&ptrap.QPI_regs) != 0 || (ptrap.QPI_regs.x.flags & CPU_CFLAG))
        return false;
    return true;
}

/* install a range of port traps using QPI */

static int Install_RM_PortTrapRange( struct PortRange_s *pr, int idx )
//////////////////////////////////////////////////////////////////////
{
    int port;
    unsigned int v;

    for( port = pr->start, v = pr->portmap; v; port++, v >>= 1 ) {
        if ( v & 1 ) {
            if ( ptrap.QPI_OldCallback.v86.segment ) {
                /* this is unreliable, since if the port was already trapped, there's no
                 * guarantee that the previous handler can actually handle it.
                 * so it might be safer to ignore the old state and - on exit -
                 * untrap the port in any case!
                 */
                ptrap.QPI_regs.x.ax = 0x1A08; /* get port status */
                ptrap.QPI_regs.x.dx = port;
                __dpmi_simulate_real_mode_procedure_retf(&ptrap.QPI_regs);
                PortState[idx] |= (ptrap.QPI_regs.h.bl) << 8; //previously trapped state
            }
            //dbgprintf(("PTRAP_Install_RM_PortRangeTrap: port=%X\n", i ));
            ptrap.QPI_regs.x.ax = 0x1A09; /* trap port */
            ptrap.QPI_regs.x.dx = port;
            __dpmi_simulate_real_mode_procedure_retf(&ptrap.QPI_regs); /* trap port */
#if RMPICTRAPDYN
            if ( port == 0x20 ) PICIndex = idx;
#endif
            PortState[idx++] |= PDT_FLGS_RMINST;
        }
    }
    return idx;
}

/* install all port trap ranges */

bool PTRAP_Install_PortTraps( int bRM, int bPM )
////////////////////////////////////////////////
{
    int i;
    struct _hdpmi_traphandler traphandler;

#ifdef NOTFLAT
    traphandler.ofsIn  = (uint16_t)&SwitchStackIOIn;
    traphandler.ofsOut = (uint16_t)&SwitchStackIOOut;
#else
    traphandler.ofsIn  = (uint32_t)&SwitchStackIOIn;
    traphandler.ofsOut = (uint32_t)&SwitchStackIOOut;
#endif

    for ( i = 0, ptrap.cntports = 0; i < ptrap.cntranges; i++ ) {
        dbgprintf(("PTRAP_Install_PortTraps: range[%u]: ports %X-%X\n", i, portranges[i].start, portranges[i].end));
        if ( bRM )
            ptrap.cntports = Install_RM_PortTrapRange( &portranges[i], ptrap.cntports );
        if ( bPM )
            if (!(portranges[i].traphdl = _hdpmi_install_trap( portranges[i].start,
                                                              portranges[i].end - portranges[i].start + 1,
                                                              &traphandler )))
                return false;
    }

    if ( bPM ) {
        /* reset hdpmi=32 option in case it is set */
        _hdpmi_set_context_mode( 0 );
#ifndef NOTFLAT
        /* install CLI handler */
        _hdpmi_set_cli_handler( _hdpmi_CliHandler );
#endif
    }

    dbgprintf(("PTRAP_Install_PortTraps: cntranges=%u cntports=%u\n", ptrap.cntranges, ptrap.cntports ));
    return true;
}

bool PTRAP_Uninstall_PortTraps( int bRM, int bPM )
//////////////////////////////////////////////////
{
    int i,k;

    dbgprintf(("PTRAP_Uninstall_PortTraps( %u, %u )\n", bRM, bPM ));
    if ( bPM ) {
        for ( i = 0; i < ptrap.cntranges; i++ )
            if ( portranges[i].traphdl )
                _hdpmi_uninstall_trap( portranges[i].traphdl );
#ifndef NOTFLAT
        /* uninstall CLI trap handler */
        _hdpmi_set_cli_handler( NULL );
#endif
    }

    for( i = 0, k = 0; i < ptrap.cntranges; i++ ) {
        unsigned int v;
        int port;
        for( port = portranges[i].start, v = portranges[i].portmap; v; port++, v >>= 1 ) {
            if ( v & 1 ) {
                if ( !( PortState[k] & 0xff00 )) {
                    if( PortState[k] & PDT_FLGS_RMINST ) {
                        ptrap.QPI_regs.x.ax = 0x1A0A; /* clear port trap */
                        ptrap.QPI_regs.x.dx = port;
                        __dpmi_simulate_real_mode_procedure_retf(&ptrap.QPI_regs);
                        PortState[k] &= ~PDT_FLGS_RMINST;
                        //dbgprintf(("PTRAP_Uninstall_RM_PortTraps: port %X untrapped\n", port ));
                    }
                }
                k++;
            }
        }
    }
    ptrap.QPI_regs.x.ax = 0x1A07; /* set trap handler */
    ptrap.QPI_regs.x.di = ptrap.QPI_OldCallback.v86.offset;
    ptrap.QPI_regs.x.es = ptrap.QPI_OldCallback.v86.segment;
    if( __dpmi_simulate_real_mode_procedure_retf(&ptrap.QPI_regs) != 0) //restore old handler
        return false;

    /* uninstall realmode callback for realmode port trapping */
    _exit_rmcbIO( &ptrap.rmcb );

    return true;
}


/* add a port range */

int PTRAP_AddRange( int start, unsigned int portmap, const PORT_TRAP_HANDLER *ptfuncs )
///////////////////////////////////////////////////////////////////////////////////////
{
    unsigned int end;
    unsigned int v;
    int i = ptrap.cntranges;
    dbgprintf(("PTRAP_AddRange(start=0x%X, portmap=0x%X) cntranges=%u\n", start, portmap, ptrap.cntranges ));

    if ( i >= countof(portranges) ) {
        dbgprintf(("PTRAP_AddRange: ERROR, range table overflow\n" ));
        return 0;
    }

    for ( end = start, v = portmap; v; end++, v >>= 1 );
    portranges[i].start = start;
    portranges[i].end = end - 1;
    portranges[i].portmap = portmap;
    portranges[i].ptfuncs = ptfuncs;
    ptrap.cntranges++;
    return 1;
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

#if PT0V86

/* v1.8: get physical address of v86 pagetab 0;
 * this is implemented by an addition to QPIEMU - it
 * won't work for Qemm.
 * Usually address translations in conv. memory are handled
 * by the v86 monitor - but in v86 mode only. In protected-mode,
 * for a VCPI client like HDPMI there's no (fast) method to do this.
 */

uint32_t PTRAP_GetPageTab0v86( void )
/////////////////////////////////////
{
    if ( ptrap.QPI_regs.x.cs ) {
        ptrap.QPI_regs.x.ax = 0x5000;
        __dpmi_simulate_real_mode_procedure_retf(&ptrap.QPI_regs);
        if ( 0 == ( ptrap.QPI_regs.x.flags & 1 ) )
            return ( ptrap.QPI_regs.d.edx );
    }
    return 0;
}
#endif

