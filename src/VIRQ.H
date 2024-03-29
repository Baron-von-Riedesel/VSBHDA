#ifndef _VIRQ_H_
#define _VIRQ_H_
//IRQ virtualization
//https://wiki.osdev.org/8259_PIC
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t VPIC_PIC(uint32_t port, uint32_t val, uint32_t out);
//void VPIC_Write(uint16_t port, uint8_t value);
//uint8_t VPIC_Read(uint16_t port);

void VIRQ_Invoke( void );
void VIRQ_Init( void );
void VIRQ_SetCallType( void );

#ifdef __cplusplus
}
#endif

#endif//_VIRQ_H_
