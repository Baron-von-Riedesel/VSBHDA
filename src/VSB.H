#ifndef _VSB_H_
#define _VSB_H_

#include <stdint.h>

#include "SB.H"

#ifdef __cplusplus
extern "C"
{
#endif

void    VSB_Init(int addr, int irq, int dma, int hdma, int DSPVer, void *hAU );
uint8_t VSB_GetIRQ( void );
int     VSB_GetDMA( void );
int     VSB_Running( void ); /* DSP op running? ( either DMA cmd or "Emit Silence") */
void    VSB_Stop( void );

int     VSB_IsSigned( void );
int     VSB_IsSilent( void );
int     VSB_IsAuto( void );
int     VSB_GetChannels( void );
int     VSB_GetSampleRate( void );
unsigned int VSB_GetBits( void );
uint32_t VSB_GetBuffSpace( void );
void    VSB_ReduceBuffSpace( uint32_t bytes ); /* reduce space in bytes */
void    VSB_ResetBuffSpace( void ); /* autoinit: reset buffer space */
int     VSB_GetIRQStatus( void );
uint8_t VSB_GetMixerReg( uint8_t index );
int     VSB_ReadDirectSamples( uint8_t * );

#ifdef __cplusplus
}
#endif

#endif//_VSB_H_
