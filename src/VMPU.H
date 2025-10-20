#ifndef _VMPU_H_
#define _VMPU_H_

#ifdef __cplusplus
extern "C"
{
#endif

void VMPU_Init( int );
void VMPU_Exit( void );
uint8_t VMPU_Acc(uint16_t port, uint8_t val, uint16_t flags);
void VMPU_SBMidi_RawWrite( uint8_t value );
#if SOUNDFONT
void VMPU_Process_Messages( void );
#endif

#ifdef __cplusplus
}
#endif

#endif//_VMPU_H_
