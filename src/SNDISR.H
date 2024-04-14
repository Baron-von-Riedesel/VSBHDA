#ifndef _SNDISR_H_
#define _SNDISR_H_
//sound IRQ
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

struct clientregs {
    uint32_t ds;
    uint32_t es;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t res;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
#ifdef NOTFLAT
    uint16_t ip;
    uint16_t cs;
    uint16_t fl;
#else
    uint32_t ip;
    uint32_t cs;
    uint32_t fl;
#endif
};

#ifdef DJGPP /* no _far attribute in gcc */
bool SNDISR_InstallISR( uint8_t intno, int(*ISR)( uint32_t ofs, uint32_t sel ) );
int  SNDISR_Interrupt( uint32_t ofs, uint32_t sel );
#else
bool SNDISR_InstallISR( uint8_t intno, int(*ISR)(struct clientregs _far *) );
int  SNDISR_Interrupt( struct clientregs _far *clstat );
#endif
bool SNDISR_UninstallISR( void );

#ifdef __cplusplus
}
#endif

#endif
