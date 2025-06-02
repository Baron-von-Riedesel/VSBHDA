
/* VPIC & VIRQ */

#include <stdint.h>
#include <stdbool.h>
#include <dos.h>

#include "CONFIG.H"
#include "PLATFORM.H"
#include "PIC.H"
#include "LINEAR.H"
#include "PTRAP.H"
#include "VIRQ.H"
#include "VSB.H"

/* MASKSBIRQ: 1=SB irq is masked while VIRQ_Invoke() is running.
 * This approach has issues if a PCI card is using the SB irq, because an IRQ
 * may happen while VIRQ_Invoke() is NOT running. So, either
 * a) mask real SB IRQ permanently (for IRQ2, that's not a good idea) OR
 * b) hook SB IRQ and ensure that the client cannot set/get the SB IRQ vector.
 * Option b) is implemented by setting HOSTRT=0 in sbisr.asm - should
 * be changed to a cmdline option.
 */
#define MASKSBIRQ 1

extern bool _SB_InstallISR( uint8_t, void *, int );
extern bool _SB_UninstallISR( uint8_t );
extern void SBIsrCall( void );

extern void *dosheap;

/* index 0=master, 1=slave */
struct VPic_s {
    uint8_t ISR[2];  /* ISRs */
    uint8_t OCW[2];  /* OCW2s */
    uint8_t Mask[2]; /* IMRs */
    uint8_t bIrq; /* real sound hw irq */
};

static struct VPic_s vpic;

int8_t VIRQ_Irq = -1;

#define IRQ_IS_VIRTUALIZED() (VIRQ_Irq != -1)

static void VPIC_Write(uint16_t port, uint8_t value)
////////////////////////////////////////////////////
{
    /* port 0x21/0xA1? */
    if ( port & 1 ) {
        vpic.Mask[(port == 0xA1) ? 1 : 0] = value;

        /* refuse to mask the real sound hw irq
         * if that irq is >= 8, ports 0x21 and 0xA1 are trapped;
         * else, just 0x21 is trapped.
         */
        if ( port == 0xA1 && vpic.bIrq >= 8 ) {
            value &= ~(1 << (vpic.bIrq - 8) );
        } else if ( port == 0x21 ) {
            if ( vpic.bIrq < 8 )
                value &= ~(1 << vpic.bIrq );
            else
                value &= ~(1 << 2);  /* unmask bit 2 of MPIC */
        }

#if MASKSBIRQ
        /* ensure SB Irq 2/5/7 remains masked while emulated Irq runs. */
        if ( IRQ_IS_VIRTUALIZED() && port == 0x21 )
            value |= ( 1 << VIRQ_Irq );
#endif

    } else if( IRQ_IS_VIRTUALIZED() ) {
        //dbgprintf(("VPIC_Write:%x,%x\n",port,value));
        int index = (port == 0x20) ? 0 : 1; /* currently it's always port 0x20 */
        vpic.OCW[index] = value;
        dbgprintf(("VPIC_Write(%x,%x) vpic.ISR[%u]=%X\n", port, value, index, vpic.ISR[index] ));
        /* v1.6: test just bit 5 of OCW2 for EOI */
        //if( value == 0x20 && vpic.ISR[index] ) {
        if( ( value & 0x20 ) && vpic.ISR[index] ) {
            if ( value & 0x40 ) { /* v1.6: specific EOI? */
                if (( value & 0x7 ) == VIRQ_Irq ) {
                    vpic.ISR[index] = 0;
                    return; //don't send to real PIC if it's virtualized.
                }
            } else {
                /* v1.6: unspecific EOI - check if SB irq bit in ISR is first */
                uint16_t wISR = PIC_GetISR( 0 ) | 0x8000;
                if ( vpic.bIrq < 8 )
                    wISR &= ~(1 << vpic.bIrq );
                else if ( VIRQ_Irq != 2 )
                    wISR &= ~(1 << 2);  /* reset bit 2 of ISR if snd hw IRQ is 8-15 */
                if ( BSF( wISR ) >= VIRQ_Irq ) {
                    vpic.ISR[index] = 0;
                    return; //don't send to real PIC if it's virtualized.
                }
            }
        }
    }

    UntrappedIO_OUT(port, value);
}

