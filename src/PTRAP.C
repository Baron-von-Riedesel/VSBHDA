
/* port trapping
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
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
#if VMPU
#include "VMPU.H"
#endif

#define DOSMEMSTART 0x60 /* offset in PSP, bits 0-3 must be zero */

// next 2 defines must match EQUs in rmcode1.asm!
#define HANDLE_IN_388H_DIRECTLY 0
#define RMPICTRAPDYN 0 /* 1=trap PIC for v86-mode dynamically when needed */

extern struct globalvars gvars;
uint32_t _hdpmi_rmcbIO( void(*Fn)( __dpmi_regs *), __dpmi_regs *reg, __dpmi_raddr * );
void _hdpmi_CliHandler( void );
void SwitchStackIOIn(  void );
void SwitchStackIOOut( void );

static __dpmi_regs QPI_regs;   /* used for QPI access (either Qemm's or QPIEMU's) */
static __dpmi_raddr QPI_OldCallback;
static __dpmi_raddr rmcb;      /* realmode callback used to handle trapped port access in v86 mode */

static int maxports;
static int maxranges;
#if RMPICTRAPDYN
static int PICIndex;
#endif
#if HANDLE_IN_388H_DIRECTLY || !RMPICTRAPDYN
extern void * copyrmcode( void *, int );
void * dosheap;
#endif

static uint32_t traphdl[8+1]; /* hdpmi32i trap handles */


struct HDPMIAPI_ENTRY HDPMIAPI_Entry; /* vendor API entry */

void    (*UntrappedIO_OUT_Handler)(uint16_t port, uint8_t value) = (void (*)(uint16_t, uint8_t))&outp;
uint8_t (*UntrappedIO_IN_Handler)(uint16_t port) = (uint8_t (*)(uint16_t))&inp;

static const uint8_t ChannelPageMap[] = { 0x87, 0x83, 0x81, 0x82, -1, 0x8b, 0x89, 0x8a };

#define OPL3_PDT  0
#define MPIC_PDT  1
#define SPIC_PDT  2
#define DMA_PDT   3
#define DMAPG_PDT 4
#if SB16
#define HDMA_PDT  5
#define SB_PDT    6
#define MPU_PDT   7
#else
#define SB_PDT    5
#define MPU_PDT   6
#endif

static uint16_t PortTable[] = {
	0x388, 0x389, 0x38A, 0x38B | 0x8000,
	0x20, 0x21 | 0x8000,
	0xA1 | 0x8000,
	0x02, 0x03,                   /* ch 1; will be modified if LDMA != 1 */
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F | 0x8000,
#if SB16
	0x83, 0x8B | 0x8000,          /* ch 1 & ch 5: page regs */
#else
	0x83 | 0x8000,
#endif
#if SB16
	0xC4, 0xC6,                   /* ch 5; will be modified if HDMA != 5 */
	0xD0, 0xD2, 0xD4, 0xD6, 0xD8, 0xDA, 0xDC, 0xDE | 0x8000,
#endif
	0x220, 0x221, 0x222, 0x223, /* FM */
	0x224, 0x225, 0x226,
	0x228, 0x229, /* FM */
	0x22A, 0x22C,
	0x22E, 0x22F | 0x8000,
#if VMPU
	0x330, 0x331 | 0x8000,
#endif
    0xffff
};


