#ifndef _VIRQ_H_
#define _VIRQ_H_
//IRQ virtualization
//https://wiki.osdev.org/8259_PIC
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

uint8_t VPIC_Acc(uint16_t port, uint8_t val, uint16_t out);
void VPIC_Init( uint8_t );

void VIRQ_Invoke( void );
//int  VIRQ_GetSndIrq( void );
void VIRQ_WaitForSndIrq( void );
void VIRQ_Init( uint8_t );
void VIRQ_Exit( uint8_t );
#if !HOSTRT
void VIRQ_Check( void );
#endif

#ifdef __cplusplus
}
#endif

#endif//_VIRQ_H_
