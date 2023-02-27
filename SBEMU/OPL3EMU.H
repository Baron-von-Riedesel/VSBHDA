#ifndef _OPL3EMU_H_
#define _OPL3EMU_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void OPL3EMU_Init(int samplerate);
//get mode set by client. 0: OPL2, other:OPL3
int OPL3EMU_GetMode();
int OPL3EMU_GenSamples(int16_t* pcm16, int count);

uint32_t OPL3EMU_OPL3_388(uint32_t port, uint32_t val, uint32_t out);
uint32_t OPL3EMU_OPL3_389(uint32_t port, uint32_t val, uint32_t out);
uint32_t OPL3EMU_OPL3_38A(uint32_t port, uint32_t val, uint32_t out);
uint32_t OPL3EMU_OPL3_38B(uint32_t port, uint32_t val, uint32_t out);



#ifdef __cplusplus
}
#endif


#endif//_OPL3EMU_H_