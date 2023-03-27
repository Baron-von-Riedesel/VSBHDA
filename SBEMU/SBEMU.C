
#include <string.h>
#include <stdlib.h>
#include "SBEMUCFG.H"
#include "PLATFORM.H"
#include "SBEMU.H"
#include "CTADPCM.H"
#include "VIRQ.H"

typedef struct 
{
    int step;
    uint8_t ref;
    uint8_t useRef;
}ADPCM_STATE;

void MAIN_Uninstall( void );

#define SBEMU_RESET_START 0
#define SBEMU_RESET_END 1
#define SBEMU_RESET_POLL 2

static int SBEMU_ResetState = SBEMU_RESET_END;
static int SBEMU_Started = 0;
static int SBEMU_IRQ = 5;
static int SBEMU_DMA = 1;
#if SB16
static int SBEMU_HDMA = 5;
#endif
static int SBEMU_DACSpeaker = 1;
static int SBEMU_Bits = 8;
static int SBEMU_SampleRate = 22050;
static int SBEMU_Samples = 0;
static int SBEMU_Auto = 0;
static int SBEMU_HighSpeed = 0;
static int SBEMU_DSPCMD = -1;
static int SBEMU_DSPCMD_Subindex = 0;
static int SBEMU_DSPDATA_Subindex = 0;
int SBEMU_TriggerIRQ = 0;
static int SBEMU_Pos = 0;
static uint8_t SBEMU_IRQMap[4] = {2,5,7,10};
static uint8_t SBEMU_MixerRegIndex = 0;
static uint8_t SBEMU_idbyte;
static uint8_t SBEMU_WS = 0x80;
static uint8_t SBEMU_RS = 0x2A;
static uint16_t SBEMU_DSPVER = 0x0302;
#if ADPCM
static ADPCM_STATE SBEMU_ADPCM;
#endif
static int SBEMU_TimeConstantMapMono[][2] =
{
    0xA5, 11025,
    0xD2, 22050,
    0xE9, 44100,
};
static uint8_t SBEMU_Copyright[] = "COPYRIGHT (C) CREATIVE TECHNOLOGY LTD, 1992.";

static uint8_t SBEMU_MixerRegs[256];

static int SBEMU_Indexof(uint8_t* array, int count, uint8_t  val)
/////////////////////////////////////////////////////////////////
{
	for(int i = 0; i < count; ++i) {
		if(array[i] == val)
			return i;
	}
	return -1;
}


static void SBEMU_Mixer_WriteAddr( uint8_t value )
//////////////////////////////////////////////////
{
    dbgprintf("SBEMU_Mixer_WriteAddr: value=%x\n", value);
    SBEMU_MixerRegIndex = value;
}