/* PortHandler array must match port array */
static PORT_TRAP_HANDLER PortHandler[] = {
	VOPL3_388, VOPL3_389, VOPL3_38A, VOPL3_38B,
	VPIC_Acc, VPIC_Acc,    /* 0x20, 0x21 */
	VPIC_Acc,              /* 0xA1 */
	VDMA_Acc, VDMA_Acc,    /* base+cnt for ch 1; will be modified if LDMA != 1 */
	VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, /* 0x08-0x0F */
	VDMA_Acc,              /* page reg for ch 1; will be modified if LDMA != 1 */
#if SB16
	VDMA_Acc,              /* page reg for ch 5; will be modified if HDMA != 5 */
#endif
#if SB16
	VDMA_Acc, VDMA_Acc,    /* base+cnt for ch 5; will be modified if HDMA != 5 */
	VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, /* 0xD0-0xDE */
#endif
	VOPL3_388, VOPL3_389, VOPL3_38A, VOPL3_38B, /* 0x220-0x223 */
	VSB_MixerAddr, VSB_MixerData,               /* 0x224-0x225 */
	VSB_DSP_Reset,                              /* 0x226 */
	VOPL3_388, VOPL3_389,                       /* 0x228, 0x229 */
	VSB_DSP_Acc0A, VSB_DSP_Acc0C,               /* 0x22a, 0x22c */
	VSB_DSP_Acc0E, VSB_DSP_Acc0F,               /* 0x22e, 0x22f */
#if VMPU
	VMPU_Acc, VMPU_Acc,
#endif
};

static uint16_t PortState[countof(PortHandler)];

/* hdpmi is restricted to 8 port ranges */
static int portranges[8+1];

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
    for ( i = 0; i < maxports; i++ ) {
        if( PortTable[i] == port ) {
            regs->h.al = PortHandler[i]( port, regs->h.al, regs->x.cx );
            regs->x.flags &= ~CPU_CFLAG; /* clear carry flag, indicates that access was handled */
            return;
        }
    }

    /* this should never be reached. */

    dbgprintf(("RM_TrapHandler: unhandled port=%x val=%x out=%x (OldCB=%x:%x)\n", regs->x.dx, regs->h.al, regs->h.cl, QPI_OldCallback.v86.segment, QPI_OldCallback.v86.offset ));
#if 0
    if ( QPI_OldCallback.v86.segment ) {
        __dpmi_regs r = *regs;
        r.x.ip = QPI_OldCallback.v86.offset;
        r.x.cs = QPI_OldCallback.v86.segment;
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

uint32_t PTRAP_PM_TrapHandler( uint16_t port, uint32_t flags, uint32_t value )
//////////////////////////////////////////////////////////////////////////////
{
    int i;
    for( i = 0; i < maxports; i++ )
        if( PortTable[i] == port) {
            return PortHandler[i](port, value, flags );
        }

    /* ports that are trapped, but not handled; this may happen, since
     * hdpmi32i's support for port trapping is limited to 8 ranges.
     */
    if ( flags & TRAPF_OUT) {
        UntrappedIO_OUT( port, value );
        return value;
    } else
        return UntrappedIO_IN( port ) | (value &= ~0xff);
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
            QPI_regs.x.ip = entryfar & 0xFFFF;
            QPI_regs.x.cs = entryfar >> 16;
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
            QPI_regs.x.ip = r.x.di;
            QPI_regs.x.cs = r.x.es;
        }
    }
#endif
    /* if Qemm hasn't been found, try Jemm's QPIEMU ... */
    if ( QPI_regs.x.cs == 0 ) {
        /* QPIEMU installation check;
         * getting the entry point of QPIEMU is non-trivial in protected-mode, since
         * the int 2Fh must be executed as interrupt ( not just "simulated" ). Here
         * a small ( 3 bytes ) helper proc is constructed on the fly, at PSP:005Ch:
         * a INT 2Fh, followed by an RETF.
         */
        uint32_t *dosmem = NearPtr(_my_psp() + 0x5C);
        *dosmem = 0xCB2FCD;  /* INT 2Fh & IRET */
        r.x.ax = 0x1684;
        r.x.bx = 0x4354;
        r.x.cs = _my_psp() >> 4;
        r.x.ip = 0x5C;
        if( __dpmi_simulate_real_mode_procedure_retf(&r) != 0 || r.h.al )
            return 0;
        QPI_regs.x.ip = r.x.di;
        QPI_regs.x.cs = r.x.es;
    }
    QPI_regs.h.ah = 0x03; /* get version */
    if( __dpmi_simulate_real_mode_procedure_retf(&QPI_regs) == 0 ) {
        return QPI_regs.x.ax;
    }
    return 0;
}

