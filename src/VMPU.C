
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dos.h>
#include <string.h>

#include "CONFIG.H"
#include "LINEAR.H"
#include "PTRAP.H"
#include "VMPU.H"
#if SOUNDFONT
#include "../tsf/TSF.H"
#endif

#if VMPU

extern struct globalvars gvars;

#if SOUNDFONT
tsf* tsfrenderer = NULL;

struct VMPU_s {
    unsigned int wptr;
    unsigned int rptr;
    unsigned int index;
    unsigned char last_status;
    unsigned char data0;
    unsigned char buffer[4096];
    unsigned char sysex[32];
};
static struct VMPU_s vmpu;

static const unsigned char gm_reset[4] = { 0x7E, 0x7F, 0x09, 0x01 };
static const unsigned char gs_reset[9] = { 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41 };
#endif


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
#if SOUNDFONT
		vmpu.buffer[vmpu.wptr++] = value;
		vmpu.wptr &= 0xfff;
#endif
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
		/* status port
		 * bit 6: 1=interface busy
		 * bit 7: 1=no data
		 */
		uint8_t rc = 0;
		if ( bReset ) {
			dbgprintf(("VMPU_Read(%X)=0\n", port ));
			return rc;
		} else if ( bUART == 1 ) {
			dbgprintf(("VMPU_Read(%X)=0\n", port ));
			return rc;
		}
		rc |= 0x80; /* no data to read */
#if SOUNDFONT
		return rc | (( vmpu.wptr - vmpu.rptr ) == -1 ? 0x40 : 0);
#else
		return rc;
#endif
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

#if SOUNDFONT

/* process MIDI messages;
 * called during interrupt time - don't assume that
 * any message is "complete".
 */