static void SBEMU_Mixer_Write( uint8_t value )
//////////////////////////////////////////////
{
	dbgprintf("SBEMU_Mixer_Write: value=%x\n", value);
	SBEMU_MixerRegs[SBEMU_MixerRegIndex] = value;
	if(SBEMU_MixerRegIndex == SBEMU_MIXERREG_RESET) {
		SBEMU_MixerRegs[SBEMU_MIXERREG_MASTERVOL] = 0xE; //3:(1). default 4
		SBEMU_MixerRegs[SBEMU_MIXERREG_MIDIVOL] = 0xE;
		SBEMU_MixerRegs[SBEMU_MIXERREG_VOICEVOL] = 0x6; //(1):2:(1) deault 0

		if(SBEMU_DSPVER < 0x0400) { //before SB16
			SBEMU_MixerRegs[SBEMU_MIXERREG_VOICESTEREO] = 0xEE;
			SBEMU_MixerRegs[SBEMU_MIXERREG_MASTERSTEREO] = 0xEE;
			SBEMU_MixerRegs[SBEMU_MIXERREG_MIDISTEREO] = 0xEE;
        } else { //SB16
			SBEMU_MixerRegs[SBEMU_MIXERREG_VOICESTEREO] = 0xFF;
			SBEMU_MixerRegs[SBEMU_MIXERREG_MASTERSTEREO] = 0xFF;
			SBEMU_MixerRegs[SBEMU_MIXERREG_MIDISTEREO] = 0xFF;
			SBEMU_MixerRegs[SBEMU_MIX16REG_MASTERL] = 0xF8;
			SBEMU_MixerRegs[SBEMU_MIX16REG_MASTERR] = 0xF8;
			SBEMU_MixerRegs[SBEMU_MIX16REG_VOICEL] = 0xF8;
			SBEMU_MixerRegs[SBEMU_MIX16REG_VOICER] = 0xF8;
			SBEMU_MixerRegs[SBEMU_MIX16REG_MIDIL] = 0xF8;
			SBEMU_MixerRegs[SBEMU_MIX16REG_MIDIR] = 0xF8;
		}
	}
#if SB16
	if(SBEMU_DSPVER >= 0x0400) { //SB16
		if(SBEMU_MixerRegIndex >= SBEMU_MIX16REG_MASTERL && SBEMU_MixerRegIndex <= SBEMU_MIX16REG_MIDIR) {
			//5bits, drop 1 bit
			value = (value >> 4) & 0xF;
			switch(SBEMU_MixerRegIndex) {
			case SBEMU_MIX16REG_MASTERL: SBEMU_MixerRegs[SBEMU_MIXERREG_MASTERSTEREO] &= 0x0F; SBEMU_MixerRegs[SBEMU_MIXERREG_MASTERSTEREO] |= (value<<4); break;
			case SBEMU_MIX16REG_MASTERR: SBEMU_MixerRegs[SBEMU_MIXERREG_MASTERSTEREO] &= 0xF0; SBEMU_MixerRegs[SBEMU_MIXERREG_MASTERSTEREO] |= value; break;
			case SBEMU_MIX16REG_VOICEL:  SBEMU_MixerRegs[SBEMU_MIXERREG_VOICESTEREO]  &= 0x0F; SBEMU_MixerRegs[SBEMU_MIXERREG_VOICESTEREO]  |= (value<<4); break;
			case SBEMU_MIX16REG_VOICER:  SBEMU_MixerRegs[SBEMU_MIXERREG_VOICESTEREO]  &= 0xF0; SBEMU_MixerRegs[SBEMU_MIXERREG_VOICESTEREO]  |= value; break;
			case SBEMU_MIX16REG_MIDIL:   SBEMU_MixerRegs[SBEMU_MIXERREG_MIDISTEREO]   &= 0x0F; SBEMU_MixerRegs[SBEMU_MIXERREG_MIDISTEREO]   |= (value<<4); break;
			case SBEMU_MIX16REG_MIDIR:   SBEMU_MixerRegs[SBEMU_MIXERREG_MIDISTEREO]   &= 0xF0; SBEMU_MixerRegs[SBEMU_MIXERREG_MIDISTEREO]   |= value; break;
			}
		}
	}
#endif
}

static uint8_t SBEMU_Mixer_Read( void )
///////////////////////////////////////
{
	dbgprintf("SBEMU: mixer read: %x\n", SBEMU_MixerRegs[SBEMU_MixerRegIndex]);
	return SBEMU_MixerRegs[SBEMU_MixerRegIndex];
}

