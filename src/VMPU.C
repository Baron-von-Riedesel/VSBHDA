
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
    unsigned char status;
    unsigned char channel;
    unsigned char data0;
    unsigned char buffer[4096];
    unsigned char sysex[32];
};
static struct VMPU_s vmpu;

static const unsigned char gm_reset[4] = { 0x7E, 0x7F, 0x09, 0x01 };
//static const unsigned char gs_reset[9] = { 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41 };
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
#if SOUNDFONT
    VMPU_Write( 0, value );
#endif
}

uint32_t VMPU_MPU(uint32_t port, uint32_t val, uint32_t out)
////////////////////////////////////////////////////////////
{
    return out ? (VMPU_Write(port, val), val) : ( val &= ~0xff, val |= VMPU_Read(port) );
}

#if SOUNDFONT

static void reset(void)
///////////////////////
{
    tsf_reset(tsfrenderer);
    tsf_channel_set_bank_preset(tsfrenderer, 9, 128, 0);
    tsf_set_volume(tsfrenderer, 1.0f);
}

/* process MIDI messages;
 * called during interrupt time - don't assume that
 * any message is "complete".
 */

void VMPU_Process_Messages(void)
////////////////////////////////
{
    for ( ;vmpu.rptr != vmpu.wptr; vmpu.rptr++, vmpu.rptr &= 0xfff ) {
        if ( (vmpu.buffer[vmpu.rptr] & 0x80) && vmpu.buffer[vmpu.rptr] != 0xF7 ) {
            vmpu.status = vmpu.buffer[vmpu.rptr] >> 4;
            vmpu.channel = vmpu.buffer[vmpu.rptr] & 0xf;
            vmpu.index = 0;
        } else {
            switch ( vmpu.status ) {
            case 0xD: /* channel pressure +1 */
                tsf_channel_set_pressure(tsfrenderer, vmpu.channel, vmpu.buffer[vmpu.rptr] / 127.f);
                break;
            case 0xA: /* polyphonic key pressure +2, 1.byte is key, 2.byte is pressure value */
                if ( vmpu.index == 0)
                    vmpu.data0 = vmpu.buffer[vmpu.rptr];
                else {
                    vmpu.index = -1;
                }
                break;
            case 0x8: /* note off +2 */
                if ( vmpu.index == 0 )
                    vmpu.data0 = vmpu.buffer[vmpu.rptr];
                else {
                    /* second byte (velocity) currently not used */
                    tsf_channel_note_off(tsfrenderer, vmpu.channel, vmpu.data0);
                    vmpu.index = -1;
                }
                break;
            case 0x9: /* note on +2 */
                if ( vmpu.index == 0)
                    vmpu.data0 = vmpu.buffer[vmpu.rptr];
                else {
                    tsf_channel_note_on(tsfrenderer, vmpu.channel, vmpu.data0, vmpu.buffer[vmpu.rptr] / 127.0f);
                    vmpu.index = -1;
                }
                break;
            case 0xE: /* pitch bend +2 */
                if ( vmpu.index == 0)
                    vmpu.data0 = vmpu.buffer[vmpu.rptr];
                else {
                    tsf_channel_set_pitchwheel(tsfrenderer, vmpu.channel, vmpu.data0 | (vmpu.buffer[vmpu.rptr] << 7));
                    vmpu.index = -1;
                }
                break;
            case 0xC: /* program change +1 */
                tsf_channel_set_presetnumber(tsfrenderer, vmpu.channel, vmpu.buffer[vmpu.rptr], vmpu.channel == 0x9);
                break;
            case 0xB:
             /* control change +2; channel mode msg if rsvd. controller numbers 120-127 are used
              * 1. byte is c(ontroller), 2. byte is v(alue.
              * c=120, v=0: all sounds off
              * c=121, v=0: all controllers reset
              * c=122, v=0/127: local control off/on
              * c=123, v=0: all notes off
              * c=124/125/126/127, v=0: omni mode off/on, mono mode on, poly mode on
              */
                if ( vmpu.index == 0)
                    vmpu.data0 = vmpu.buffer[vmpu.rptr];
                else {
                    tsf_channel_midi_control(tsfrenderer, vmpu.channel, vmpu.data0, vmpu.buffer[vmpu.rptr]);
                    vmpu.index = -1;
                }
                break;
            case 0xF: /* system +0 */
                switch ( vmpu.channel ) {
                case 0:
                    if ( vmpu.index == 0 )
                        memset( vmpu.sysex, 0, sizeof( vmpu.sysex ) );
                    if (vmpu.buffer[vmpu.rptr] == 0xF7) {
                        /* sysex msg (41 xx 42 12 aaaaaa dd cc F7? (41=Roland, 42=GS synth, 12=sending) */
                        if (vmpu.index > 8 && vmpu.sysex[0] == 0x41 && vmpu.sysex[2] == 0x42 && vmpu.sysex[3] == 0x12 ) {
                            uint32_t addr = ((uint32_t)vmpu.sysex[4] << 16) + ((uint32_t)vmpu.sysex[5] << 8) + (uint32_t)vmpu.sysex[6];
                            if ( addr == 0x400004 ) {
                                tsf_set_volume(tsfrenderer, ((vmpu.sysex[7] > 127) ? 127 : vmpu.sysex[7]) / 127.f);
                                dbgprintf(("sysex msg 0xF0: 41 %X 42 12 %6X %X (set GS volume)\n", vmpu.sysex[1], addr, vmpu.sysex[7] ));
                            } else if ( addr == 0x40007f ) {
                                if ( vmpu.sysex[7] == 0 ) { /* 0=GS reset (enter GS mode), 7F=gs "rereset" (exit GS mode) */
                                    reset();
                                    dbgprintf(("sysex msg 0xF0: 41 %X 42 12 40007f (GS reset)\n", vmpu.sysex[1] ));
                                }
#ifdef _DEBUG
                                else dbgprintf(("sysex msg 0xF0: 41 %X 42 12 %6X %X (unhandled)\n", vmpu.sysex[1], addr, vmpu.sysex[7] ));
#endif

                            }
#ifdef _DEBUG
                            else dbgprintf(("sysex msg 0xF0: 41 %X 42 12 %6X (unhandled)\n", vmpu.sysex[1], addr ));
#endif
                        } else if (vmpu.index > 5 && vmpu.sysex[0] == 0x7f && vmpu.sysex[1] == 0x7f && vmpu.sysex[2] == 0x04 && vmpu.sysex[3] == 0x01) {
                            /* msg 7f 7f 04 01 ll mm? */
                            tsf_set_volume(tsfrenderer, vmpu.sysex[5] / 127.f);
                            dbgprintf(("sysex msg 0xF0: 7F 7F 04 01 v=%X (set GM master vol)\n", vmpu.sysex[5]));
                        } else if ( !memcmp(vmpu.sysex, gm_reset, sizeof(gm_reset)) ) {
                            // TODO: Differentiate between GS and GM Resets.
                            reset();
                            dbgprintf(("sysex msg 0xF0: %X %X %X %X (GM reset)\n", vmpu.sysex[0], vmpu.sysex[1], vmpu.sysex[2], vmpu.sysex[3]));
                        }
#ifdef _DEBUG
                        else dbgprintf(("unknown sysex msg 0xF0: %X %X %X %X\n", vmpu.sysex[0], vmpu.sysex[1], vmpu.sysex[2], vmpu.sysex[3]));
#endif
                        vmpu.index = -1;
                    } else if ( vmpu.index < sizeof( vmpu.sysex ) )
                        vmpu.sysex[vmpu.index] = vmpu.buffer[vmpu.rptr];
                    break;
                case 0xF: /* 0xff - in a midi file it's supposed to be start of a "meta event"! */
                    reset();
                    dbgprintf(("sysex msg 0xFF\n"));
                    break;
                //case 0x1: /* MIDI time code quarter frame */
                //case 0x2: /* song position ptr + LSB,MSB */
                //case 0x3: /* song select */
                default:
#ifdef _DEBUG
                    if ( vmpu.index == 0 ) dbgprintf(("sysex msg %X\n", vmpu.channel | 0xF0 ));
#endif
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
    if (!gvars.soundfont) ;
    else if ( tsfrenderer = tsf_load_filename(gvars.soundfont) ) {
        int channel = 0;
        memset( &vmpu, 0, sizeof (vmpu ) );
        printf("TSF: soundfont %s [presets=%d]\n", gvars.soundfont, tsf_get_presetcount(tsfrenderer) );
        tsf_set_max_voices(tsfrenderer, gvars.voices);
        printf("TSF: voice limit=%d\n", gvars.voices);
        tsf_set_output(tsfrenderer, TSF_STEREO_INTERLEAVED, freq, 0);
        tsf_set_samplerate_output(tsfrenderer, freq );
        for (channel = 0; channel < 16; channel++)
            tsf_channel_midi_control(tsfrenderer, channel, 121, 0); /* 121 = reset controller */
        tsf_channel_set_bank_preset(tsfrenderer, 9, 128, 0); /* channel 9 set to 128 (percussion) */
    } else
        printf("Failed loading soundfont %s\n", gvars.soundfont);
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