void VMPU_Process_Messages(void)
////////////////////////////////
{
    for ( ;vmpu.rptr != vmpu.wptr; vmpu.rptr++, vmpu.rptr &= 0xfff ) {
        if ( (vmpu.buffer[vmpu.rptr] & 0x80) && vmpu.buffer[vmpu.rptr] != 0xF7 ) {
            vmpu.last_status = vmpu.buffer[vmpu.rptr];
            vmpu.index = 0;
        } else {
            switch ( vmpu.last_status & 0xF0 ) {
            case 0xD0: /* mono key pressure +1 */
                tsf_channel_set_pressure(tsfrenderer, vmpu.last_status & 0xf, vmpu.buffer[vmpu.rptr] / 127.f);
                break;
            case 0xA0: /* poly key pressure +2 */
                if ( vmpu.index != 0)
                    vmpu.index = -1;
                break;
            case 0x80: /* note off +2 */
                if ( vmpu.index == 0 )
                    vmpu.data0 = vmpu.buffer[vmpu.rptr];
                else {
                    /* second byte (velocity) currently not used */
                    tsf_channel_note_off(tsfrenderer, vmpu.last_status & 0xf, vmpu.data0);
                    vmpu.index = -1;
                }
                break;
            case 0x90: /* note on +2 */
                if ( vmpu.index == 0)
                    vmpu.data0 = vmpu.buffer[vmpu.rptr];
                else {
                    tsf_channel_note_on(tsfrenderer, vmpu.last_status & 0xf, vmpu.data0, vmpu.buffer[vmpu.rptr] / 127.0f);
                    vmpu.index = -1;
                }
                break;
            case 0xE0: /* pitch bend +2 */
                if ( vmpu.index == 0)
                    vmpu.data0 = vmpu.buffer[vmpu.rptr];
                else {
                    tsf_channel_set_pitchwheel(tsfrenderer, vmpu.last_status & 0xf, (vmpu.data0 & 0x7f) | ((vmpu.buffer[vmpu.rptr] & 0x7f) << 7));
                    vmpu.index = -1;
                }
                break;
            case 0xC0: /* program change +1 */
                tsf_channel_set_presetnumber(tsfrenderer, vmpu.last_status & 0xf, vmpu.buffer[vmpu.rptr], (vmpu.last_status & 0xf) == 0x9);
                break;
            case 0xB0: /* control change +2; channel mode msg if rsvd. controller numbers 120-127 are used */
                if ( vmpu.index == 0)
                    vmpu.data0 = vmpu.buffer[vmpu.rptr];
                else {
                    tsf_channel_midi_control(tsfrenderer, vmpu.last_status & 0xf, vmpu.data0, vmpu.buffer[vmpu.rptr]);
                    vmpu.index = -1;
                }
                break;
            case 0xF0: /* system +0 */
                switch ( vmpu.last_status & 0x0F ) {
                case 0:
                    if (vmpu.buffer[vmpu.rptr] == 0xF7) {
                        if (vmpu.sysex[0] == 0x41 && vmpu.sysex[2] == 0x42 && vmpu.sysex[3] == 0x12 && vmpu.index >= 8) {
                            uint32_t addr = ((uint32_t)vmpu.sysex[4] << 16) + ((uint32_t)vmpu.sysex[5] << 8) + (uint32_t)vmpu.sysex[6];
                            if (addr == 0x400004 && tsfrenderer) {
                                tsf_set_volume(tsfrenderer, ((vmpu.sysex[7] > 127) ? 127 : vmpu.sysex[7]) / 127.f);
                            }
                        }
                        if (vmpu.index > 5 && vmpu.sysex[0] == 0x7f && vmpu.sysex[1] == 0x7f && vmpu.sysex[2] == 0x04 && vmpu.sysex[3] == 0x01) {
                            //_dprintf("GM Master Vol 0x%02X\n", vmpu.sysex[5]);
                            tsf_set_volume(tsfrenderer, vmpu.sysex[5] / 127.f);
                        }
                        // TODO: Differentiate between GS and GM Resets.
                        if (!memcmp(vmpu.sysex, gs_reset, sizeof(gs_reset)) || !memcmp(vmpu.sysex, gm_reset, sizeof(gm_reset))) {
                            tsf_reset(tsfrenderer);
                            tsf_channel_set_bank_preset(tsfrenderer, 9, 128, 0);
                            tsf_set_volume(tsfrenderer, 1.0f);
                        }
                        vmpu.index = -1;
                    } else if ( vmpu.index < 32 )
                        if ( vmpu.index == 0 )
                            memset( vmpu.sysex, 0, sizeof( vmpu.sysex ) );
                        vmpu.sysex[vmpu.index] = vmpu.buffer[vmpu.rptr];
                    break;
                case 0xF:
                    tsf_reset(tsfrenderer);
                    tsf_channel_set_bank_preset(tsfrenderer, 9, 128, 0);
                    tsf_set_volume(tsfrenderer, 1.0f);
                    break;
                //case 0x1: /* MIDI time code quarter frame */
                //case 0x2: /* song position ptr + LSB,MSB */
                //case 0x3: /* song select */
                default:
                    break;
                }
                break;
            } /* end switch() */
            vmpu.index++;
        } /* end if() */
    } /* end for() */
}
#endif

void VMPU_Init( int freq )
//////////////////////////
{
#if SOUNDFONT
    char* soundfont_file = getenv("SOUNDFONT");
    if (!soundfont_file) return;
    if ( tsfrenderer = tsf_load_filename(soundfont_file) ) {
        int channel = 0;
        memset( &vmpu, 0, sizeof (vmpu ) );
        printf("TinySoundFont loaded; soundfont %s\n", soundfont_file);
        if (gvars.voices < 32) gvars.voices = 32;
        if (gvars.voices > 256) gvars.voices = 256;
        printf("Maximum voice limit: %d\n", gvars.voices);
        tsf_set_max_voices(tsfrenderer, gvars.voices);
        tsf_set_output(tsfrenderer, TSF_STEREO_INTERLEAVED, freq, 0);
        for (channel = 0; channel < 16; channel++)
            tsf_channel_midi_control(tsfrenderer, channel, 121, 0);
        tsf_channel_set_bank_preset(tsfrenderer, 9, 128, 0);
        tsf_set_samplerate_output(tsfrenderer, freq );
    } else
        printf("Failed loading soundfont %s\n", soundfont_file);
#endif
}

void VMPU_Exit( void )
//////////////////////
{
#if SOUNDFONT
    if ( tsfrenderer) {
        tsf *tmp = tsfrenderer;
        tsfrenderer = NULL; /* disables rendering in isr */
        tsf_close( tmp );
    }
#endif
}


#endif
