
#include <stdint.h>
#include <stdbool.h>
#include <dos.h>

#include "CONFIG.H"
#include "LINEAR.H"
#include "PTRAP.H"
#include "VMPU.H"

/* 0x330: data port
 * 0x331: read: status port
 *       write: command port
 * status port:
 * bit 6: 0=data ready to read
 * bit 7: 0=data ready to write (both command and data)
 * command port:
 *  ff: reset, then ACK (FE) from data port
 *  3f: set to UART mode
 */

static bool bReset = false;

static void VMPU_Write(uint16_t port, uint8_t value)
////////////////////////////////////////////////////
{
	dbgprintf(("VMPU_Write(%X)=%X\n", port, value ));
	if ( port == 0x331 ) {
		if ( value == 0xff )
			bReset = true;
	} else {
	}
    return;
}

static uint8_t VMPU_Read(uint16_t port)
///////////////////////////////////////
{
	dbgprintf(("VMPU_Read(%X)\n", port ));
	if ( port == 0x330 ) {
		if ( bReset ) {
			bReset = false;
			return 0xfe;
		}
		return 0;
	} else {
		if ( bReset )
			return 0x00;
		return 0x80;
	}
}


uint32_t VMPU_MPU(uint32_t port, uint32_t val, uint32_t out)
////////////////////////////////////////////////////////////
{
    return out ? (VMPU_Write(port, val), val) : ( val &= ~0xff, val |= VMPU_Read(port) );
}
