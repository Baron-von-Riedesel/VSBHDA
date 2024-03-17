#ifndef _SNDISR_H_
#define _SNDISR_H_
//sound IRQ
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

bool SNDISR_InstallISR( uint8_t intno, int(*ISR)(void) );
bool SNDISR_UninstallISR( void );
int  SNDISR_InterruptPM( void );

#ifdef __cplusplus
}
#endif

#endif