static void SBEMU_DSP_Reset( uint8_t value )
////////////////////////////////////////////
{
    dbgprintf("SBEMU: DSP reset: %d\n",value);
    if(value == 1)
    {
        SBEMU_ResetState = SBEMU_RESET_START;
        SBEMU_MixerRegs[SBEMU_MIXERREG_INT_SETUP] = 1<<SBEMU_Indexof(SBEMU_IRQMap,countof(SBEMU_IRQMap),SBEMU_IRQ);
        //SBEMU_MixerRegs[SBEMU_MIXERREG_DMA_SETUP] = (1<<SBEMU_DMA)&0xEB;
#if SB16
        SBEMU_MixerRegs[SBEMU_MIXERREG_DMA_SETUP] = ( (1<<SBEMU_DMA) | ( SBEMU_HDMA ? (1<<SBEMU_HDMA) : 0)) & 0xEB;
#else
        SBEMU_MixerRegs[SBEMU_MIXERREG_DMA_SETUP] = (1<<SBEMU_DMA) & 0xB;
#endif
        SBEMU_MixerRegs[SBEMU_MIXERREG_MODEFILTER] = 0xFD; //mask out stereo
        SBEMU_MixerRegIndex = 0;
        SBEMU_DSPCMD = -1;
        SBEMU_DSPCMD_Subindex = 0;
        SBEMU_DSPDATA_Subindex = 0;
        SBEMU_Started = 0;
        SBEMU_Samples = 0;
        SBEMU_Auto = 0;
        SBEMU_Bits = 8;
        SBEMU_Pos = 0;
        SBEMU_HighSpeed = 0;
        SBEMU_TriggerIRQ = 0;
        SBEMU_Mixer_WriteAddr( SBEMU_MIXERREG_RESET );
        SBEMU_Mixer_Write( 1 );
    }
    if(value == 0 && SBEMU_ResetState == SBEMU_RESET_START)
        SBEMU_ResetState = SBEMU_RESET_POLL;

    /* uninstall */
    if( value == 0x55 && SBEMU_ResetState == SBEMU_RESET_START )
        MAIN_Uninstall();
}

//void MAIN_Interrupt( void );

