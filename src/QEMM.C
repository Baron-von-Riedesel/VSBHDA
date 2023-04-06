#include <stdlib.h>
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
#include "SBEMUCFG.h"
#include "QEMM.h"
#include "DPMI_.H"
#include "UNTRAPIO.H"

// next 2 defines must match EQUs in rmwrap.asm!
#define HANDLE_IN_388H_DIRECTLY 1
#define RMPICTRAPDYN 0 /* trap PIC for v86-mode dynamically when needed */

uint32_t _hdpmi_rmcbIO( void(*Fn)( DPMI_REG*), DPMI_REG* reg, __dpmi_raddr * );

static __dpmi_raddr QEMM_Entry;

//static bool QEMM_InCallback;
static uint16_t QEMM_OldCallbackIP;
static uint16_t QEMM_OldCallbackCS;
static __dpmi_raddr rmcb;

static QEMM_IODT *pIodt;
static int maxports;
static int PICIndex;

#if HANDLE_IN_388H_DIRECTLY || !RMPICTRAPDYN
extern void QEMM_RM_Wrapper( void );
extern void QEMM_RM_WrapperEnd( void );
#endif

static void QEMM_TrapHandler( DPMI_REG * regs)
//////////////////////////////////////////////
{
    uint16_t port = regs->w.dx;
    uint8_t val = regs->h.al;
    uint8_t out = regs->h.cl;

	for ( int i = 0; i < maxports; i++ ) {
		if( (pIodt+i)->port == port) {
			regs->w.flags &= ~CPU_CFLAG;
			//uint8_t val2 = (pIodt+i)->handler( port, val, out );
			//regs->h.al = out ? regs->h.al : val2;
			regs->h.al = (pIodt+i)->handler( port, val, out );
			return;
		}
    }

	/* this should never be reached. */

	dbgprintf("QEMM_TrapHandler: unhandled port=%x val=%x out=%x (OldCB=%x:%x)\n", port, val, out, QEMM_OldCallbackCS, QEMM_OldCallbackIP );
	//regs->w.flags |= CPU_CFLAG;
	if ( QEMM_OldCallbackCS ) {
		DPMI_REG r = *regs;
		r.w.cs = QEMM_OldCallbackCS;
		r.w.ip = QEMM_OldCallbackIP;
		DPMI_CallRealModeRETF(&r);
		regs->w.flags |= r.w.flags & CPU_CFLAG;
		regs->h.al = r.h.al;
	}
}

//https://www.cs.cmu.edu/~ralf/papers/qpi.txt
//https://fd.lod.bz/rbil/interrup/memory/673f_cx5145.html
//http://mirror.cs.msu.ru/oldlinux.org/Linux.old/docs/interrupts/int-html/rb-7414.htm

uint16_t QEMM_GetVersion(void)
//////////////////////////////
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
        if(count != 4)
            return 0;
        r.w.cs = entryfar >> 16;
        r.w.ip = entryfar & 0xFFFF;
    } else {
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
    QEMM_Entry.offset16 = r.w.ip;
    QEMM_Entry.segment  = r.w.cs;
    if( DPMI_CallRealModeRETF(&r) == 0 )
        return r.w.ax;
    return 0;
}

