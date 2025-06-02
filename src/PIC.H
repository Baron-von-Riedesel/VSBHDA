#ifndef _PIC_H_
#define _PIC_H_
//8259 Programmable Interrupt Controller

//real mode vector mapping (also for DPMI interface)
//although DPMI server might have remapped the PIC, the real mode vector is still used as a compatible interface
//i.e. you can set DPMI interrupt vector (real mode or protected mode) using those number.
#define PIC_IRQ0_VEC 0x08   //PIC master
#define PIC_IRQ8_VEC 0x70   //PIC slave

#define PIC_IRQ2VEC(irq)  ((uint8_t)((irq) < 8 ? PIC_IRQ0_VEC + (irq) : PIC_IRQ8_VEC + ((irq) - 8)))
#define PIC_VEC2IRQ(ivec) ((uint8_t)((ivec) < PIC_IRQ0_VEC + 8 ? (ivec) - 8 : (ivec) - PIC_IRQ8_VEC + 8))

#ifdef __cplusplus
extern "C"
{
#endif

void PIC_SendEOI(uint8_t irq);        //send end of interrupt.
//void PIC_MaskIRQ(uint8_t irq);      //mask an irq line
uint8_t PIC_GetISR(int bSlave);
void PIC_UnmaskIRQ(uint8_t irq);      //unmask an irq line
uint16_t PIC_GetIRQMask(void);
void PIC_SetIRQMask(uint16_t mask);

#define PIC_IS_IRQ_MASKED(mask, irq) (((mask) & (  1 << (irq))))
#define PIC_IRQ_MASK(mask, irq)      (((mask) | (  1 << (irq))))
#define PIC_IRQ_UNMASK(mask, irq)    (((mask) & (~(1 << (irq)))))

#ifdef __cplusplus
}
#endif

#endif//_PIC_H_
