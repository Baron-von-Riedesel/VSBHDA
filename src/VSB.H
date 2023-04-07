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

//generic IO functions
uint32_t VSB_MixerAddr(uint32_t port, uint32_t val, uint32_t out);
uint32_t VSB_MixerData(uint32_t port, uint32_t val, uint32_t out);
uint32_t VSB_DSP_Reset(uint32_t port, uint32_t val, uint32_t out);
uint32_t VSB_DSP_Read(uint32_t port, uint32_t val, uint32_t out);
uint32_t VSB_DSP_Write(uint32_t port, uint32_t val, uint32_t out);
uint32_t VSB_DSP_ReadStatus(uint32_t port, uint32_t val, uint32_t out);
uint32_t VSB_DSP_ReadINT16BitACK(uint32_t port, uint32_t val, uint32_t out);

//used by emulations
void VSB_Init(int irq, int dma, int hdma, int DSPVer );
uint8_t VSB_GetIRQ();
uint8_t VSB_GetDMA();
uint8_t VSB_GetHDMA();
int VSB_Running();
void VSB_Stop();

int VSB_GetDACSpeaker();
unsigned int VSB_GetBits();
int VSB_GetChannels();
int VSB_GetSampleRate();
int VSB_GetSampleBytes();
int VSB_GetAuto();
int VSB_GetPos(); //get pos in bytes
int VSB_SetPos(int pos); //set pos in bytes
//int VSB_IRQTriggered();
//void VSB_ResetTriggeredIRQ();
uint8_t VSB_GetMixerReg(uint8_t index);
#if ADPCM
int VSB_DecodeADPCM(uint8_t* adpcm, int bytes); //decode in place
#endif

#ifdef __cplusplus
}
#endif

#endif//_VSB_H_
