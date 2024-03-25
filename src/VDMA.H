#ifndef _VDMA_H_
#define _VDMA_H_
//ISA DMA virtualization
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

uint32_t VDMA_DMA(uint32_t port, uint32_t val, uint32_t out);
//void VDMA_Write(uint16_t port, uint8_t byte);
//uint8_t VDMA_Read(uint16_t port);

void VDMA_Virtualize(int channel, int enable);
uint32_t VDMA_GetAddress(int channel);
int32_t VDMA_GetCounter(int channel);
uint32_t VDMA_GetIndex(int channel);
uint32_t VDMA_SetIndexCounter(int channel, uint32_t index, int32_t counter);
int VDMA_GetAuto(int channel);
int VDMA_GetWriteMode(int channel);
void VDMA_WriteData(int channel, unsigned char data );
//void VDMA_ToggleComplete(int channel);

#ifdef __cplusplus
}
#endif

#endif//_VDMA_H_
