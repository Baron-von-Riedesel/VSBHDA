#ifndef _PTRAP_H_
#define _PTRAP_H_ 1

#ifdef __cplusplus
extern "C"
{
#endif

typedef uint8_t (*PORT_TRAP_HANDLER)(uint16_t port, uint8_t val, uint16_t flags);

#define PDT_FLGS_RMINST 1
#define PDT_FLGS_PMINST 2

#define TRAPF_OUT 4
#define TRAPF_IF  0x200

//get QEMM version
uint16_t PTRAP_GetQEMMVersion(void);

void PTRAP_Prepare( int opl, int sbaddr, int dma, int hdma, int sndint );
void PTRAP_InitPortMax(void);
bool PTRAP_Prepare_RM_PortTrap(void);
bool PTRAP_Install_RM_PortTraps( void );
bool PTRAP_Uninstall_RM_PortTraps( void );
void PTRAP_SetPICPortTrap( int );

bool PTRAP_DetectHDPMI();
bool PTRAP_Install_PM_PortTraps( void );
bool PTRAP_Uninstall_PM_PortTraps( void );

void    PTRAP_UntrappedIO_OUT(uint16_t port, uint8_t value);
uint8_t PTRAP_UntrappedIO_IN(uint16_t port);

#if PT0V86
/* v1.8: physical address v86 pagetab 0 */
uint32_t PTRAP_GetPageTab0v86( void );
#endif

extern void    (*UntrappedIO_OUT_Handler)(uint16_t port, uint8_t value);
extern uint8_t (*UntrappedIO_IN_Handler)(uint16_t port);

#define UntrappedIO_OUT( a, b ) UntrappedIO_OUT_Handler( a, b )
#define UntrappedIO_IN( a ) UntrappedIO_IN_Handler( a )

#ifdef __cplusplus
}
#endif

#endif
