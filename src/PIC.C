
//reference: https://wiki.osdev.org/PIC

#include <dos.h>
#include <stdint.h>
#include <stdbool.h>
#ifndef DJGPP
#include <conio.h>
#endif

#include "PLATFORM.H"
#include "PIC.H"
#include "PTRAP.H"

//master PIC
#define PIC_PORT1 0x20
#define PIC_DATA1 0x21
//slave PIC
#define PIC_PORT2 0xA0
#define PIC_DATA2 0xA1

#define PIC_READISR 0x0B    //read interrupte service register (current interrupting IRQ)

//#define inp UntrappedIO_IN
//#define outp UntrappedIO_OUT

void PIC_SendEOIWithIRQ(uint8_t irq)
////////////////////////////////////
{
//    if(irq == 7 || irq == 15) //check spurious irq
//        return PIC_SendEOI();
    if(irq >= 8)
        UntrappedIO_OUT(PIC_PORT2, 0x20);
    UntrappedIO_OUT(PIC_PORT1, 0x20);
}

//#undef inp
//#undef outp
//#define inp inp
//#define outp outp

void PIC_UnmaskIRQ(uint8_t irq)
///////////////////////////////
{
    uint16_t port = PIC_DATA1;
    if(irq >= 8)
    {
        uint8_t master = inp(port);
        if(master & 0x4)
            outp(port, (uint8_t)(master & ~0x4)); //unmask slave
        port = PIC_DATA2;
        irq = (uint8_t)(irq - 8);
    }
    outp(port, (uint8_t)(inp(port)&~( 1 << irq )));
}

uint16_t PIC_GetIRQMask(void)
/////////////////////////////
{
    uint16_t mask = (uint16_t)((inp(PIC_DATA2) << 8) | inp(PIC_DATA1));
    return mask;
}

void PIC_SetIRQMask(uint16_t mask)
//////////////////////////////////
{
    outp(PIC_DATA1, (uint8_t)mask);
    outp(PIC_DATA2, (uint8_t)(mask >> 8));
}
