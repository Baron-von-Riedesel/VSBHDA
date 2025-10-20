#ifndef _VSB_H_
#define _VSB_H_

#include <stdint.h>

#include "SB.H"

#ifdef __cplusplus
extern "C"
{
#endif

#if ADPCM
typedef struct 
{
    int step;
    uint8_t ref;
    uint8_t useRef;
} ADPCM_STATE;
#endif

//generic IO functions
uint8_t VSB_MixerAddr(uint16_t port, uint8_t val, uint16_t flags);
uint8_t VSB_MixerData(uint16_t port, uint8_t val, uint16_t flags);
uint8_t VSB_DSP_Reset(uint16_t port, uint8_t val, uint16_t flags);
uint8_t VSB_DSP_Acc0A(uint16_t port, uint8_t val, uint16_t flags);
uint8_t VSB_DSP_Acc0C(uint16_t port, uint8_t val, uint16_t flags);
uint8_t VSB_DSP_Acc0E(uint16_t port, uint8_t val, uint16_t flags);
uint8_t VSB_DSP_Acc0F(uint16_t port, uint8_t val, uint16_t flags);

void VSB_Init(int irq, int dma, int hdma, int DSPVer, void *hAU );
uint8_t VSB_GetIRQ( void );
int VSB_GetDMA( void );
//int VSB_GetHDMA( void );
int VSB_Running( void );
void VSB_Stop( void );

//int VSB_GetDACSpeaker( void );
unsigned int VSB_GetBits( void );
int VSB_IsSigned( void );
int VSB_IsSilent( void );
int VSB_GetChannels( void );
int VSB_GetSampleRate( void );
uint32_t VSB_GetSampleBufferSize( void );
uint32_t VSB_GetPos( void ); //get pos in bytes
uint32_t VSB_SetPos(uint32_t pos); //set pos in bytes
int VSB_GetAuto( void );
void VSB_SetIRQStatus( uint8_t );
int VSB_GetIRQStatus( void );
uint8_t VSB_GetMixerReg(uint8_t index);
int VSB_ReadDirectSamples(uint8_t *);

#ifdef __cplusplus
}
#endif

#endif//_VSB_H_