static uint8_t VPIC_Read(uint16_t port)
///////////////////////////////////////
{
	uint8_t rc;
	switch ( port ) {
	case 0x21: rc = vpic.Mask[0]; break;
	case 0xA1: rc = vpic.Mask[1]; break;
	case 0x20:
		rc = UntrappedIO_IN(port);
		if( IRQ_IS_VIRTUALIZED() ) {
			if (vpic.OCW[0] == 0x0B) { /* read ISR? */
				rc |= vpic.ISR[0];  /* set SB Irq bit */
				if ( vpic.bIrq < 8 ) /* v1.6: reset real sound hw Irq bit */
					rc &= ~(1 << vpic.bIrq);
				else
					rc &= ~(1 << 2);
			}
			dbgprintf(("VPIC_Read(%x)=%X vpic.ISR[0]=%X\n", port, rc, vpic.ISR[0] ));
		}
		break;
#if 0 /* not needed if SB irq is < 8 */
	case 0xA0:
		rc = UntrappedIO_IN(port);
		if( IRQ_IS_VIRTUALIZED() )
			if (vpic.OCW[1] == 0x0B) //ISR
				rc |= vpic.ISR[1];
		break;
#endif
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
    vpic.bIrq = hwirq;

    vpic.Mask[0] = UntrappedIO_IN(0x21);
    vpic.Mask[1] = UntrappedIO_IN(0xA1);
}

void VIRQ_Invoke( void )
////////////////////////
{
    uint8_t irq;
#if MASKSBIRQ
    uint16_t mask;
#endif

    //dbgprintf(("VIRQ_Invoke\n"));
    irq = VSB_GetIRQ();

#if MASKSBIRQ
    /* mask the real irq 2/5/7. This is done so the SB interrupt won't interfere with a real device
     * using that IRQ. As for IRQ7: since this is the "spurious" irq, it's important that the IRQ7 mask bit
     * read from port 0x21 is 0!
     */
    mask = PIC_GetIRQMask();
    PIC_SetIRQMask( mask | (1 << irq ) );
#endif
    /* set values for ports 0x0020/0x00a0 */
    if(irq < 8) {
        vpic.ISR[0] = 1 << irq;
        vpic.ISR[1] = 0;
    } else {
        vpic.ISR[0] = 0x04; //cascade
        vpic.ISR[1] = 1 << (irq - 8);
    }
    vpic.Mask[0] &= ~(1 << irq); /* unmask the emulated irq 2/5/7 */
    vpic.OCW[0] = vpic.OCW[1] = 0;

    PTRAP_SetPICPortTrap( 1 );
    dbgprintf(("VIRQ_Invoke: calling SBIsrCall\n"));
    VIRQ_Irq = irq;
    SBIsrCall();
    VIRQ_Irq = -1;
    dbgprintf(("VIRQ_Invoke: back from SBIsrCall\n"));
    PTRAP_SetPICPortTrap( 0 );
#if !SETIF
    _disable_ints(); /* the ISR should have run a STI! So disable interrupts again before the masks are restored */
#endif
#if MASKSBIRQ
    PIC_SetIRQMask( mask ); /* restore the PIC mask */
#endif
    return;
}

void VIRQ_Init( uint8_t virq )
//////////////////////////////
{
    _SB_InstallISR( PIC_IRQ2VEC( virq ), dosheap, 2 );
}

void VIRQ_Exit( uint8_t virq )
//////////////////////////////
{
    _SB_UninstallISR( PIC_IRQ2VEC( virq ) );
}