static void SBEMU_DSP_Write( uint8_t value )
////////////////////////////////////////////
{
    dbgprintf("SBEMU_DSP_Write %02x, DSPCMD=%02x\n", value, SBEMU_DSPCMD);
    if(SBEMU_HighSpeed) //highspeed won't accept further commands, need reset
        return;
    int OldStarted = SBEMU_Started;
    SBEMU_WS = 0x80;
    if( SBEMU_DSPCMD == -1 ) {
        //SBEMU_DSPCMD = value;
        switch( value ) { /* handle 1-byte cmds here */
        case SBEMU_CMD_TRIGGER_IRQ: /* F2 */
#if SB16
        case SBEMU_CMD_TRIGGER_IRQ16: /* F3 */
#endif
            SBEMU_MixerRegs[SBEMU_MIXERREG_INT_STS] |= ( value == SBEMU_CMD_TRIGGER_IRQ ? 0x1 : 0x2 );
#if TRIGGERATONCE
# if 1
            /* no need to reset SBEMU_TriggerIRQ in MAIN_Interrupt() */
            VIRQ_Invoke(SBEMU_GetIRQ());
# else
            /* calling MAIN_Interrupt() directly might cause problems if SETIF is 1 */
            SBEMU_TriggerIRQ = 1;
            //MAIN_Interrupt(); /* will reset SBEMU_TriggerIRQ */
# endif
#endif
            break;
        case SBEMU_CMD_DAC_SPEAKER_ON: /* D1 */
        case SBEMU_CMD_DAC_SPEAKER_OFF: /* D3 */
            break;
        case SBEMU_CMD_HALT_DMA: /* D0 */
        case SBEMU_CMD_CONTINUE_DMA: /* D4 */
            SBEMU_Started = ( value == SBEMU_CMD_CONTINUE_DMA );
            break;
#if SB16
        case SBEMU_CMD_HALT_DMA16: /* D5 */
        case SBEMU_CMD_CONTINUE_DMA16: /* D6 */
            SBEMU_Started = ( value == SBEMU_CMD_CONTINUE_DMA16 );
            break;
#endif
        case SBEMU_CMD_8BIT_OUT_AUTO_HS: /* 90 */
        case SBEMU_CMD_8BIT_OUT_AUTO: /* 1C */
            SBEMU_Auto = TRUE;
            SBEMU_Bits = 8;
            SBEMU_HighSpeed = ( value == SBEMU_CMD_8BIT_OUT_AUTO_HS );
            SBEMU_Started = TRUE; //start transfer
            SBEMU_Pos = 0;
            break;
#if ADPCM
        case SBEMU_CMD_2BIT_OUT_AUTO: /* 1F */
        case SBEMU_CMD_3BIT_OUT_AUTO: /* 7F */
        case SBEMU_CMD_4BIT_OUT_AUTO: /* 7D */
			SBEMU_Auto = TRUE;
			SBEMU_ADPCM.useRef = TRUE;
			SBEMU_ADPCM.step = 0;
			SBEMU_Bits = (value <= SBEMU_CMD_2BIT_OUT_1_NREF) ? 2 : (value >= SBEMU_CMD_3BIT_OUT_1_NREF) ? 3 : 4;
			SBEMU_MixerRegs[SBEMU_MIXERREG_MODEFILTER] &= ~0x2;
			SBEMU_Started = TRUE; //start transfer here
			SBEMU_Pos = 0;
            break;
#endif
#if SB16
        case SBEMU_CMD_EXIT_16BIT_AUTO: /* D9 */
        case SBEMU_CMD_EXIT_8BIT_AUTO:  /* DA */
            if( SBEMU_Auto ) {
                SBEMU_Auto = FALSE;
                SBEMU_Started = FALSE;
            }
            break;
#endif
        case 0x2A: //unknown commands
            break;
        default:
            SBEMU_DSPCMD = value;
            SBEMU_DSPCMD_Subindex = 0;
        }
#if SB16
	} else if ( SBEMU_DSPCMD >= 0xB0 && SBEMU_DSPCMD <= 0xCF ) {
		//SBEMU_Fifo = ( SBEMU_DSPCMD & 0x2 ) ? 1 : 0;
		switch ( SBEMU_DSPCMD_Subindex ) {
		case 0:
			SBEMU_Auto = ( ( SBEMU_DSPCMD & 0x4 ) ? 1 : 0 );
			SBEMU_Bits = ( ( SBEMU_DSPCMD & 0x40 ) ? 8 : 16 );
			/* bit 4 of value: 1=signed */
			/* bit 5 of value: 1=stereo */
			SBEMU_MixerRegs[SBEMU_MIXERREG_MODEFILTER] |= ( ( value & 0x20 ) ? 2 : 0 );
			SBEMU_DSPCMD_Subindex++;
			break;
		case 1:
			SBEMU_Samples = value;
			SBEMU_DSPCMD_Subindex++;
			break;
		default:
			SBEMU_Samples |= value<<8;
			SBEMU_DSPCMD = -1;
			SBEMU_Started = TRUE;
			SBEMU_Pos = 0;
		}
#endif
	} else {
		switch(SBEMU_DSPCMD) {
		case SBEMU_CMD_SET_TIMECONST: /* 40 */
			SBEMU_SampleRate = 0;
			for(int i = 0; i < 3; ++i) {
				if(value >= SBEMU_TimeConstantMapMono[i][0]-3 && value <= SBEMU_TimeConstantMapMono[i][0]+3) {
					SBEMU_SampleRate = SBEMU_TimeConstantMapMono[i][1] / SBEMU_GetChannels();
					break;
				}
			}
			if(SBEMU_SampleRate == 0)
				SBEMU_SampleRate = 256000000/(65536-(value<<8)) / SBEMU_GetChannels();
			SBEMU_DSPCMD_Subindex = 2; //only 1byte
			dbgprintf("SBEMU_DSP_Write SET_TIMECONST: set sampling rate: %d\n", SBEMU_SampleRate);
			break;
		case SBEMU_CMD_SET_SIZE: /* 48 - used for auto command */
		case SBEMU_CMD_8BIT_OUT_1_HS: /* 91 */
		case SBEMU_CMD_8BIT_OUT_1: /* 14 */
			if(SBEMU_DSPCMD_Subindex++ == 0)
				SBEMU_Samples = value;
			else {
				SBEMU_Samples |= value << 8;
				SBEMU_HighSpeed = ( SBEMU_DSPCMD == SBEMU_CMD_8BIT_OUT_AUTO_HS );
				if ( SBEMU_DSPCMD == SBEMU_CMD_8BIT_OUT_1 || SBEMU_DSPCMD == SBEMU_CMD_8BIT_OUT_1_HS ) {
					SBEMU_Started = TRUE;
					SBEMU_Bits = 8;
					SBEMU_Pos = 0;
				}
			}
			break;
#if ADPCM
		case SBEMU_CMD_2BIT_OUT_1: /* 16 */
		case SBEMU_CMD_2BIT_OUT_1_NREF: /* 17 */
		case SBEMU_CMD_3BIT_OUT_1:
		case SBEMU_CMD_3BIT_OUT_1_NREF:
		case SBEMU_CMD_4BIT_OUT_1:
		case SBEMU_CMD_4BIT_OUT_1_NREF:
			if(SBEMU_DSPCMD_Subindex++ == 0)
				SBEMU_Samples = value;
			else {
				SBEMU_Samples |= value<<8;
				SBEMU_Auto = FALSE;
				SBEMU_ADPCM.useRef = (SBEMU_DSPCMD==SBEMU_CMD_2BIT_OUT_1 || SBEMU_DSPCMD==SBEMU_CMD_3BIT_OUT_1 || SBEMU_DSPCMD==SBEMU_CMD_4BIT_OUT_1);
				SBEMU_ADPCM.step = 0;
				SBEMU_Bits = (SBEMU_DSPCMD<=SBEMU_CMD_2BIT_OUT_1_NREF) ? 2 : (SBEMU_DSPCMD>=SBEMU_CMD_3BIT_OUT_1_NREF) ? 3 : 4;
				SBEMU_MixerRegs[SBEMU_MIXERREG_MODEFILTER] &= ~0x2;
				SBEMU_Started = TRUE; //start transfer here
				SBEMU_Pos = 0;
			}
			break;
#endif
		case SBEMU_CMD_SET_SAMPLERATE_I: /* 42 */
			SBEMU_DSPCMD_Subindex++;
			break;
		case SBEMU_CMD_SET_SAMPLERATE: /* 41 - command start: sample rate */
			if(SBEMU_DSPCMD_Subindex++ == 0)
				SBEMU_SampleRate = value << 8; /* hibyte first */
			else {
				SBEMU_SampleRate &= ~0xFF;
				SBEMU_SampleRate |= value;
			}
			break;
		case SBEMU_CMD_DSP_ID: /* E0 */
			SBEMU_idbyte = value;
			break;
		} /* end switch */
		if( SBEMU_DSPCMD_Subindex >= 2 )
			SBEMU_DSPCMD = -1;
	} /* endif DSPCMD == -1 */
	if(SBEMU_Started != OldStarted ) {
		dbgprintf("SBEMU_DSP_Write exit, SBEMU_Started=%u\n", SBEMU_Started );
	}
}

