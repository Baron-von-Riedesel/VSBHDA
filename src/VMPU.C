
#include <stdint.h>
#include <stdbool.h>
#include <dos.h>

#include "CONFIG.H"
#include "LINEAR.H"
#include "PTRAP.H"
#include "VMPU.H"

#if VMPU

extern struct globalvars gvars;

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
	dbgprintf(("VMPU_Write(%X, %X)\n", port, value ));
	if ( port == gvars.mpu + 1 ) {  /* command port? */
		if ( value == 0xff )
			bReset = true;
	} else {
	}
    return;
}

static uint8_t VMPU_Read(uint16_t port)
///////////////////////////////////////
{
	if ( port == gvars.mpu ) {
		if ( bReset ) {
			dbgprintf(("VMPU_Read(%X)=0xfe\n", port ));
			bReset = false;
			return 0xfe;
		}
		dbgprintf(("VMPU_Read(%X)=0\n", port ));
		return 0;
	} else {
		if ( bReset ) {
			dbgprintf(("VMPU_Read(%X)=0\n", port ));
			return 0x00;
		}
//		dbgprintf(("VMPU_Read(%X)=0x80\n", port ));
		return 0x80;
	}
}

/* SB-MIDI data written with DSP cmd 0x38 */

void VMPU_SBMidi_RawWrite( uint8_t value )
//////////////////////////////////////////
{
}

uint32_t VMPU_MPU(uint32_t port, uint32_t val, uint32_t out)
////////////////////////////////////////////////////////////
{
    return out ? (VMPU_Write(port, val), val) : ( val &= ~0xff, val |= VMPU_Read(port) );
}
#endif