/* v1.6: extracted from PTRAP_Prepare_RM_PortTrap() because that function may be optional.
 * Variables maxports, maxranges, portranges[] and PortTable[] are initialized.
 */

void PTRAP_InitPortMax( void )
//////////////////////////////
{
    int i, j;
    /* setup port ranges */
    for ( i = 0, j = 1, portranges[0] = 0; PortTable[i] != 0xffff; i++ ) {
        if ( PortTable[i] & 0x8000 ) {
            portranges[j] = i+1;
            PortTable[i] &= 0x7fff;
            j++;
        }
    }
    maxports = i;
    maxranges = j - 1;
}

/*
 * Prepare real-mode port trapping.
 * This isn't called if /RM0 has been set or QPI API hasn't been found!
 */

#if HANDLE_IN_388H_DIRECTLY || !RMPICTRAPDYN

struct rmcode1 {   /* structure must match definitions in rmcode1.asm! */
    uint32_t rmcb; /* realmode callback */
    uint16_t data; /* port 0x388/0x389 access optimization (not active) */
    uint16_t wPort; /* used for PIC port trapping; contains either 0x0020 or 0xffff */
    uint32_t qpi;  /* QPI entry */
    uint8_t codev86[]; /* v86 code */
};

#endif

bool PTRAP_Prepare_RM_PortTrap()
////////////////////////////////
{
    static __dpmi_regs TrapHandlerREG; /* static RMCS for RMCB */
#if HANDLE_IN_388H_DIRECTLY || !RMPICTRAPDYN
    struct rmcode1 *dosmem;
#endif

    QPI_regs.x.ax = 0x1A06;
    /* get current trap handler */
    if(__dpmi_simulate_real_mode_procedure_retf(&QPI_regs) != 0 || (QPI_regs.x.flags & CPU_CFLAG))
        return false;
    QPI_OldCallback.v86.offset  = QPI_regs.x.di;
    QPI_OldCallback.v86.segment = QPI_regs.x.es;
    dbgprintf(("PTRAP_Prepare_RM_PortTrap: old callback=%x:%x\n",QPI_OldCallback.v86.segment, QPI_OldCallback.v86.segment));

    /* get a realmode callback */
    if ( _hdpmi_rmcbIO( &RM_TrapHandler, &TrapHandlerREG, &rmcb ) == 0 )
        return false;

#if HANDLE_IN_388H_DIRECTLY || !RMPICTRAPDYN
    /* copy 16-bit code to DOS memory (PSP:60h) */
    dosmem = NearPtr(_my_psp() + DOSMEMSTART);
    dosheap = copyrmcode( (void *)dosmem, 0 );

    /* the code starts with a rmcode1 struct, now to be initialized...  */
    dosmem->rmcb = rmcb.segofs;
#if !RMPICTRAPDYN
    dosmem->qpi = (QPI_regs.x.cs << 16) | QPI_regs.x.ip;
#endif
    /* set new trap handler ES:DI */
    //r.x.di = 4+2+2+4;
    QPI_regs.x.di = offsetof(struct rmcode1, codev86);
    QPI_regs.x.es = (_my_psp() + DOSMEMSTART) >> 4;
#else
    QPI_regs.x.di = rmcb.v86.offset;
    QPI_regs.x.es = rmcb.v86.segment;
#endif
    QPI_regs.x.ax = 0x1A07; /* set trap handler */
    if( __dpmi_simulate_real_mode_procedure_retf(&QPI_regs) != 0 || (QPI_regs.x.flags & CPU_CFLAG))
        return false;
    return true;
}

/* install a range of port traps using QPI */

