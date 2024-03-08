
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "CONFIG.H"
#include "PLATFORM.H"
#include "VSB.H"
#include "VIRQ.H"

#if REINITOPL
extern void MAIN_ReinitOPL( void );
#endif

#define SILENCE  1 /* 1=support DSP cmd 0x80 */
#define LATERATE 0 /* 1=store time constant and compute rate when required */

static const int VSB_DSPVersion[] =
{
    0,
    0x0100,
    0x0105, /* type 2: SB 1.5? */
    0x0202,
    0x0302, /* type 4: SB Pro */
    0x0302,
#if SB16
    0x0405, /* type 6: SB16 */
#endif
};

extern void pds_mdelay(unsigned long x);

void MAIN_Uninstall( void );

#if ADPCM
extern ADPCM_STATE ISR_adpcm_state;
#endif

#define VSB_RESET_END 0
#define VSB_RESET_START 1

static int VSB_ResetState = VSB_RESET_END;
static int VSB_Started = 0;
static int VSB_IRQ = 5;
static int VSB_DMA = 1;
#if SB16
static int VSB_HDMA = 5;
#endif
static int VSB_DACSpeaker = 1;
static unsigned int VSB_Bits = 8;
static int VSB_SampleRate = 22050;
static int VSB_Samples = 0;
static int VSB_Auto = false; /* auto-initialize mode active */
static int VSB_HighSpeed = 0;
static int VSB_Signed = false;
static int VSB_Silent = false;
static int VSB_DSPCMD = -1;
static int VSB_DSPCMD_Subindex = 0;
int VSB_TriggerIRQ = 0;
static int VSB_Pos = 0;
static const uint8_t VSB_IRQMap[4] = {2,5,7,10};
static uint8_t VSB_MixerRegIndex = 0;
static uint8_t VSB_TestReg;
static uint8_t VSB_WS = 0x80;
static uint8_t VSB_RS = 0x7F;
static uint16_t VSB_DSPVER = 0x0302;
static const int VSB_TimeConstantMapMono[][2] =
{
    0xA5, 11025,
    0xD2, 22050,
    0xE9, 44100,
};
static const uint8_t SB_Copyright[] = "COPYRIGHT (C) CREATIVE TECHNOLOGY LTD, 1992.";

static int DSPDataBytes = 0;
static uint8_t *pData;
static uint8_t DataBuffer[2];
static uint8_t bSpeaker;
#if LATERATE
static uint8_t bTimeConst = 0;
#endif
static uint8_t VSB_MixerRegs[256];

static int VSB_Indexof(const uint8_t* array, int count, uint8_t  val)
/////////////////////////////////////////////////////////////////////
{
	int i;
	for( i = 0; i < count; ++i ) {
		if(array[i] == val)
			return i;
	}
	return -1;
}

/* write port 2x4 */

static void VSB_Mixer_WriteAddr( uint8_t value )
////////////////////////////////////////////////
{
    dbgprintf(("VSB_Mixer_WriteAddr: value=%x\n", value));
    VSB_MixerRegIndex = value;
}

/* write port 2x5 */

