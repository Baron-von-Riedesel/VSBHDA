#ifndef _VDMA_H_
#define _VDMA_H_
//ISA DMA virtualization
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void     VDMA_Virtualize(int channel, int enable);
void     VDMA_PortTrap( int ldma, int hdma );
uint32_t VDMA_GetBase(int channel);
uint32_t VDMA_GetIndex(int channel);
int32_t  VDMA_GetCount(int channel);
uint32_t VDMA_UpdatePos(int channel, uint32_t addbytes);
int      VDMA_IsAuto(int channel);
void     VDMA_WriteData(int channel, unsigned char data, unsigned char iscb );
int      VDMA_IsMasked(int channel);  /* v2.0: now called by VSB_Running() */

#ifdef __cplusplus
}
#endif

#endif//_VDMA_H_