static bool Install_RM_PortRangeTrap( uint16_t start, uint16_t end )
////////////////////////////////////////////////////////////////////
{
    int i;

    for( i = start; i < end; i++ ) {
        if ( QPI_OldCallback.v86.segment ) {
            /* this is unreliable, since if the port was already trapped, there's no
             * guarantee that the previous handler can actually handle it.
             * so it might be safer to ignore the old state and - on exit -
             * untrap the port in any case!
             */
            QPI_regs.x.ax = 0x1A08; /* get port status */
            QPI_regs.x.dx = PortTable[i] & 0x7fff;
            __dpmi_simulate_real_mode_procedure_retf(&QPI_regs);
            PortState[i] |= (QPI_regs.h.bl) << 8; //previously trapped state
        }
        QPI_regs.x.ax = 0x1A09; /* trap port */
        QPI_regs.x.dx = PortTable[i] & 0x7fff;
        __dpmi_simulate_real_mode_procedure_retf(&QPI_regs); /* trap port */
        PortState[i] |= PDT_FLGS_RMINST;
    }
    return true;
}

/* install all real-mode port trap ranges */

bool PTRAP_Install_RM_PortTraps( void )
///////////////////////////////////////
{
    int i;

    dbgprintf(("PTRAP_Install_RM_PortTraps: maxports=%u, maxranges=%u\n", maxports, maxranges ));
    for ( i = 0; i < maxranges; i++ ) {
        dbgprintf(("PTRAP_Install_RM_PortTraps: range[%u]: ports %X-%X\n", i, PortTable[portranges[i]], PortTable[portranges[i+1]-1] ));
#if RMPICTRAPDYN
        if ( PortTable[portranges[i]] == 0x20 ) {
            PICIndex = portranges[i];
            continue;
        }
#endif
        Install_RM_PortRangeTrap( portranges[i], portranges[i+1] );
    }
    return true;
}

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
    if ( QPI_regs.x.cs ) {
#if RMPICTRAPDYN
        QPI_regs.x.dx = PDispTab[PICIndex].port;
        if ( bSet ) {
            QPI_regs.x.ax = 0x1A09; /* trap */
            PortState[PICIndex] |= PDT_FLGS_RMINST;
        } else {
            QPI_regs.x.ax = 0x1A0A; /* untrap */
            PortState[PICIndex] &= ~PDT_FLGS_RMINST;
        }
        __dpmi_simulate_real_mode_procedure_retf(&QPI_regs); /* trap port */
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

bool PTRAP_Uninstall_RM_PortTraps( void )
/////////////////////////////////////////
{
    int i;

    for( i = 0; i < maxports; ++i ) {
        if ( !( PortState[i] & 0xff00 )) {
            if( PortState[i] & PDT_FLGS_RMINST ) {
                QPI_regs.x.ax = 0x1A0A; /* clear port trap */
                QPI_regs.x.dx = PortTable[i];
                __dpmi_simulate_real_mode_procedure_retf(&QPI_regs);
                PortState[i] &= ~PDT_FLGS_RMINST;
                //dbgprintf(("PTRAP_Uninstall_RM_PortTraps: port %X untrapped\n", PortTable[i] ));
            }
        }
    }
    QPI_regs.x.ax = 0x1A07; /* set trap handler */
    QPI_regs.x.di = QPI_OldCallback.v86.offset;
    QPI_regs.x.es = QPI_OldCallback.v86.segment;
    if( __dpmi_simulate_real_mode_procedure_retf(&QPI_regs) != 0) //restore old handler
        return false;

    __dpmi_free_real_mode_callback( &rmcb );

    return true;
}

bool PTRAP_DetectHDPMI()
////////////////////////
{
    uint8_t result = _get_hdpmi_vendor_api(&HDPMIAPI_Entry);

#if 0 //JHDPMI
	__dpmi_regs r = {0};
	uint32_t *dosmem = NearPtr(_my_psp() + 0x5C);
	*dosmem = 0xCB2FCD; /* INT 2Fh & RETF */
	r.x.ax = 0x1684;
	r.x.bx = 0x4858;
	r.x.cs = _my_psp() >> 4;
	r.x.ip = 0x5C;
	if( __dpmi_simulate_real_mode_procedure_retf(&r) == 0 && r.h.al == 0 )
		jhdpmi = 1;
#endif

	return (result == 0 && HDPMIAPI_Entry.seg);
}

static uint32_t PTRAP_Int_Install_PM_Trap( int start, int end, void(*handlerIn)(void), void(*handlerOut)(void) )
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    struct _hdpmi_traphandler traphandler;
#ifdef NOTFLAT
    traphandler.ofsIn  = (uint16_t)handlerIn;
    traphandler.ofsOut = (uint16_t)handlerOut;
#else
    traphandler.ofsIn  = (uint32_t)handlerIn;
    traphandler.ofsOut = (uint32_t)handlerOut;
#endif
    return _hdpmi_install_trap( start, end - start + 1, &traphandler );
}

