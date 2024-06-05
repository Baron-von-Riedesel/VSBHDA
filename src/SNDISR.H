#ifndef _SNDISR_H_
#define _SNDISR_H_
//sound IRQ
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

//int  SNDISR_Interrupt( void );
bool SNDISR_Init( int hAudio, uint16_t vol );
bool SNDISR_Exit( void );

#ifdef __cplusplus
}
#endif

#endif
