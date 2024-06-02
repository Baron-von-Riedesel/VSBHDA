#ifndef _SNDISR_H_
#define _SNDISR_H_
//sound IRQ
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

//int  SNDISR_Interrupt( void );
bool SNDISR_Init( uint8_t intno );
bool SNDISR_Exit( uint8_t intno );

#ifdef __cplusplus
}
#endif

#endif
