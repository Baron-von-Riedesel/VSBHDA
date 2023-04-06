
#include <stdbool.h>
#include <dos.h>

#include "SBEMUCFG.H"
#include "PIC.H"
#include "DPMI_.H"
#include "UNTRAPIO.H"
#include "VIRQ.H"
#include "QEMM.H"
#include "SBEMU.H"

#define CHANGEPICMASK 1 /* 1=mask all IRQs during irq 5/7 */

static int VIRQ_Irq = -1;
static uint8_t VIRQ_ISR[2];
static uint8_t VIRQ_OCW[2];

#define VIRQ_IS_VIRTUALIZING() (VIRQ_Irq != -1)

void SafeCall( uint8_t irq );
void FastCall( uint8_t irq );

void (* CallIRQ)(uint8_t) = &FastCall;

static void VIRQ_Write(uint16_t port, uint8_t value)
////////////////////////////////////////////////////
{
    if(VIRQ_IS_VIRTUALIZING())
    {
        dbgprintf("VIRQ_Write:%x,%x\n",port,value);
        if((port & 0x0F) == 0x00)
        {
            int index = ((port==0x20) ? 0 : 1);
            VIRQ_OCW[index] = value;

            if(value == 0x20) //EOI: clear ISR
            {
                VIRQ_ISR[index] = 0;
                return; //don't send to real PIC. it's virtualized
            }

            if(value == 0x0B) //read ISR
                return; //don't send to real PIC, will be handled in VIRQ_Read.
        }
        return;
    }

    UntrappedIO_OUT(port, value);
}

static uint8_t VIRQ_Read(uint16_t port)
///////////////////////////////////////
{
    if(VIRQ_IS_VIRTUALIZING())
    {
        dbgprintf("VIRQ_Read:%x\n",port);
        if((port & 0x0F) == 0x00) {
            int index = ((port == 0x20) ? 0 : 1);
            if(VIRQ_OCW[index] == 0x0B) { //ISR
                return VIRQ_ISR[index];
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
void SafeCall( uint8_t irq )
////////////////////////////
{
    static DPMI_REG r = {0};
    int n = PIC_IRQ2VEC(irq);
    r.w.ip = DPMI_LoadW(n*4);
    r.w.cs = DPMI_LoadW(n*4+2);
    DPMI_CallRealModeIRET(&r);
}

/*
 * FastCall.
 * Pro: fast, no unneeded mode switches for protected-mode handlers.
 * Con: may not work if both a real-mode and protected-mode handler have been set.
 */
void FastCall( uint8_t irq )
////////////////////////////
{
    if(irq == 7)
        asm("int $0x0F");
    else
        asm("int $0x0D");
}

void VIRQ_Invoke( void )
////////////////////////
{
    uint8_t irq;

    dbgprintf("VIRQ_Invoke\n");
    irq = SBEMU_GetIRQ();

#if CHANGEPICMASK
    int mask = PIC_GetIRQMask();
    PIC_SetIRQMask(0xFFFF);
#endif

    QEMM_SetPICPortTrap( 1 );

    VIRQ_ISR[0] = VIRQ_ISR[1] = 0;
    if(irq < 8) //master
        VIRQ_ISR[0] = 1 << irq;
    else //slave
    {
        VIRQ_ISR[0] = 0x04; //cascade
        VIRQ_ISR[1] = 1 << (irq-8);
    }
    
    VIRQ_Irq = irq;
    CallIRQ(irq);
    VIRQ_Irq = -1;
#if !SETIF
    _disable_ints(); /* the ISR should have run a STI! So disable interrupts again before the masks are restored */
#endif
    QEMM_SetPICPortTrap( 0 );
#if CHANGEPICMASK
    PIC_SetIRQMask(mask);  /* restore masks */
#endif
    return;
}

void VIRQ_SafeCall( void )
//////////////////////////
{
    CallIRQ = &SafeCall;
}

uint32_t VIRQ_IRQ(uint32_t port, uint32_t val, uint32_t out)
////////////////////////////////////////////////////////////
{
    return out ? (VIRQ_Write(port, val), val) : (val &=~0xFF, val |= VIRQ_Read(port));
}


