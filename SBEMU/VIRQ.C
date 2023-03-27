
#include "SBEMUCFG.H"
#include "PIC.H"
#include "DPMI_.H"
#include "UNTRAPIO.H"
#include "VIRQ.H"
#include "QEMM.H"
#include <dos.h>

#define CHANGEPICMASK 1 /* 1=mask all IRQs during irq 5/7 */

static int VIRQ_Irq = -1;
static uint8_t VIRQ_ISR[2];
static uint8_t VIRQ_OCW[2];

#define VIRQ_IS_VIRTUALIZING() (VIRQ_Irq != -1)

static void VIRQ_Write(uint16_t port, uint8_t value)
////////////////////////////////////////////////////
{
    //dbgprintf("VIRQW:%x,%x\n",port,value);
    if(VIRQ_IS_VIRTUALIZING())
    {
        dbgprintf("VIRQW:%x,%x\n",port,value);
        if((port&0x0F) == 0x00)
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
        dbgprintf("VIRQR:%x\n",port);
        if((port&0x0F) == 0x00) {
            int index = ((port==0x20) ? 0 : 1);
            if(VIRQ_OCW[index] == 0x0B) { //ISR
                return VIRQ_ISR[index];
            }
        }
        //return 0;
    }
    return UntrappedIO_IN(port);
}

void VIRQ_Invoke(uint8_t irq )
//////////////////////////////
{

#if QEMMPICTRAPDYN
	QEMM_SetPICPortTrap( 1 );
#endif


#if CHANGEPICMASK
    int mask = PIC_GetIRQMask();
    PIC_SetIRQMask(0xFFFF);
#endif
    VIRQ_ISR[0] = VIRQ_ISR[1] = 0;
    if(irq < 8) //master
        VIRQ_ISR[0] = 1 << irq;
    else //slave
    {
        VIRQ_ISR[0] = 0x04; //cascade
        VIRQ_ISR[1] = 1 << (irq-8);
    }
    
    VIRQ_Irq = irq;
#if 0
    /* this approach makes HDPMI call the "previous" real-mode handler, that is the handler
     * that was installed when HDPMI has been launched. That's why this strategy isn't successful here.
     */
    static DPMI_REG r = {0};
    DPMI_CallRealModeINT(PIC_IRQ2VEC(irq), &r);
#elif 0
    /* this approach works so long as HDPMI=1 is NOT set. It has the other disadvantage that, if a protected-mode
     * handler is installed, 4 extra mode switches are triggered (PM->RM->RMCB->RM->PM).
     */
    static DPMI_REG r = {0};
    int n = PIC_IRQ2VEC(irq);
    r.w.ip = DPMI_LoadW(n*4);
    r.w.cs = DPMI_LoadW(n*4+2);
    DPMI_CallRealModeIRET(&r);
#else
    /*
     * A general problem: if the interrupt is handled in real mode, EOIs sent to the PICs cannot be trapped!
     */
    if(irq == 7)
        asm("int $0x0F");
    else
        asm("int $0x0D");
#endif

    VIRQ_Irq = -1;
#if !SETIF
	//CLI(); /* the ISR should have run a STI! So disable interrupts again before the masks are restored */
	asm("mov $0x900, %ax\n\t" "int $0x31" );
#endif
#if CHANGEPICMASK
    PIC_SetIRQMask(mask);  /* restore masks */
#endif
#if QEMMPICTRAPDYN
	QEMM_SetPICPortTrap( 0 );
#endif
    return;
}

uint32_t VIRQ_IRQ(uint32_t port, uint32_t val, uint32_t out)
////////////////////////////////////////////////////////////
{
    return out ? (VIRQ_Write(port, val), val) : (val &=~0xFF, val |= VIRQ_Read(port));
}


