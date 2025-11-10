
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
    unsigned int widx;  /* current write index in buffer */
    unsigned int ridx;  /* current read index in buffer */
    int didx;           /* index for current msg data */
    unsigned char status;
    unsigned char channel;
    unsigned char data[32]; /* data for current msg */
    unsigned char buffer[4096];
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
		vmpu.buffer[vmpu.widx++] = value;
		vmpu.widx &= 0xfff;
#endif
	}
	return;
}

static uint8_t VMPU_Read(uint16_t port)
///////////////////////////////////////
{
	/* data port? */
	if ( port == gvars.mpu ) {
		/* reading port 0x330 should reset MPU-401 interrupt status bit in mixer reg 0x82! */
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
		return rc | (( vmpu.widx - vmpu.ridx ) == -1 ? 0x40 : 0);
#else
		return rc;
#endif
	}
}

/* SB-MIDI data written with DSP cmd 0x38 (so-called "Normal" mode */

void VMPU_SBMidi_RawWrite( uint8_t value )
//////////////////////////////////////////
{
#if SOUNDFONT
    VMPU_Write( 0, value );
#endif
}

/* access of MIDI ports 0x330/0x331 */

uint8_t VMPU_Acc(uint16_t port, uint8_t val, uint16_t flags)
////////////////////////////////////////////////////////////
{
    return (flags & TRAPF_OUT) ? (VMPU_Write(port, val), val) : VMPU_Read(port);
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
    for ( ;vmpu.ridx != vmpu.widx; vmpu.ridx++, vmpu.ridx &= 0xfff ) {
        if ( (vmpu.buffer[vmpu.ridx] & 0x80) && vmpu.buffer[vmpu.ridx] != 0xF7 ) {
            vmpu.status = vmpu.buffer[vmpu.ridx] >> 4;
            vmpu.channel = vmpu.buffer[vmpu.ridx] & 0xf;
            vmpu.didx = 0;
        } else {
            switch ( vmpu.status ) {
            case 0xD: /* channel pressure +1 */
                tsf_channel_set_pressure(tsfrenderer, vmpu.channel, vmpu.buffer[vmpu.ridx] / 127.f);
                break;
            case 0xA: /* polyphonic key pressure +2, 1.byte is key, 2.byte is pressure value */
                if ( vmpu.didx == 0)
                    vmpu.data[0] = vmpu.buffer[vmpu.ridx];
                else {
                    vmpu.didx = -1;
                }
                break;
            case 0x8: /* note off +2 */
                if ( vmpu.didx == 0 )
                    vmpu.data[0] = vmpu.buffer[vmpu.ridx];
                else {
                    /* second byte (velocity) currently not used */
                    tsf_channel_note_off(tsfrenderer, vmpu.channel, vmpu.data[0]);
                    vmpu.didx = -1;
                }
                break;
            case 0x9: /* note on +2 */
                if ( vmpu.didx == 0)
                    vmpu.data[0] = vmpu.buffer[vmpu.ridx];
                else {
                    tsf_channel_note_on(tsfrenderer, vmpu.channel, vmpu.data[0], vmpu.buffer[vmpu.ridx] / 127.0f);
                    vmpu.didx = -1;
                }
                break;
            case 0xE: /* pitch bend +2 */
                if ( vmpu.didx == 0)
                    vmpu.data[0] = vmpu.buffer[vmpu.ridx];
                else {
                    tsf_channel_set_pitchwheel(tsfrenderer, vmpu.channel, vmpu.data[0] | (vmpu.buffer[vmpu.ridx] << 7));
                    vmpu.didx = -1;
                }
                break;
            case 0xC: /* program change +1 */
                tsf_channel_set_presetnumber(tsfrenderer, vmpu.channel, vmpu.buffer[vmpu.ridx], vmpu.channel == 0x9);
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
                if ( vmpu.didx == 0)
                    vmpu.data[0] = vmpu.buffer[vmpu.ridx];
                else {
                    tsf_channel_midi_control(tsfrenderer, vmpu.channel, vmpu.data[0], vmpu.buffer[vmpu.ridx]);
                    vmpu.didx = -1;
                }
                break;
            case 0xF: /* system +0 */
                switch ( vmpu.channel ) {
                case 0:
                    if ( vmpu.didx == 0 )
                        memset( vmpu.data, 0, sizeof( vmpu.data ) );
                    if (vmpu.buffer[vmpu.ridx] == 0xF7) {
                        /* sysex msg (41 xx 42 12 aaaaaa dd cc F7? (41=Roland, 42=GS synth, 12=sending) */
                        if (vmpu.didx > 8 && vmpu.data[0] == 0x41 && vmpu.data[2] == 0x42 && vmpu.data[3] == 0x12 ) {
                            uint32_t addr = ((uint32_t)vmpu.data[4] << 16) + ((uint32_t)vmpu.data[5] << 8) + (uint32_t)vmpu.data[6];
                            if ( addr == 0x400004 ) {
                                tsf_set_volume(tsfrenderer, ((vmpu.data[7] > 127) ? 127 : vmpu.data[7]) / 127.f);
                                dbgprintf(("sysex msg 0xF0: 41 %X 42 12 %6X %X (set GS volume)\n", vmpu.data[1], addr, vmpu.data[7] ));
                            } else if ( addr == 0x40007f ) {
                                if ( vmpu.data[7] == 0 ) { /* 0=GS reset (enter GS mode), 7F=gs "rereset" (exit GS mode) */
                                    reset();
                                    dbgprintf(("sysex msg 0xF0: 41 %X 42 12 40007f (GS reset)\n", vmpu.data[1] ));
                                }
#ifdef _DEBUG
                                else dbgprintf(("sysex msg 0xF0: 41 %X 42 12 %6X %X (unhandled)\n", vmpu.data[1], addr, vmpu.data[7] ));
#endif

                            }
#ifdef _DEBUG
                            else dbgprintf(("sysex msg 0xF0: 41 %X 42 12 %6X (unhandled)\n", vmpu.data[1], addr ));
#endif
                        } else if (vmpu.didx > 5 && vmpu.data[0] == 0x7f && vmpu.data[1] == 0x7f && vmpu.data[2] == 0x04 && vmpu.data[3] == 0x01) {
                            /* msg 7f 7f 04 01 ll mm? */
                            tsf_set_volume(tsfrenderer, vmpu.data[5] / 127.f);
                            dbgprintf(("sysex msg 0xF0: 7F 7F 04 01 v=%X (set GM master vol)\n", vmpu.data[5]));
                        } else if ( !memcmp(vmpu.data, gm_reset, sizeof(gm_reset)) ) {
                            // TODO: Differentiate between GS and GM Resets.
                            reset();
                            dbgprintf(("sysex msg 0xF0: %X %X %X %X (GM reset)\n", vmpu.data[0], vmpu.data[1], vmpu.data[2], vmpu.data[3]));
                        }
#ifdef _DEBUG
                        else dbgprintf(("unknown sysex msg 0xF0: %X %X %X %X\n", vmpu.data[0], vmpu.data[1], vmpu.data[2], vmpu.data[3]));
#endif
                        vmpu.didx = -1;
                    } else if ( vmpu.didx < sizeof( vmpu.data ) )
                        vmpu.data[vmpu.didx] = vmpu.buffer[vmpu.ridx];
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
                    if ( vmpu.didx == 0 ) dbgprintf(("sysex msg %X\n", vmpu.channel | 0xF0 ));
#endif
                    break;
                }
                break;
            } /* end switch() */
            vmpu.didx++;
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
        if (gvars.voices != VOICES_DEFAULT)
            printf("TSF: voice limit=%d\n", gvars.voices);
        tsf_set_max_voices(tsfrenderer, gvars.voices);
        tsf_set_output(tsfrenderer, TSF_STEREO_INTERLEAVED, freq, 0);
        tsf_set_samplerate_output(tsfrenderer, freq );
        for (channel = 0; channel < 16; channel++)
            tsf_channel_midi_control(tsfrenderer, channel, 121, 0); /* 121 = reset controller */
        tsf_channel_set_bank_preset(tsfrenderer, 9, 128, 0); /* channel 9 set to 128 (percussion) */
    } else
        printf("Failed loading soundfont \"%s\"\n", gvars.soundfont);
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
