#ifndef _VMPU_H_
#define _VMPU_H_

#ifdef __cplusplus
extern "C"
{
#endif

void VMPU_Init( int freq );
void VMPU_Exit( void );
uint32_t VMPU_MPU(uint32_t port, uint32_t val, uint32_t out);
void VMPU_SBMidi_RawWrite( uint8_t value );
#if SOUNDFONT
void VMPU_Process_Messages( void );
#endif

#ifdef __cplusplus
}
#endif

#endif//_VMPU_H_
