#ifndef _PTRAP_H_
#define _PTRAP_H_ 1

#ifdef __cplusplus
extern "C"
{
#endif

typedef uint8_t (*PORT_TRAP_HANDLER)(uint16_t port, uint8_t val, uint16_t flags);

#define TRAPF_OUT 4
#define TRAPF_IF  0x200

//get QEMM version
uint16_t PTRAP_GetQEMMVersion(void);

int  PTRAP_AddRange( int start, unsigned int portmap, const PORT_TRAP_HANDLER * );
bool PTRAP_Init_RM(void);
void PTRAP_SetPICPortTrap( int );
bool PTRAP_DetectHDPMI();
bool PTRAP_Install_PortTraps( int, int );
bool PTRAP_Uninstall_PortTraps( int, int );

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