bool QEMM_Prepare_IOPortTrap()
//////////////////////////////
{
	static DPMI_REG TrapHandlerREG; /* static RMCS for RMCB */
    DPMI_REG r = {0};
    r.w.ip = QEMM_Entry.offset16;
    r.w.cs = QEMM_Entry.segment;
    r.w.ax = 0x1A06;
    /* get current trap handler */
    if(DPMI_CallRealModeRETF(&r) != 0 || (r.w.flags & CPU_CFLAG))
        return false;
    QEMM_OldCallbackIP = r.w.es;
    QEMM_OldCallbackCS = r.w.di;
    dbgprintf("QEMM_Prepare_IOPortTrap: old callback=%x:%x\n",r.w.es, r.w.di);

    /* get a realmode callback */
    if ( _hdpmi_rmcbIO( &QEMM_TrapHandler, &TrapHandlerREG, &rmcb ) == 0 )
        return false;

#if HANDLE_IN_388H_DIRECTLY || !RMPICTRAPDYN
    /* copy 16-bit code to DOS memory
     * bytes 0-3 are the realmode callback
     * bytes 4-5 are used as data
     */
    uint32_t codesize = (void *)&QEMM_RM_WrapperEnd - (void *)&QEMM_RM_Wrapper;
    uint32_t dosmem = _go32_info_block.linear_address_of_original_psp + 0x80;
    DPMI_CopyLinear( dosmem, DPMI_PTR2L(&rmcb), 4 );
    DPMI_StoreD( dosmem + 4, 0 );
#if !RMPICTRAPDYN
    DPMI_CopyLinear( dosmem + 8, DPMI_PTR2L(&QEMM_Entry), 4 );
#endif
    DPMI_CopyLinear( dosmem + 4 + 4 + 4, DPMI_PTR2L( &QEMM_RM_Wrapper ), codesize );

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


static bool QEMM_Install_IOPortTrap(QEMM_IODT iodt[], uint16_t start, uint16_t end )
////////////////////////////////////////////////////////////////////////////////////
{
	DPMI_REG r = {0};

	r.w.ip = QEMM_Entry.offset16;
	r.w.cs = QEMM_Entry.segment;
	for( int i = start; i < end; i++ ) {
		if ( QEMM_OldCallbackCS ) {
			/* this is unreliable, since if the port was already trapped, there's no
			 * guarantee that the previous handler can actually handle it.
			 * so it might be safer to ignore the old state and - on exit -
			 * untrap the port in any case!
			 */
			r.w.ax = 0x1A08;
			r.w.dx = iodt[i].port;
			DPMI_CallRealModeRETF(&r);
			iodt[i].flags |= (r.h.bl) << 8; //previously trapped state
		}
		r.w.ax = 0x1A09;
		r.w.dx = iodt[i].port;
		DPMI_CallRealModeRETF(&r); /* trap port */
		iodt[i].flags |= IODT_FLGS_RMINST;
    }
    return true;
}

bool QEMM_Install_PortTraps( QEMM_IODT iodt[], int Rangetab[], int max )
////////////////////////////////////////////////////////////////////////
{
	pIodt = iodt;
	maxports = Rangetab[max];

	for ( int i = 0; i < max; i++ ) {
#if RMPICTRAPDYN
		if ( iodt[Rangetab[i]].port == 0x20 ) {
			PICIndex = Rangetab[i];
			continue;
		}
#endif
		QEMM_Install_IOPortTrap( iodt, Rangetab[i], Rangetab[i+1] );
	}
    return 1;
}

void QEMM_SetPICPortTrap( int bSet )
////////////////////////////////////
{
	/* might be called even if support for v86 is disabled */
	if ( QEMM_Entry.segment ) {
#if RMPICTRAPDYN
		DPMI_REG r = {0};
		r.w.ip = QEMM_Entry.offset16;
		r.w.cs = QEMM_Entry.segment;
		r.w.dx = (pIodt+PICIndex)->port;
		if ( bSet ) {
			r.w.ax = 0x1A09; /* trap */
			(pIodt+PICIndex)->flags |= IODT_FLGS_RMINST;
		} else {
			r.w.ax = 0x1A0A; /* untrap */
			(pIodt+PICIndex)->flags &= ~IODT_FLGS_RMINST;
		}
		DPMI_CallRealModeRETF(&r); /* trap port */
#else
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

bool QEMM_Uninstall_PortTraps(QEMM_IODT* iodt, int max )
////////////////////////////////////////////////////////
{
	DPMI_REG r = {0};

	r.w.ip = QEMM_Entry.offset16;
	r.w.cs = QEMM_Entry.segment;
	for(int i = 0; i < max; ++i) {
		if ( !( iodt[i].flags & 0xff00 )) {
			if( iodt[i].flags & IODT_FLGS_RMINST ) {
				r.w.ax = 0x1A0A; //clear trap
				r.w.dx = iodt[i].port;
				DPMI_CallRealModeRETF(&r);
				iodt[i].flags &= ~IODT_FLGS_RMINST;
				//dbgprintf("QEMM_Uninstall_PortTraps: port %X untrapped\n", iodt[i].port );
			}
		}
    }
	r.w.ax = 0x1A07;
	r.w.es = QEMM_OldCallbackCS;
	r.w.di = QEMM_OldCallbackIP;
	if( DPMI_CallRealModeRETF(&r) != 0) //restore old handler
		return false;

	DPMI_FreeRMCB( &rmcb );

    return true;
}

