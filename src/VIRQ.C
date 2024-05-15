
/* VPIC & VIRQ */

#include <stdint.h>
#include <stdbool.h>
#include <dos.h>

#include "CONFIG.H"
#include "PIC.H"
#include "LINEAR.H"
#include "PTRAP.H"
#include "VIRQ.H"
#include "VSB.H"

#define CHANGEPICMASK 0 /* 1=mask all IRQs during irq 5/7 */

#if JHDPMI
extern int jhdpmi;
#endif

extern void FastCall( uint8_t irq );

struct VPic_s {
	int bIrq; /* real sound hw irq */
	uint8_t ISR[2];
	uint8_t OCW[2];
};

static struct VPic_s pic;

static int VIRQ_Irq = -1;

static uint16_t OrgCS;

#define IRQ_IS_VIRTUALIZED() (VIRQ_Irq != -1)

static void SafeCall( uint8_t irq );

static void (* CallIRQ)(uint8_t) = &FastCall;

static void VPIC_Write(uint16_t port, uint8_t value)
////////////////////////////////////////////////////
{
	/* refuse to mask the real sound hw irq
	 * if IRQ is >= 8, ports 0x21 and 0xA1 are trapped;
	 * else, just 0x21 is trapped.
	 */
	if ( port & 1 ) {
		if ( port == 0xA1 || ( pic.bIrq < 8 ) )
			value &= ~(1 << (pic.bIrq & 7));
		else {
			value &= ~(1 << 2);  /* mask bit 2 of MPIC */
		}
		UntrappedIO_OUT(port, value);
		return;
	}

    if( IRQ_IS_VIRTUALIZED() ) {
        //dbgprintf(("VPIC_Write:%x,%x\n",port,value));
		int index = ((port == 0x20) ? 0 : 1);
		pic.OCW[index] = value;

		if( value == 0x20 ) { //EOI: clear ISR
			pic.ISR[index] = 0;
#if !CHANGEPICMASK
			VIRQ_Irq = -1; /* virtualize just once */
			PTRAP_SetPICPortTrap( 0 );
#endif
			return; //don't send to real PIC. it's virtualized
		}
#if 0
		else if(value == 0x0B) //read ISR
			return; //don't send to real PIC, will be handled in VIRQ_Read.
#endif
	}

    UntrappedIO_OUT(port, value);
}

static uint8_t VPIC_Read(uint16_t port)
///////////////////////////////////////
{
    uint8_t rc = UntrappedIO_IN(port);
    if( IRQ_IS_VIRTUALIZED() ) {
        int index = ((port == 0x20) ? 0 : 1);
        if (pic.OCW[index] == 0x0B) { //ISR
            rc |= pic.ISR[index];
        }
        //dbgprintf(("VPIC_Read(%x)=%x\n",port,rc));
    }
    return rc;
}

uint32_t VPIC_Acc(uint32_t port, uint32_t val, uint32_t out)
////////////////////////////////////////////////////////////
{
    return out ? (VPIC_Write(port, val), val) : (val &=~0xFF, val |= VPIC_Read(port));
}

void VPIC_Init( uint8_t hwirq )
///////////////////////////////
{
    pic.bIrq = hwirq;
}

/* SafeCall: this approach works so long as HDPMI=1 is NOT set.
 * Pro: it works if a protected-mode program has installed both a real-mode
 *      AND a protected-mode handler routine.
 * Con: - if a protected-mode handler is installed, 4 extra mode switches are
 *      triggered (PM->RM->RMCB->RM->PM).
 *      - the values of the real-mode segment registers (ds,es,fs,gs) are unknown.
 *
 * FastCall:
 * Pro: - fast, no unneeded mode switches for protected-mode handlers.
 *      - the real-mode segment registers (ss,ds,es,fs,gs) are restored automatically
 *        in case the interrupt is routed to real-mode.
 * Con: if JHDPMI is loaded, there are no cons, else:
 *      - may not work if both a real-mode and protected-mode handler have been set.
 * since v1.5, FastCall is implemented as assembly code. This allows to switch to
 * the client context (ss:esp, ds, es) when the IRQ is emulated.
 */

static void SafeCall( uint8_t irq )
///////////////////////////////////
{
    static __dpmi_regs r = {0};
    int n = PIC_IRQ2VEC(irq);
    r.x.flags = 0x0002;
    r.x.ip = ReadLinearW(n*4+0);
    r.x.cs = ReadLinearW(n*4+2);
    __dpmi_simulate_real_mode_procedure_iret(&r);
}

void VIRQ_Invoke( void )
////////////////////////
{
    uint8_t irq;
#if CHANGEPICMASK
    int mask;
#endif

    //dbgprintf(("VIRQ_Invoke\n"));
    irq = VSB_GetIRQ();

#if CHANGEPICMASK
    mask = PIC_GetIRQMask();
    PIC_SetIRQMask(0xFFFF);
#endif

    PTRAP_SetPICPortTrap( 1 );

    /* set values for ports 0x0020/0x00a0 */
    if(irq < 8) {
        pic.ISR[0] = 1 << irq;
        pic.ISR[1] = 0;
    } else {
        pic.ISR[0] = 0x04; //cascade
        pic.ISR[1] = 1 << (irq - 8);
    }
    
    VIRQ_Irq = irq;
    CallIRQ(irq);
#if !SETIF
    _disable_ints(); /* the ISR should have run a STI! So disable interrupts again before the masks are restored */
#endif
    if( IRQ_IS_VIRTUALIZED() ) {
        VIRQ_Irq = -1;
        PTRAP_SetPICPortTrap( 0 );
    }
#if CHANGEPICMASK
    PIC_SetIRQMask(mask);  /* restore masks */
#endif
    return;
}

/* set emulated IRQ call type depending on what's found at IVT 5/7
 */

void VIRQ_SetCallType( uint8_t irq )
////////////////////////////////////
{
    /* if IVT 5/7 has been modified, use SafeCall, else use FastCall */
    int n = PIC_IRQ2VEC( irq );
#if JHDPMI
    if (jhdpmi)
        return;
#endif
    CallIRQ = ( ReadLinearW(n*4+2) == OrgCS ) ? &FastCall : &SafeCall;
}

void VIRQ_Init( uint8_t virq )
//////////////////////////////
{
    int n = PIC_IRQ2VEC( virq );
#if JHDPMI
    if (jhdpmi)
        return;
#endif
    OrgCS = ReadLinearW(n*4+2);
    dbgprintf(("VIRQ_Init(%u): int=%X, OrgCS=%X\n", virq, n, OrgCS ));
}