static void VSB_Mixer_Write( uint8_t value )
////////////////////////////////////////////
{
	dbgprintf(("VSB_Mixer_Write[%u]: value=%x\n", VSB_MixerRegIndex, value));
	VSB_MixerRegs[VSB_MixerRegIndex] = value;
	if( VSB_MixerRegIndex == SB_MIXERREG_RESET ) {
		VSB_MixerRegs[SB_MIXERREG_MASTERVOL] = 0xD; /* 02: bits 1-3, L&R?, default 0x99?, for SBPro+: map to 0x22? */
		VSB_MixerRegs[SB_MIXERREG_MIDIVOL] = 0xD;   /* 06: bits 1-3 */
		/* todo: SB_MIXERREG_VOICEVOL is for SB20 only, for SBPro+ it's MIC level 2/3 bits */
		VSB_MixerRegs[SB_MIXERREG_VOICEVOL] = 0x6;  /* 0A: bits 1-2, default ? */

		VSB_MixerRegs[SB_MIXERREG_VOICESTEREO] = 0xDD;  /* 04: */
		VSB_MixerRegs[SB_MIXERREG_MASTERSTEREO] = 0xDD; /* 22: */
		VSB_MixerRegs[SB_MIXERREG_MIDISTEREO] = 0xDD;   /* 26: */
#if SB16
		if(VSB_DSPVER >= 0x0400) { //SB16
			VSB_MixerRegs[SB16_MIXERREG_MASTERL] = 0xC0; /* 5 bits only (3-7) */
			VSB_MixerRegs[SB16_MIXERREG_MASTERR] = 0xC0;
			VSB_MixerRegs[SB16_MIXERREG_VOICEL] = 0xC0;
			VSB_MixerRegs[SB16_MIXERREG_VOICER] = 0xC0;
			VSB_MixerRegs[SB16_MIXERREG_MIDIL] = 0xC0;
			VSB_MixerRegs[SB16_MIXERREG_MIDIR] = 0xC0;
			VSB_MixerRegs[SB16_MIXERREG_TREBLEL] = 0x80;
			VSB_MixerRegs[SB16_MIXERREG_TREBLER] = 0x80;
			VSB_MixerRegs[SB16_MIXERREG_BASSL] = 0x80;
			VSB_MixerRegs[SB16_MIXERREG_BASSR] = 0x80;
		}
#endif
	}
#if SB16
	if( VSB_DSPVER >= 0x0400 ) { //SB16
		if( VSB_MixerRegIndex >= SB16_MIXERREG_MASTERL && VSB_MixerRegIndex <= SB16_MIXERREG_MIDIR ) {
			//5bits, drop lowest bit
			value = value >> 4;
			switch(VSB_MixerRegIndex) {
			case SB16_MIXERREG_MASTERL:
				VSB_MixerRegs[SB_MIXERREG_MASTERSTEREO] &= 0x0F;
				VSB_MixerRegs[SB_MIXERREG_MASTERSTEREO] |= (value << 4);
				break;
			case SB16_MIXERREG_MASTERR:
				VSB_MixerRegs[SB_MIXERREG_MASTERSTEREO] &= 0xF0;
				VSB_MixerRegs[SB_MIXERREG_MASTERSTEREO] |= value;
				break;
			case SB16_MIXERREG_VOICEL:
				VSB_MixerRegs[SB_MIXERREG_VOICESTEREO]  &= 0x0F;
				VSB_MixerRegs[SB_MIXERREG_VOICESTEREO]  |= (value << 4);
				break;
			case SB16_MIXERREG_VOICER:
				VSB_MixerRegs[SB_MIXERREG_VOICESTEREO]  &= 0xF0;
				VSB_MixerRegs[SB_MIXERREG_VOICESTEREO]  |= value;
				break;
			case SB16_MIXERREG_MIDIL:
				VSB_MixerRegs[SB_MIXERREG_MIDISTEREO]   &= 0x0F;
				VSB_MixerRegs[SB_MIXERREG_MIDISTEREO]   |= (value << 4);
				break;
			case SB16_MIXERREG_MIDIR:
				VSB_MixerRegs[SB_MIXERREG_MIDISTEREO]   &= 0xF0;
				VSB_MixerRegs[SB_MIXERREG_MIDISTEREO]   |= value;
				break;
			}
		} else {
			/* map registers:
			 * SB16: auto update MASTERL/MASTERR if MASTERSTEREO is set
			 * SB16: auto update VOICEL/VOICER   if VOICESTEREO is set
			 * SB16: auto update MIDIL/MIDIR     if MIDISTEREO is set
			 */
			switch ( VSB_MixerRegIndex ) {
			case SB_MIXERREG_MASTERSTEREO:
				VSB_MixerRegs[SB16_MIXERREG_MASTERR] = ((value & 0xF) << 4) | 8;
				VSB_MixerRegs[SB16_MIXERREG_MASTERL] = (value & 0xF0) | 8;
				break;
			case SB_MIXERREG_VOICESTEREO:
				VSB_MixerRegs[SB16_MIXERREG_VOICER] = ((value & 0xF) << 4) | 8;
				VSB_MixerRegs[SB16_MIXERREG_VOICEL] = (value & 0xF0) | 8;
				break;
			case SB_MIXERREG_MIDISTEREO:
				VSB_MixerRegs[SB16_MIXERREG_MIDIR] = ((value & 0xF) << 4) | 8;
				VSB_MixerRegs[SB16_MIXERREG_MIDIL] = (value & 0xF0) | 8;
				break;
			}
		}
	}
#endif
}