static uint8_t SBEMU_DSP_Read( void )
/////////////////////////////////////
{
    if( SBEMU_ResetState == SBEMU_RESET_POLL || SBEMU_ResetState == SBEMU_RESET_START ) {
        SBEMU_ResetState = SBEMU_RESET_END;
        dbgprintf("SBEMU_DSP_Read: AAh\n");
        return 0xAA; //reset ready
    }
    if( SBEMU_DSPCMD == SBEMU_CMD_DSP_GETVER ) {
        //https://github.com/joncampbell123/dosbox-x/wiki/Hardware:Sound-Blaster:DSP-commands:0xE1
        if(SBEMU_DSPDATA_Subindex++ == 0)
            return SBEMU_DSPVER>>8;
        else {
            SBEMU_DSPDATA_Subindex = 0;
            SBEMU_DSPCMD = -1;
            dbgprintf("SBEMU_DSP_Read GETVER: %X\n", SBEMU_DSPVER );
            return SBEMU_DSPVER & 0xFF;
        }
    } else if(SBEMU_DSPCMD == SBEMU_CMD_DSP_ID) {
        SBEMU_DSPCMD = -1;
        SBEMU_DSPDATA_Subindex = 0;
        dbgprintf("SBEMU_DSP_Read ID: %X\n", SBEMU_idbyte ^ 0xFF );
        return SBEMU_idbyte ^ 0xFF;
    } else if(SBEMU_DSPCMD == SBEMU_CMD_DSP_COPYRIGHT) {
        if(SBEMU_DSPDATA_Subindex == sizeof(SBEMU_Copyright)-1) {
            SBEMU_DSPCMD = -1;
            SBEMU_DSPDATA_Subindex = 0;
            return 0xFF;
        }
        dbgprintf("SBEMU_DSP_Read COPYRIGHT: %X\n", SBEMU_Copyright[SBEMU_DSPDATA_Subindex++] );
        return SBEMU_Copyright[SBEMU_DSPDATA_Subindex++];
    }
    return 0xFF;
}

