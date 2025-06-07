
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
 * bit 6: 0=ready to write cmd or MIDI data; 1=interface busy
 * bit 7: 0=data ready to read; 1=no data at data port
 * command port:
 *  0xff: reset - triggers ACK (FE) to be read from data port
 *  0x3f: set to UART mode - triggers ACK (FE) to be read from data port
 */

static bool bReset = false;
static uint8_t bUART = 0;

static void VMPU_Write(uint16_t port, uint8_t value)
////////////////////////////////////////////////////
{
	dbgprintf(("VMPU_Write(%X, %X)\n", port, value ));
	if ( port == gvars.mpu + 1 ) {	/* command port? */
		if ( value == 0xff ) /* reset? */
			bReset = true;
		else if ( value == 0x3f ) /* UART mode? */
			bUART = 1;
	} else {
	}
    return;
}

static uint8_t VMPU_Read(uint16_t port)
///////////////////////////////////////
{
    /* data port? */
	if ( port == gvars.mpu ) {
		if ( bReset ) {
			dbgprintf(("VMPU_Read(%X)=0xfe (reset)\n", port ));
			bReset = false;
			bUART = 0;
			return 0xfe;
		} else if ( bUART == 1 ) {
			dbgprintf(("VMPU_Read(%X)=0xfe (UART mode)\n", port ));
			bUART = 2;
			return 0xfe;
		}
		dbgprintf(("VMPU_Read(%X)=0\n", port ));
		return 0;
	} else {
		/* status port */
		if ( bReset ) {
			dbgprintf(("VMPU_Read(%X)=0\n", port ));
			return 0x00; /* bit 6=0 -> data ready */
		} else if ( bUART == 1 ) {
			dbgprintf(("VMPU_Read(%X)=0\n", port ));
			return 0x00; /* bit 6=0 -> data ready */
		}
//		dbgprintf(("VMPU_Read(%X)=0x80\n", port ));
		return 0x80; /* 80h=no data (bit 7=1); interface not busy (bit 6=0) */
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