/* read port 2x5 */

static uint8_t VSB_Mixer_Read( void )
/////////////////////////////////////
{
	dbgprintf(("Mixer_Read: %x\n", VSB_MixerRegs[VSB_MixerRegIndex]));
	return VSB_MixerRegs[VSB_MixerRegIndex];
}

/* write port 2x6 */

static void DSP_Reset( uint8_t value )
//////////////////////////////////////
{
    dbgprintf(("DSP_Reset: %u\n",value));
    if(value == 1) {
        VSB_ResetState = VSB_RESET_START;
        VSB_MixerRegs[SB_MIXERREG_INT_SETUP] = 1 << VSB_Indexof(VSB_IRQMap, countof(VSB_IRQMap), VSB_IRQ);
        //VSB_MixerRegs[SB_MIXERREG_DMA_SETUP] = (1 << VSB_DMA) & 0xEB;
#if SB16
        VSB_MixerRegs[SB_MIXERREG_DMA_SETUP] = ( (1 << VSB_DMA) | ( VSB_HDMA ? (1<<VSB_HDMA) : 0)) & 0xEB;
#else
        VSB_MixerRegs[SB_MIXERREG_DMA_SETUP] = (1 << VSB_DMA) & 0xB;
#endif
        VSB_MixerRegs[SB_MIXERREG_MODEFILTER] = 0xFD; //mask out stereo
        VSB_MixerRegIndex = 0;
        VSB_DSPCMD = -1;
        VSB_DSPCMD_Subindex = 0;
        DSPDataBytes = 0;
        VSB_Started = 0;
        VSB_Samples = 0;
        VSB_Auto = false;
        VSB_Signed = false;
        VSB_Silent = false;
        VSB_Bits = 8;
        VSB_Pos = 0;
        VSB_HighSpeed = 0;
        VSB_TriggerIRQ = 0;
#if LATERATE
        bTimeConst = 0xD2; /* = 22050 */
#endif
        VIRQ_SetCallType();
        VSB_Mixer_WriteAddr( SB_MIXERREG_RESET );
        VSB_Mixer_Write( 1 );
#if REINITOPL
        MAIN_ReinitOPL();
#endif
    } else if ( VSB_ResetState == VSB_RESET_START ) {
        switch (value) {
        case 0:
            pData = DataBuffer;
            DSPDataBytes = 1;
            DataBuffer[0] = 0xAA;
            VSB_ResetState = VSB_RESET_END;
            break;
        case 0x55:  /* uninstall */
            MAIN_Uninstall();
        }
    }
}

/* write port 2xC */