/* read "data/command" port
 * bit 7=0 means DSP is ready to receive cmd/data
 */

static uint8_t SBEMU_DSP_WriteStatus( void )
////////////////////////////////////////////
{
    //dbgprintf("SBEMU_DSP_WriteStatus (bit 7=0 means DSP ready for cmd/data)\n");
    //return 0; //ready for write (bit7 clear)
    SBEMU_WS += 0x80; //some games will wait on busy first
    return SBEMU_WS;
}

static uint8_t SBEMU_DSP_ReadStatus( void )
///////////////////////////////////////////
{
    dbgprintf("SBEMU_DSP_ReadStatus\n");
/*
    if(SBEMU_ResetState == SBEMU_RESET_POLL || SBEMU_ResetState == SBEMU_RESET_START
    || SBEMU_DSPCMD == SBEMU_CMD_DSP_GETVER
    || SBEMU_DSPCMD == SBEMU_CMD_DSP_ID
    || SBEMU_DSPCMD == SBEMU_CMD_DSP_COPYRIGHT)
        return 0xFF; //ready for read (bit7 set)
*/
    SBEMU_RS += 0x80;
    SBEMU_MixerRegs[SBEMU_MIXERREG_INT_STS] &= ~0x1;
    //SBEMU_TriggerIRQ = 0;
    return SBEMU_RS;
}

static uint8_t SBEMU_DSP_INT16ACK( void )
/////////////////////////////////////////
{
    SBEMU_MixerRegs[SBEMU_MIXERREG_INT_STS] &= ~0x2;
    return 0xFF;
}

void SBEMU_Init(int irq, int dma, int hdma, int DSPVer )
////////////////////////////////////////////////////////
{
    SBEMU_IRQ = irq;
    SBEMU_DMA = dma;
#if SB16
    SBEMU_HDMA = hdma;
#endif
    SBEMU_DSPVER = DSPVer;
    SBEMU_Mixer_WriteAddr( SBEMU_MIXERREG_RESET );
    SBEMU_Mixer_Write( 1 );
}

uint8_t SBEMU_GetIRQ()
//////////////////////
{
    if(SBEMU_MixerRegs[SBEMU_MIXERREG_INT_SETUP] == 0)
        return 0xFF;
    int bit = BSF(SBEMU_MixerRegs[SBEMU_MIXERREG_INT_SETUP]);
    if(bit >= 4)
        return 0xFF;
    return SBEMU_IRQMap[bit];
}

