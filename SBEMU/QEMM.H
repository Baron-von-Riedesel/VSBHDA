#ifndef _EMM_H_
#define _EMM_H_ 1
#include "PLATFORM.H"

#ifdef __cplusplus
extern "C"
{
#endif

typedef uint32_t (*QEMM_IOTRAP_HANDLER)(uint32_t port, uint32_t val, uint32_t out);

//user interface, not actual struct
typedef struct QEMM_IODispatchTable
{
    uint16_t    port;
    uint16_t    flags;
    QEMM_IOTRAP_HANDLER handler;
}QEMM_IODT;

#define IODT_FLGS_RMINST 1
#define IODT_FLGS_PMINST 2

//get QEMM version
uint16_t QEMM_GetVersion(void);

//I/O virtualize
BOOL QEMM_Prepare_IOPortTrap(void);
BOOL QEMM_Install_PortTraps( QEMM_IODT *, int *, int );
BOOL QEMM_Uninstall_PortTraps( QEMM_IODT *, int );
#if QEMMPICTRAPDYN
void QEMM_SetPICPortTrap( int );
#endif

#ifdef __cplusplus
}
#endif

#endif