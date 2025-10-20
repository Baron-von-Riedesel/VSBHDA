#ifndef _VDMA_H_
#define _VDMA_H_
//ISA DMA virtualization
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

uint8_t VDMA_Acc(uint16_t port, uint8_t val, uint16_t flags);

void VDMA_Virtualize(int channel, int enable);
uint32_t VDMA_GetBase(int channel);
uint32_t VDMA_GetIndex(int channel);
int32_t VDMA_GetCount(int channel);
uint32_t VDMA_SetIndexCount(int channel, uint32_t index, int32_t count);
int VDMA_GetAuto(int channel);
int VDMA_GetWriteMode(int channel);
void VDMA_WriteData(int channel, unsigned char data, unsigned char iscb );
//void VDMA_SetComplete(int channel, unsigned char value );

#ifdef __cplusplus
}
#endif

#endif//_VDMA_H_