uint8_t SBEMU_GetDMA()
//////////////////////
{
#if SB16
    if ( SBEMU_Bits > 8 ) {
        if( SBEMU_MixerRegs[SBEMU_MIXERREG_DMA_SETUP] & 0xF0 )
            return( BSF(SBEMU_MixerRegs[SBEMU_MIXERREG_DMA_SETUP]>>4) + 4 );
    }
#endif
    if( SBEMU_MixerRegs[SBEMU_MIXERREG_DMA_SETUP] )
        return( BSF(SBEMU_MixerRegs[SBEMU_MIXERREG_DMA_SETUP] ) );
    return 0xFF;
}

#if SB16
uint8_t SBEMU_GetHDMA()
///////////////////////
{
    if( !(SBEMU_MixerRegs[SBEMU_MIXERREG_DMA_SETUP] & 0xF0 ))
        return 0xFF;
    int bit = BSF(SBEMU_MixerRegs[SBEMU_MIXERREG_DMA_SETUP]>>4) + 4;
    return bit;
}
#endif

int SBEMU_HasStarted()
//////////////////////
{
    return SBEMU_Started;
}

void SBEMU_Stop()
/////////////////
{
    SBEMU_Started = FALSE;
    SBEMU_HighSpeed = FALSE;
    SBEMU_Pos = 0;
}

int SBEMU_GetDACSpeaker()
/////////////////////////
{
    return SBEMU_DACSpeaker;
}

int SBEMU_GetBits()
///////////////////
{
    return SBEMU_Bits;
}

int SBEMU_GetChannels()
///////////////////////
{
    return (SBEMU_MixerRegs[SBEMU_MIXERREG_MODEFILTER] & 0x2) ? 2 : 1;
}

int SBEMU_GetSampleRate()
/////////////////////////
{
    return SBEMU_SampleRate;
}

int SBEMU_GetSampleBytes()
//////////////////////////
{
 //   return SBEMU_Samples + 1;
 //   return(( SBEMU_Samples + 1 ) * SBEMU_Bits / 8 );
    return((SBEMU_Samples + 1) * max(1, SBEMU_Bits >> 3));
}

int SBEMU_GetAuto()
///////////////////
{
    return SBEMU_Auto;
}

int SBEMU_GetPos()
//////////////////
{
    return SBEMU_Pos;
}

int SBEMU_SetPos(int pos)
/////////////////////////
{
    if(pos >= SBEMU_GetSampleBytes())
        SBEMU_MixerRegs[SBEMU_MIXERREG_INT_STS] |= SBEMU_GetBits() <= 8 ? 0x01 : 0x02;
    return SBEMU_Pos = pos;
}

#if 0
int SBEMU_IRQTriggered()
////////////////////////
{
    return SBEMU_TriggerIRQ;
}

void SBEMU_ResetTriggeredIRQ()
//////////////////////////////
{
    SBEMU_TriggerIRQ = 0;
}
#endif

uint8_t SBEMU_GetMixerReg(uint8_t index)
////////////////////////////////////////
{
    return SBEMU_MixerRegs[index];
}