#if 0//def _DEBUG
void PTRAP_PrintPorts( void )
/////////////////////////////
{
    int start = 0;
    int i;
    dbgprintf(( "PTRAP_PrintPorts:\n" ));
    for ( i = 0; i < maxports; i++ ) {
        if ( i == ( maxports - 1 ) || ( PortTable[i+1] != PortTable[i]+1 || PortState[i+1] != PortState[i] ) ) {
            if ( i == start )
                dbgprintf(( "%X (%X)\n", PortTable[start], PortState[start] ));
            else
                dbgprintf(( "%X-%X (%X)\n", PortTable[start], PortTable[i], PortState[start] ));
            start = i + 1;
        }
    }
    return;
}
#endif

bool PTRAP_Install_PM_PortTraps( void )
///////////////////////////////////////
{
    int i;
    int start, end;

    /* reset hdpmi=32 option in case it is set */
    _hdpmi_set_context_mode( 0 );

#ifndef NOTFLAT
    /* install CLI handler */
    _hdpmi_set_cli_handler( _hdpmi_CliHandler );
#endif
    for ( i = 0; i < maxranges; i++ ) {
        if ( portranges[i+1] > portranges[i] ) { /* skip if range is empty */
            start = PortTable[portranges[i]];
            end = PortTable[portranges[i+1] - 1];
            dbgprintf(("PTRAP_Install_PM_PortTraps: %X-%X\n", start, end ));
            if (!(traphdl[i] = PTRAP_Int_Install_PM_Trap( start, end, &SwitchStackIOIn, &SwitchStackIOOut)))
                return false;
        }
    }
#if 0//def _DEBUG
    PTRAP_PrintPorts();
#endif
    return true;
}

/* delete 1-x entries in PortTable[] and PortHandler[], adjust port ranges */

static void PDT_DelEntries( int start, int end, int entries )
/////////////////////////////////////////////////////////////
{
    int i;
    for ( i = start; i < end - entries; i++ ) {
        PortTable[i] = PortTable[i + entries];
        PortHandler[i] = PortHandler[i + entries];
    }
    maxports -= entries;
    for ( i = 0; i <= maxranges; i++ ) {
        if ( portranges[i] > start ) {
            portranges[i] -= entries;
        }
    }
}

/* adjust PortTable[] and PortHandler[] to current settings of /D, /H, /A, /OPL
 * note: sndirq is the irq of the real sound hardware!
 */

