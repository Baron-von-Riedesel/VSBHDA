#ifndef _VOPL3_H_
#define _VOPL3_H_
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void VOPL3_Init(int samplerate);
#if REINITOPL
void VOPL3_Reinit(int samplerate);
#endif
//get mode set by client. 0: OPL2, other:OPL3
int VOPL3_GetMode();
int VOPL3_GenSamples(int16_t* pcm16, int count);

#ifndef NOFM
uint8_t VOPL3_388(uint16_t port, uint8_t val, uint16_t out);
uint8_t VOPL3_389(uint16_t port, uint8_t val, uint16_t out);
uint8_t VOPL3_38A(uint16_t port, uint8_t val, uint16_t out);
uint8_t VOPL3_38B(uint16_t port, uint8_t val, uint16_t out);
#else
static inline uint8_t VOPL3_388(uint16_t port, uint8_t val, uint16_t out) { return val; }
static inline uint8_t VOPL3_389(uint16_t port, uint8_t val, uint16_t out) { return val; }
static inline uint8_t VOPL3_38A(uint16_t port, uint8_t val, uint16_t out) { return val; }
static inline uint8_t VOPL3_38B(uint16_t port, uint8_t val, uint16_t out) { return val; }
#endif
bool VOPL3_IsActive( void );

#ifdef __cplusplus
}
#endif


#endif//_VOPL3_H_