#if ADPCM
int SBEMU_DecodeADPCM(uint8_t* adpcm, int bytes)
////////////////////////////////////////////////
{
    int start = 0;
    if(SBEMU_ADPCM.useRef)
    {
        SBEMU_ADPCM.useRef = FALSE;
        SBEMU_ADPCM.ref = *adpcm;
        SBEMU_ADPCM.step = 0;
        ++start;
    }

    int outbytes = bytes * (9/SBEMU_Bits);
    uint8_t* pcm = (uint8_t*)malloc(outbytes);
    int outcount = 0;

	switch ( SBEMU_Bits ) {
	case 2:
		for(int i = start; i < bytes; ++i) {
			pcm[outcount++] = decode_ADPCM_2_sample((adpcm[i] >> 6) & 0x3, &SBEMU_ADPCM.ref, &SBEMU_ADPCM.step);
			pcm[outcount++] = decode_ADPCM_2_sample((adpcm[i] >> 4) & 0x3, &SBEMU_ADPCM.ref, &SBEMU_ADPCM.step);
			pcm[outcount++] = decode_ADPCM_2_sample((adpcm[i] >> 2) & 0x3, &SBEMU_ADPCM.ref, &SBEMU_ADPCM.step);
			pcm[outcount++] = decode_ADPCM_2_sample((adpcm[i] >> 0) & 0x3, &SBEMU_ADPCM.ref, &SBEMU_ADPCM.step);
		}
	case 3:
		for(int i = start; i < bytes; ++i) {
			pcm[outcount++] = decode_ADPCM_3_sample((adpcm[i] >> 5) & 0x7, &SBEMU_ADPCM.ref, &SBEMU_ADPCM.step);
			pcm[outcount++] = decode_ADPCM_3_sample((adpcm[i] >> 2) & 0x7, &SBEMU_ADPCM.ref, &SBEMU_ADPCM.step);
			pcm[outcount++] = decode_ADPCM_3_sample((adpcm[i] & 0x3) << 1, &SBEMU_ADPCM.ref, &SBEMU_ADPCM.step);
		}
	default:
		for(int i = start; i < bytes; ++i) {
			pcm[outcount++] = decode_ADPCM_4_sample(adpcm[i] >> 4, &SBEMU_ADPCM.ref, &SBEMU_ADPCM.step);
			pcm[outcount++] = decode_ADPCM_4_sample(adpcm[i]& 0xf, &SBEMU_ADPCM.ref, &SBEMU_ADPCM.step);
		}
	}
    //assert(outcount <= outbytes);
    dbgprintf("SBEMU: adpcm decode: %d %d\n", outcount, outbytes);
    memcpy(adpcm, pcm, outcount);
    free(pcm);
    return outcount;
}
#endif

uint32_t SBEMU_SB_MixerAddr( uint32_t port, uint32_t val, uint32_t out )
////////////////////////////////////////////////////////////////////////
{
    return out ? (SBEMU_Mixer_WriteAddr( val ), val) : val;
}
uint32_t SBEMU_SB_MixerData( uint32_t port, uint32_t val, uint32_t out )
////////////////////////////////////////////////////////////////////////
{
    return out ? (SBEMU_Mixer_Write( val ), val) : (val &=~0xFF, val |= SBEMU_Mixer_Read() );
}
uint32_t SBEMU_SB_DSP_Reset( uint32_t port, uint32_t val, uint32_t out )
////////////////////////////////////////////////////////////////////////
{
    return out ? (SBEMU_DSP_Reset( val ), val) : val;
}

/* read/write DSP "read data"
 * port offset 0Ah
 * data is available if "read status" bit 7=1
 */

uint32_t SBEMU_SB_DSP_Read( uint32_t port, uint32_t val, uint32_t out )
///////////////////////////////////////////////////////////////////////
{
    return out ? val : (val &=~0xFF, val |= SBEMU_DSP_Read());
}

/* read/write DSP "write data or command"
 * port offset 0Ch
 */

uint32_t SBEMU_SB_DSP_Write( uint32_t port, uint32_t val, uint32_t out )
////////////////////////////////////////////////////////////////////////
{
    return out ? (SBEMU_DSP_Write( val ), val) : SBEMU_DSP_WriteStatus();
}

/* read/write DSP "read status"
 * port offset 0Eh
 * data is available if read status bit 7=1
 * a read also works as 8-bit "IRQ ack"
 */
uint32_t SBEMU_SB_DSP_ReadStatus( uint32_t port, uint32_t val, uint32_t out )
/////////////////////////////////////////////////////////////////////////////
{
    return out ? val : (val &=~0xFF, val |= SBEMU_DSP_ReadStatus());
}
uint32_t SBEMU_SB_DSP_ReadINT16BitACK( uint32_t port, uint32_t val, uint32_t out )
//////////////////////////////////////////////////////////////////////////////////
{
    return out ? val : (val &=~0xFF, val |= SBEMU_DSP_INT16ACK());
}