void PTRAP_Prepare( int opl, int sbaddr, int dma, int hdma, int sndirq )
////////////////////////////////////////////////////////////////////////
{
    int i;
    dbgprintf(("PTRAP_Prepare: opl=%X, sb=%X, dma=%X, hdma=%X)\n", opl, sbaddr, dma, hdma ));
    /* low dma: adjust the entry for DMA channel addr/count */
    PortTable[portranges[DMA_PDT] + 0] = dma * 2;
    PortTable[portranges[DMA_PDT] + 1] = dma * 2 + 1;
    /* low dma: adjust the entry for DMA page reg */
    PortTable[portranges[DMAPG_PDT]] = ChannelPageMap[ dma ];
    /* if the sound hw IRQ is < 8, the slave PIC doesn't need to be trapped */
    if ( sndirq < 8 ) {
        PDT_DelEntries( portranges[SPIC_PDT], maxports, 1 );
    }
#if SB16
    if ( hdma ) {
        /* high dma: adjust the entry for DMA channel addr/count */
        PortTable[portranges[HDMA_PDT] + 0] = hdma * 4 + (0xC0-0x10);
        PortTable[portranges[HDMA_PDT] + 1] = hdma * 4 + 2 + (0xC0-0x10);
        /* high dma: adjust the entry for DMA page reg */
        PortTable[portranges[DMAPG_PDT] + 1] = ChannelPageMap[ hdma ];
    } else {
        /* if no SB16 emulation, remove all HDMA ports */
        PDT_DelEntries( portranges[DMAPG_PDT] + 1, maxports, 1 );
        PDT_DelEntries( portranges[HDMA_PDT], maxports, portranges[HDMA_PDT+1] - portranges[HDMA_PDT] );
    }
#endif
#if VMPU
    if ( gvars.mpu ) {
        PortTable[portranges[MPU_PDT] + 0] = gvars.mpu;
        PortTable[portranges[MPU_PDT] + 1] = gvars.mpu + 1;
    } else {
        PDT_DelEntries( portranges[MPU_PDT], maxports, 2 );
    }
#endif
    /* adjust the SB ports to the selected base */
    if ( sbaddr != 0x220 )
        for( i = portranges[SB_PDT]; i < portranges[SB_PDT+1]; i++ )
            PortTable[i] += sbaddr - 0x220;

    /* if no OPL3 emulation, skip ports 0x388-0x38b, 0x220-0x223 and 0x228-0x229 */
    if ( !opl ) {
        PDT_DelEntries( portranges[OPL3_PDT], maxports, 4 );
        PDT_DelEntries( portranges[SB_PDT], maxports, 4 );
        /* v1.8: also remove ports 0x228-0x229; +3 to skip ports 0x224,0x225,0x226 */
        PDT_DelEntries( portranges[SB_PDT]+3, maxports, 2 );
    }

    /* delete empty port ranges */
    for ( i = 0; i < maxranges; i++ ) {
        if ( 0 == portranges[i+1] - portranges[i] ) {
            int j;
            for ( j = i; j < maxranges; j++) {
                portranges[j] = portranges[j+1];
            }
            maxranges--;
        }
    }

#ifdef _DEBUG
    dbgprintf(("PTRAP_Prepare: maxports=%u, maxranges=%u\n", maxports, maxranges ));
    for( i = 0; i < maxranges; i++ ) {
        dbgprintf(("PTRAP_Prepare: range[%u]: ports %X-%X\n", i, PortTable[portranges[i]], PortTable[portranges[i+1]-1] ));
    }
#endif

}

bool PTRAP_Uninstall_PM_PortTraps( void )
/////////////////////////////////////////
{
    int i;
    for ( i = 0; traphdl[i]; i++ )
        _hdpmi_uninstall_trap( traphdl[i] );

#ifndef NOTFLAT
    /* uninstall CLI trap handler */
    _hdpmi_set_cli_handler( NULL );
#endif

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

#if PT0V86

/* v1.8: get physical address of v86 pagetab 0;
 * this is implemented by an addition to QPIEMU - it
 * won't work for Qemm.
 */

uint32_t PTRAP_GetPageTab0v86( void )
/////////////////////////////////////
{
    if ( QPI_regs.x.cs ) {
        QPI_regs.x.ax = 0x5000;
        __dpmi_simulate_real_mode_procedure_retf(&QPI_regs);
        if ( 0 == ( QPI_regs.x.flags & 1 ) )
            return ( QPI_regs.d.edx );
    }
    return 0;
}
#endif