static void DSP_Write( uint8_t value )
//////////////////////////////////////
{
    int OldStarted;
    dbgprintf(("DSP_Write %02x, DSPCMD=%02x\n", value, VSB_DSPCMD));
    if(VSB_HighSpeed) //highspeed won't accept further commands, need reset
        return;
    OldStarted = VSB_Started;
    VSB_WS = 0x80;
    if( VSB_DSPCMD == -1 ) {
        //VSB_DSPCMD = value;
        switch( value ) { /* handle 1-byte cmds here */
        case SB_DSP_TRIGGER_IRQ: /* F2 */
            VSB_MixerRegs[SB_MIXERREG_IRQ_STATUS] |= 0x1;
            VSB_TriggerIRQ = 1;
            break;
#if SB16
        case SB_DSP_TRIGGER_IRQ16: /* F3 */
            VSB_MixerRegs[SB_MIXERREG_IRQ_STATUS] |= 0x2;
            VSB_TriggerIRQ = 1;
            break;
#endif
        case SB_DSP_SPEAKER_ON: /* D1 */
            bSpeaker = true;
            break;
        case SB_DSP_SPEAKER_OFF: /* D3 */
            bSpeaker = false;
            break;
            break;
        case SB_DSP_HALT_DMA: /* D0 */
        case SB_DSP_CONTINUE_DMA: /* D4 */
            VSB_Started = ( value == SB_DSP_CONTINUE_DMA );
            break;
#if SB16
        case SB_DSP_HALT_DMA16: /* D5 */
            VSB_Started = false;
            break;
        case SB_DSP_CONT_8BIT_AUTO: /* 45 */
        case SB_DSP_CONT_16BIT_AUTO: /* 47 */
            if (!VSB_Auto)
                break;
        case SB_DSP_CONTINUE_DMA16: /* D6 */
            VSB_Started = true;
            break;
#endif
        case SB_DSP_8BIT_OUT_AUTO_HS: /* 90 */
        case SB_DSP_8BIT_OUT_AUTO: /* 1C */
            VSB_Auto = true;
            VSB_Bits = 8;
            VSB_HighSpeed = ( value == SB_DSP_8BIT_OUT_AUTO_HS );
            VSB_Signed = false;
            VSB_Silent = false;
            VSB_Started = true; //start transfer
            VSB_Pos = 0;
            break;
#if ADPCM
        case SB_DSP_2BIT_OUT_AUTO: /* 1F; AUTO: bit 3=1 */
        case SB_DSP_3BIT_OUT_AUTO: /* 7F */
        case SB_DSP_4BIT_OUT_AUTO: /* 7D */
            VSB_Auto = true;
            ISR_adpcm_state.useRef = true;
            ISR_adpcm_state.step = 0;
            VSB_Bits = (value == SB_DSP_2BIT_OUT_AUTO) ? 2 : (value == SB_DSP_3BIT_OUT_AUTO) ? 3 : 4;
            VSB_MixerRegs[SB_MIXERREG_MODEFILTER] &= ~0x2;
            VSB_Silent = false;
            VSB_Started = true; //start transfer here
            VSB_Pos = 0;
            break;
#endif
#if SB16
        case SB_DSP_EXIT_16BIT_AUTO: /* D9 */
        case SB_DSP_EXIT_8BIT_AUTO:  /* DA */
            if( VSB_Auto ) {
                VSB_Auto = false;
                VSB_Started = false;
            }
            break;
#endif
		case SB_DSP_GETVER: /* E1 */
			pData = DataBuffer;
			DSPDataBytes = 2;
			DataBuffer[0] = VSB_DSPVER >> 8;
			DataBuffer[1] = VSB_DSPVER & 0xFF;
			break;
		case SB_DSP_COPYRIGHT: /* E3 */
			pData = (uint8_t *)SB_Copyright;
			DSPDataBytes = sizeof( SB_Copyright ) - 1;
			break;
		case SB_DSP_READ_TESTREG: /* E8 */
			pData = DataBuffer;
			DSPDataBytes = 1;
			DataBuffer[0] = VSB_TestReg;
			break;
		case SB_DSP_SPEAKER_STATUS: /* D8 */
			pData = DataBuffer;
			DSPDataBytes = 1;
			DataBuffer[0] = ( bSpeaker ? 0xff : 0 );
			break;
#if SB16
		case SB_DSP_STATUS: /* FB */
			pData = DataBuffer;
			DSPDataBytes = 1;
			DataBuffer[0] = VSB_Started ? (( VSB_Bits <= 8 ? 1 : 4 ) ) : 0;
			break;
#endif
        case 0x2A: //unknown commands
            break;
        default:
            VSB_DSPCMD = value;
            VSB_DSPCMD_Subindex = 0;
        }
#if SB16
    } else if ( VSB_DSPCMD >= 0xB0 && VSB_DSPCMD <= 0xCF ) {
        //VSB_Fifo = ( VSB_DSPCMD & 0x2 ) ? 1 : 0;
        switch ( VSB_DSPCMD_Subindex ) {
        case 0:
            VSB_Auto = ( ( VSB_DSPCMD & 0x4 ) ? true : false );
            VSB_Bits = ( ( VSB_DSPCMD & 0x40 ) ? 8 : 16 );
            /* bit 4 of value: 1=signed */
            VSB_Signed = ( ( value & 0x10 ) ? true : false );
            /* bit 5 of value: 1=stereo */
            VSB_MixerRegs[SB_MIXERREG_MODEFILTER] &= ~2;
            VSB_MixerRegs[SB_MIXERREG_MODEFILTER] |= ( ( value & 0x20 ) ? 2 : 0 );
            VSB_DSPCMD_Subindex++;
            break;
        case 1:
            VSB_Samples = value; /* lobyte */
            VSB_DSPCMD_Subindex++;
            break;
        default:
            VSB_Samples |= value << 8; /* hibyte; the value is #samples-1! */
            VSB_DSPCMD = -1;
            VSB_Silent = false;
            VSB_Started = true;
            VSB_Pos = 0;
        }
#endif
    } else {
        int i;
        switch(VSB_DSPCMD) {
        case SB_DSP_SET_TIMECONST: /* 40 */
            VSB_SampleRate = 0;
#if !LATERATE
            for( i = 0; i < 3; ++i ) {
                if(value >= VSB_TimeConstantMapMono[i][0]-3 && value <= VSB_TimeConstantMapMono[i][0]+3) {
                    VSB_SampleRate = VSB_TimeConstantMapMono[i][1] / VSB_GetChannels();
                    break;
                }
            }
            if(VSB_SampleRate == 0)
                VSB_SampleRate = 256000000/( 65536 - (value << 8) ) / VSB_GetChannels();
                //VSB_SampleRate = 1000000 / ( 256 - value ) / VSB_GetChannels();
#else
            bTimeConst = value;
#endif
            dbgprintf(("DSP_Write: time constant=%X\n", value ));
            VSB_DSPCMD_Subindex = 2; //only 1byte
            break;
        case SB_DSP_SET_SIZE: /* 48 - used for auto command */
        case SB_DSP_8BIT_OUT_1_HS: /* 91 */
        case SB_DSP_8BIT_OUT_1: /* 14 */
            if(VSB_DSPCMD_Subindex++ == 0)
                VSB_Samples = value;
            else {
                VSB_Samples |= value << 8; /* the value is #samples-1! */
                VSB_HighSpeed = ( VSB_DSPCMD == SB_DSP_8BIT_OUT_AUTO_HS );
                if ( VSB_DSPCMD == SB_DSP_8BIT_OUT_1 || VSB_DSPCMD == SB_DSP_8BIT_OUT_1_HS ) {
                    VSB_Silent = false;
                    VSB_Started = true;
                    VSB_Bits = 8;
                    VSB_Pos = 0;
                }
            }
            break;
#if ADPCM
        case SB_DSP_2BIT_OUT_1_NREF: /* 16 */
        case SB_DSP_2BIT_OUT_1:      /* 17 */
        case SB_DSP_4BIT_OUT_1_NREF: /* 74 */
        case SB_DSP_4BIT_OUT_1:      /* 75 */
        case SB_DSP_3BIT_OUT_1_NREF: /* 76 */
        case SB_DSP_3BIT_OUT_1:      /* 77 */
            if( VSB_DSPCMD_Subindex++ == 0 )
                VSB_Samples = value; /* lobyte */
            else {
                VSB_Samples |= value << 8; /* hibyte; the value is #samples-1! */
                VSB_Auto = false;
                /* useref is bit 0 */
                //VSB_ADPCM.useRef = (VSB_DSPCMD == SB_DSP_2BIT_OUT_1 || VSB_DSPCMD == SB_DSP_3BIT_OUT_1 || VSB_DSPCMD == SB_DSP_4BIT_OUT_1);
                ISR_adpcm_state.useRef = (VSB_DSPCMD & 1 );
                ISR_adpcm_state.step = 0;
                VSB_Bits = (VSB_DSPCMD <= SB_DSP_2BIT_OUT_1) ? 2 : (VSB_DSPCMD >= SB_DSP_3BIT_OUT_1_NREF) ? 3 : 4;
                VSB_MixerRegs[SB_MIXERREG_MODEFILTER] &= ~0x2; /* reset stereo */
                VSB_Started = true; //start transfer here
                VSB_Silent = false;
                VSB_Signed = false;
                VSB_Pos = 0;
            }
            break;
#endif
        case SB_DSP_SET_SAMPLERATE_I: /* 42 */
            VSB_DSPCMD_Subindex++;
            break;
        case SB_DSP_SET_SAMPLERATE: /* 41 - command start: sample rate */
            if(VSB_DSPCMD_Subindex++ == 0)
                VSB_SampleRate = value << 8; /* hibyte first */
            else {
                VSB_SampleRate &= ~0xFF;
                VSB_SampleRate |= value;
            }
            break;
#if SILENCE
        case SB_DSP_SILENCE_DAC: /* 80 - output silence samples */
            if(VSB_DSPCMD_Subindex++ == 0)
                VSB_Samples = value;
            else {
                VSB_Samples |= value << 8; /* the value is #samples-1! */
                VSB_MixerRegs[SB_MIXERREG_MODEFILTER] &= ~0x2; /* reset stereo */
                VSB_Signed = false;
                VSB_Bits = 8;
                VSB_Pos = 0;
                VSB_Silent = true;
                VSB_Started = true;
            }
            break;
#endif
        case SB_DSP_ID: /* E0: supposed to return bitwise NOT of data byte */
            pData = DataBuffer;
            DSPDataBytes = 1;
            DataBuffer[0] = value ^ 0xFF;
            break;
        case SB_DSP_WRITE_TESTREG: /* E4 */
            VSB_TestReg = value;
            VSB_DSPCMD_Subindex = 2; //only 1byte
            break;
        } /* end switch */
        if( VSB_DSPCMD_Subindex >= 2 )
            VSB_DSPCMD = -1;
    } /* endif DSPCMD == -1 */
    if(VSB_Started != OldStarted ) {
        dbgprintf(("DSP_Write exit, VSB_Started=%u\n", VSB_Started ));
    }
}

