
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

static int VIRQ_Irq = -1;
static uint8_t VPIC_ISR[2];
static uint8_t VPIC_OCW[2];

static uint16_t OrgCS;

#define IRQ_IS_VIRTUALIZED() (VIRQ_Irq != -1)

static void SafeCall( uint8_t irq );
static void FastCall( uint8_t irq );

static void (* CallIRQ)(uint8_t) = &FastCall;

static void VPIC_Write(uint16_t port, uint8_t value)
////////////////////////////////////////////////////
{
    if( IRQ_IS_VIRTUALIZED() ) {
        //dbgprintf(("VPIC_Write:%x,%x\n",port,value));
        if((port & 0x0F) == 0x00) {
            int index = ((port==0x20) ? 0 : 1);
            VPIC_OCW[index] = value;

            if(value == 0x20) { //EOI: clear ISR
                VPIC_ISR[index] = 0;
#if !CHANGEPICMASK
                VIRQ_Irq = -1; /* virtualize just once */
                PTRAP_SetPICPortTrap( 0 );
#endif
                return; //don't send to real PIC. it's virtualized
            }

            if(value == 0x0B) //read ISR
                return; //don't send to real PIC, will be handled in VIRQ_Read.
        }
        return;
    }

    UntrappedIO_OUT(port, value);
}

static uint8_t VPIC_Read(uint16_t port)
///////////////////////////////////////
{
    if( IRQ_IS_VIRTUALIZED() ) {
        //dbgprintf(("VPIC_Read:%x\n",port));
        if( (port & 0x0F) == 0x00) {
            int index = ((port == 0x20) ? 0 : 1);
            if (VPIC_OCW[index] == 0x0B) { //ISR
                return VPIC_ISR[index];
            }
        }
        return 0;
    }
    return UntrappedIO_IN(port);
}

/* SafeCall: this approach works so long as HDPMI=1 is NOT set.
 * Pro: it works if a protected-mode program has installed both a real-mode
 *      AND a protected-mode handler routine.
 * Con: if a protected-mode handler is installed, 4 extra mode switches are
 *      triggered (PM->RM->RMCB->RM->PM).
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

#ifndef DJGPP
void callint0F(void);
void callint0D(void);
#pragma aux callint0F = "int 0Fh" parm[] modify exact[];
#pragma aux callint0D = "int 0Dh" parm[] modify exact[];
#endif

/*
 * FastCall.
 * Pro: fast, no unneeded mode switches for protected-mode handlers.
 * Con: may not work if both a real-mode and protected-mode handler have been set.
 */
static void FastCall( uint8_t irq )
///////////////////////////////////
{
#ifdef DJGPP
    if(irq == 7)
        asm("int $0x0F");
    else
        asm("int $0x0D");
#else
    if(irq == 7)
        callint0F();
    else
        callint0D();
#endif
}

void VIRQ_Invoke( void )
////////////////////////
{
    uint8_t irq;
    int mask;

    dbgprintf(("VIRQ_Invoke\n"));
    irq = VSB_GetIRQ();

#if CHANGEPICMASK
    mask = PIC_GetIRQMask();
    PIC_SetIRQMask(0xFFFF);
#endif

    PTRAP_SetPICPortTrap( 1 );

    /* set values for ports 0x0020/0x00a0 */
    if(irq < 8) {
        VPIC_ISR[0] = 1 << irq;
        VPIC_ISR[1] = 0;
    } else {
        VPIC_ISR[0] = 0x04; //cascade
        VPIC_ISR[1] = 1 << (irq - 8);
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

void VIRQ_SetCallType( void )
/////////////////////////////
{
    /* if IVT 5/7 has been modified, use SafeCall, else use FastCall */
    int n = PIC_IRQ2VEC( VSB_GetIRQ() );
    CallIRQ = ( ReadLinearW(n*4+2) == OrgCS ) ? &FastCall : &SafeCall;
}

void VIRQ_Init( void )
//////////////////////
{
    int n = PIC_IRQ2VEC( VSB_GetIRQ() );
    if ( n < 0x100 )
        OrgCS = ReadLinearW(n*4+2);
    dbgprintf(("VIRQ_Init: int=%X, OrgCS=%X\n", n, OrgCS ));
}

uint32_t VPIC_PIC(uint32_t port, uint32_t val, uint32_t out)
////////////////////////////////////////////////////////////
{
    return out ? (VPIC_Write(port, val), val) : (val &=~0xFF, val |= VPIC_Read(port));
}
