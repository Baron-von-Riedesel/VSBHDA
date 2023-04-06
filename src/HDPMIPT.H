#ifndef _HDPMIPT_H_
#define _HDPMIPT_H_
//HDPMI port trap utility

#include "QEMM.H" //QEMM compatible interface

bool HDPMIPT_Detect();

bool HDPMIPT_Install_PortTraps( QEMM_IODT[], int *, int );
bool HDPMIPT_Uninstall_PortTraps( QEMM_IODT*, int );

bool HDPMIPT_InstallISR( uint8_t irq, int(*ISR)(void) );
bool HDPMIPT_UninstallISR( void );

void HDPMIPT_UntrappedIO_OUT(uint16_t port, uint8_t value);
uint8_t HDPMIPT_UntrappedIO_IN(uint16_t port);

#ifdef _DEBUG
void HDPMIPT_PrintPorts( QEMM_IODT* , int ); /* display trapped ports in protected-mode */
#endif

#endif //_HDPMIPT_H_