/* read port 02xA */

static uint8_t DSP_Read( void )
///////////////////////////////
{
    if ( DSPDataBytes ) {
        dbgprintf(("DSP_Read: %X\n", *pData ));
        DSPDataBytes--;
        VSB_RS &= 0x7F;
        return( *(pData++));
    }
    dbgprintf(("DSP_Read: read buffer empty (FFh)\n"));
    return 0xFF;
}

/* read port 02xC
 * bit 7=0 means DSP is ready to receive cmd/data
 */

static uint8_t DSP_WriteStatus( void )
//////////////////////////////////////
{
    //dbgprintf(("DSP_WriteStatus (bit 7=0 means DSP ready for cmd/data)\n"));
    //return 0; //ready for write (bit7 clear)
    VSB_WS += 0x80; //some games will wait on busy first
    return VSB_WS;
}

/* read status register 02xE */

static uint8_t DSP_ReadStatus( void )
/////////////////////////////////////
{
    VSB_MixerRegs[SB_MIXERREG_IRQ_STATUS] &= ~0x1; /* bit 0-2: IRQ 8,16,MIDI */

    if ( DSPDataBytes || ( VSB_ResetState != VSB_RESET_END ) )
        VSB_RS |= 0x80;
    dbgprintf(("DSP_ReadStatus=%X\n", VSB_RS));

    return VSB_RS;
}

