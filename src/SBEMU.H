#ifndef _SBEMU_H_
#define _SBEMU_H_

#include <stdint.h>

#include "SB.H"

#ifdef __cplusplus
extern "C"
{
#endif

#if !TRIGGERATONCE
extern int SBEMU_TriggerIRQ;
#endif

//generic IO functions
uint32_t SBEMU_SB_MixerAddr(uint32_t port, uint32_t val, uint32_t out);
uint32_t SBEMU_SB_MixerData(uint32_t port, uint32_t val, uint32_t out);
uint32_t SBEMU_SB_DSP_Reset(uint32_t port, uint32_t val, uint32_t out);
uint32_t SBEMU_SB_DSP_Read(uint32_t port, uint32_t val, uint32_t out);
uint32_t SBEMU_SB_DSP_Write(uint32_t port, uint32_t val, uint32_t out);
uint32_t SBEMU_SB_DSP_ReadStatus(uint32_t port, uint32_t val, uint32_t out);
uint32_t SBEMU_SB_DSP_ReadINT16BitACK(uint32_t port, uint32_t val, uint32_t out);

//used by emulations
void SBEMU_Init(int irq, int dma, int hdma, int DSPVer );
uint8_t SBEMU_GetIRQ();
uint8_t SBEMU_GetDMA();
uint8_t SBEMU_GetHDMA();
int SBEMU_Running();
void SBEMU_Stop();

int SBEMU_GetDACSpeaker();
unsigned int SBEMU_GetBits();
int SBEMU_GetChannels();
int SBEMU_GetSampleRate();
int SBEMU_GetSampleBytes();
int SBEMU_GetAuto();
int SBEMU_GetPos(); //get pos in bytes
int SBEMU_SetPos(int pos); //set pos in bytes
//int SBEMU_IRQTriggered();
//void SBEMU_ResetTriggeredIRQ();
uint8_t SBEMU_GetMixerReg(uint8_t index);
#if ADPCM
int SBEMU_DecodeADPCM(uint8_t* adpcm, int bytes); //decode in place
#endif

#ifdef __cplusplus
}
#endif

#endif//_SBEMU_H_
