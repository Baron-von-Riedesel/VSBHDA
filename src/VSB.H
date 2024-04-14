#ifndef _VSB_H_
#define _VSB_H_

#include <stdint.h>

#include "SB.H"

#ifdef __cplusplus
extern "C"
{
#endif

#if !TRIGGERATONCE
extern int VSB_TriggerIRQ;
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
uint32_t VSB_MixerAddr(uint32_t port, uint32_t val, uint32_t out);
uint32_t VSB_MixerData(uint32_t port, uint32_t val, uint32_t out);
uint32_t VSB_DSP_Reset(uint32_t port, uint32_t val, uint32_t out);
uint32_t VSB_DSP_Read(uint32_t port, uint32_t val, uint32_t out);
uint32_t VSB_DSP_Write(uint32_t port, uint32_t val, uint32_t out);
uint32_t VSB_DSP_ReadStatus(uint32_t port, uint32_t val, uint32_t out);
uint32_t VSB_DSP_ReadINT16BitACK(uint32_t port, uint32_t val, uint32_t out);

void VSB_Init(int irq, int dma, int hdma, int DSPVer );
uint8_t VSB_GetIRQ( void );
int VSB_GetDMA( void );
//int VSB_GetHDMA( void );
int VSB_Running( void );
void VSB_Stop( void );

int VSB_GetDACSpeaker( void );
unsigned int VSB_GetBits( void );
int VSB_IsSigned( void );
int VSB_IsSilent( void );
int VSB_GetChannels( void );
int VSB_GetSampleRate( void );
uint32_t VSB_GetSampleBufferSize( void );
uint32_t VSB_GetPos( void ); //get pos in bytes
uint32_t VSB_SetPos(uint32_t pos); //set pos in bytes
int VSB_GetAuto( void );
void VSB_SetIRQStatus( void );
//int VSB_IRQTriggered( void );
void VSB_ResetTriggeredIRQ( void );
uint8_t VSB_GetMixerReg(uint8_t index);
int VSB_GetDirectCount(uint8_t * *);
void VSB_ResetDirectCount( void );

#ifdef __cplusplus
}
#endif

#endif//_VSB_H_