/* read port 02xF */

static uint8_t DSP_INT16ACK( void )
///////////////////////////////////
{
    dbgprintf(("DSP_INT16ACK\n"));
    VSB_MixerRegs[SB_MIXERREG_IRQ_STATUS] &= ~0x2;
    return 0xFF;
}

void VSB_Init(int irq, int dma, int hdma, int type )
////////////////////////////////////////////////////
{
    VSB_IRQ = irq;
    VSB_DMA = dma;
#if SB16
    VSB_HDMA = hdma;
#endif
    VSB_DSPVER = VSB_DSPVersion[type];
    if ( VSB_DSPVER >= 0x400 ) {
        switch ( VSB_DSPVER & 0xFF ) {
        case 0x5: VSB_MixerRegs[SB_MIXERREG_IRQ_STATUS] |= 0x20; break;
        case 0x12: VSB_MixerRegs[SB_MIXERREG_IRQ_STATUS] |= 0x80; break;
        default: VSB_MixerRegs[SB_MIXERREG_IRQ_STATUS] |= 0x10;
        }
    }
    VSB_Mixer_WriteAddr( SB_MIXERREG_RESET );
    VSB_Mixer_Write( 1 );
}

uint8_t VSB_GetIRQ()
////////////////////
{
    int bit;
    if(VSB_MixerRegs[SB_MIXERREG_INT_SETUP] == 0)
        return 0xFF;
    bit = BSF(VSB_MixerRegs[SB_MIXERREG_INT_SETUP]);
    if(bit >= 4)
        return 0xFF;
    return VSB_IRQMap[bit];
}

