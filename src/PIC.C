
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

void PIC_SendEOI(uint8_t irq)
/////////////////////////////
{
//    if(irq == 7 || irq == 15) //check spurious irq
//        return PIC_SendEOI();
    if(irq >= 8)
        UntrappedIO_OUT(PIC_PORT2, 0x20);
    UntrappedIO_OUT(PIC_PORT1, 0x20);
}

uint16_t PIC_GetIRQMask(void)
/////////////////////////////
{
    return ( (uint16_t)( UntrappedIO_IN( PIC_DATA2 ) << 8) | UntrappedIO_IN( PIC_DATA1 ) );
}

void PIC_SetIRQMask(uint16_t mask)
//////////////////////////////////
{
    UntrappedIO_OUT( PIC_DATA1, (uint8_t)mask);
    UntrappedIO_OUT( PIC_DATA2, (uint8_t)(mask >> 8));
}

void PIC_UnmaskIRQ(uint8_t irq)
///////////////////////////////
{
    uint16_t mask = PIC_GetIRQMask();
    mask &= ~(1 << irq);
    if ( irq >= 8 )
        mask &= ~(1 << 2);
    PIC_SetIRQMask( mask );
}