uint8_t VSB_GetDMA()
////////////////////
{
#if SB16
    if ( VSB_Bits > 8 ) {
        if( VSB_MixerRegs[SB_MIXERREG_DMA_SETUP] & 0xF0 )
            return( BSF(VSB_MixerRegs[SB_MIXERREG_DMA_SETUP]>>4) + 4 );
    }
#endif
    if( VSB_MixerRegs[SB_MIXERREG_DMA_SETUP] )
        return( BSF(VSB_MixerRegs[SB_MIXERREG_DMA_SETUP] ) );
    return 0xFF;
}

#if SB16
uint8_t VSB_GetHDMA()
/////////////////////
{
    int bit;
    if( !(VSB_MixerRegs[SB_MIXERREG_DMA_SETUP] & 0xF0 ))
        return 0xFF;
    bit = BSF(VSB_MixerRegs[SB_MIXERREG_DMA_SETUP]>>4) + 4;
    return bit;
}
#endif

int VSB_Running()
/////////////////
{
    return VSB_Started;
}

int VSB_IsSilent()
/////////////////
{
    return VSB_Silent;
}

void VSB_Stop()
///////////////
{
    VSB_Started = false;
    VSB_HighSpeed = false;
    VSB_Pos = 0;
}

int VSB_GetDACSpeaker()
///////////////////////
{
    return VSB_DACSpeaker;
}

unsigned int VSB_GetBits()
//////////////////////////
{
    return VSB_Bits;
}

int VSB_IsSigned()
//////////////////
{
    return VSB_Signed;
}

int VSB_GetChannels()
/////////////////////
{
    return (VSB_MixerRegs[SB_MIXERREG_MODEFILTER] & 0x2) ? 2 : 1;
}

int VSB_GetSampleRate()
///////////////////////
{
#if LATERATE
	if ( !VSB_SampleRate ) {
		for(int i = 0; i < 3; ++i) {
			if( bTimeConst >= VSB_TimeConstantMapMono[i][0]-3 && bTimeConst <= VSB_TimeConstantMapMono[i][0]+3) {
				VSB_SampleRate = VSB_TimeConstantMapMono[i][1] / VSB_GetChannels();
				break;
			}
		}
		if( VSB_SampleRate == 0 )
			VSB_SampleRate = 256000000/( 65536 - ( bTimeConst << 8) ) / VSB_GetChannels();
		//VSB_SampleRate = 1000000 / ( 256 - bTimeConst ) / VSB_GetChannels();
	}
#endif
    return VSB_SampleRate;
}

int VSB_GetSampleBytes()
////////////////////////
{
	//   return VSB_Samples + 1;
	//   return(( VSB_Samples + 1 ) * VSB_Bits / 8 );
	//   return((VSB_Samples + 1) * max(1, VSB_Bits >> 3));
    //if ( !VSB_Samples ) asm("int3"); /* 1 sample, used by card detection software */
    return((VSB_Samples + 1) * ((VSB_Bits+7) >> 3));
}

int VSB_GetAuto()
/////////////////
{
    return VSB_Auto;
}

int VSB_GetPos()
////////////////
{
    return VSB_Pos;
}

int VSB_SetPos(int pos)
///////////////////////
{
    if(pos >= VSB_GetSampleBytes())
        VSB_MixerRegs[SB_MIXERREG_IRQ_STATUS] |= ((VSB_GetBits() <= 8 ) ? 0x01 : 0x02);
    return VSB_Pos = pos;
}

void VSB_SetIRQStatus( void )
/////////////////////////////
{
    VSB_MixerRegs[SB_MIXERREG_IRQ_STATUS] |= ((VSB_GetBits() <= 8 ) ? 0x01 : 0x02);
}

#if 0
int VSB_IRQTriggered()
//////////////////////
{
    return VSB_TriggerIRQ;
}

void VSB_ResetTriggeredIRQ()
////////////////////////////
{
    VSB_TriggerIRQ = 0;
}
#endif

uint8_t VSB_GetMixerReg(uint8_t index)
//////////////////////////////////////
{
    return VSB_MixerRegs[index];
}

uint32_t VSB_MixerAddr( uint32_t port, uint32_t val, uint32_t out )
///////////////////////////////////////////////////////////////////
{
    return out ? (VSB_Mixer_WriteAddr( val ), val) : val;
}
uint32_t VSB_MixerData( uint32_t port, uint32_t val, uint32_t out )
///////////////////////////////////////////////////////////////////
{
	return out ? (VSB_Mixer_Write( val ), val) : (val &= ~0xFF, val |= VSB_Mixer_Read() );
}
uint32_t VSB_DSP_Reset( uint32_t port, uint32_t val, uint32_t out )
///////////////////////////////////////////////////////////////////
{
    return out ? (DSP_Reset( val ), val) : val;
}

/* read/write DSP "read data"
 * port offset 0Ah
 * data is available if "read status" bit 7=1
 */

uint32_t VSB_DSP_Read( uint32_t port, uint32_t val, uint32_t out )
//////////////////////////////////////////////////////////////////
{
    return out ? val : (val &=~0xFF, val |= DSP_Read());
}

/* read/write DSP "write data or command"
 * port offset 0Ch
 */

uint32_t VSB_DSP_Write( uint32_t port, uint32_t val, uint32_t out )
///////////////////////////////////////////////////////////////////
{
    return out ? (DSP_Write( val ), val) : DSP_WriteStatus();
}

/* read/write DSP "read status"
 * port offset 0Eh
 * data is available if read status bit 7=1
 * a read also works as 8-bit "IRQ ack"
 */
uint32_t VSB_DSP_ReadStatus( uint32_t port, uint32_t val, uint32_t out )
////////////////////////////////////////////////////////////////////////
{
    return out ? val : (val &= ~0xFF, val |= DSP_ReadStatus());
}
uint32_t VSB_DSP_ReadINT16BitACK( uint32_t port, uint32_t val, uint32_t out )
/////////////////////////////////////////////////////////////////////////////
{
    return out ? val : (val &= ~0xFF, val |= DSP_INT16ACK());
}
