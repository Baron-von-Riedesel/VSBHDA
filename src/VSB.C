
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "CONFIG.H"
#include "PLATFORM.H"
#include "VSB.H"
#include "VIRQ.H"
#include "VDMA.H"
#if VMPU
#include "VMPU.H"
#endif

#if REINITOPL
extern void MAIN_ReinitOPL( void );
#endif
extern void MAIN_Uninstall( void );

#define LATERATE 0 /* 1=store time constant and compute rate when required */

#if SB16
#define SB16_ONLY() if (vsb.DSPVER < 0x400 ) break
#else
#define SB16_ONLY() break
#endif

static const int VSB_DSPVersion[] =
{
    0,
    0x0100,
    0x0105, /* type 2: SB 1.5?  */
    0x0202, /* type 3: ?        */
    0x0302, /* type 4: SB Pro 2 */
    0x0302, /* type 5: ?        */
    0x0405, /* type 6: SB16     */
};

static const uint8_t VSB_IRQMap[] = {2,5,7,10};
#if 0
static const int VSB_TimeConstantMapMono[][2] = {
    0xA5, 11025,
    0xD2, 22050,
    0xE9, 44100,
};
#endif

// number of bytes in input for commands (sb/sbpro)
static const uint8_t DSP_cmd_len_sb[256] = {
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x00
//  1,0,0,0, 2,0,2,2, 0,0,0,0, 0,0,0,0,  // 0x10
  1,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x10 Wari hack
  0,0,0,0, 2,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
  0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,  // 0x30

  1,2,2,0, 0,0,0,0, 2,0,0,0, 0,0,0,0,  // 0x40
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x60
  0,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x70

  2,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x80
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x90
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xa0
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xb0

  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xc0
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xd0
  1,0,1,0, 1,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xe0
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0   // 0xf0
};

#if SB16
// number of bytes in input for commands (sb16)
static const uint8_t DSP_cmd_len_sb16[256] = {
  0,0,0,0, 1,2,0,0, 1,0,0,0, 0,0,2,1,  // 0x00
//  1,0,0,0, 2,0,2,2, 0,0,0,0, 0,0,0,0,  // 0x10
  1,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x10 Wari hack
  0,0,0,0, 2,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x20
  0,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0,  // 0x30

  1,2,2,0, 0,0,0,0, 2,0,0,0, 0,0,0,0,  // 0x40
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x50
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x60
  0,0,0,0, 2,2,2,2, 0,0,0,0, 0,0,0,0,  // 0x70

  2,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x80
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0x90
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xa0
  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xb0

  3,3,3,3, 3,3,3,3, 3,3,3,3, 3,3,3,3,  // 0xc0
  0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xd0
  1,0,1,0, 1,0,0,0, 0,0,0,0, 0,0,0,0,  // 0xe0
  0,0,0,0, 0,0,0,0, 0,1,0,0, 0,0,0,0   // 0xf0
};
#endif

static const uint8_t SB_Copyright[] = "COPYRIGHT (C) CREATIVE TECHNOLOGY LTD, 1992.";

#if ADPCM
extern ADPCM_STATE ISR_adpcm_state;
#endif

#define VSB_RESET_END 0
#define VSB_RESET_START 1
#define VSB_DIRECTBUFFER_SIZE 1024

struct VSB_Status {
    int SampleRate;        /* sample rate current op */
    unsigned int Samples;  /* the length argument after a play command, unmodified (samples - 1) */
    unsigned int Position; /* byte position in sample buffer? modified by VSB_SetPos() */
    unsigned int Bits;     /* bits current op */
    uint16_t DSPVER;

    uint8_t dsp_cmd;
    uint8_t dsp_cmd_len;
    uint8_t dsp_in_pos;
    uint8_t dsp_in_data[4];
    uint8_t Irq;      /* IRQ, set by Init (2/5/7) */
    uint8_t Dma8;     /* 8-bit DMA channel */
    uint8_t Dma16;    /* 16-bit DMA channel */
    uint8_t MixerRegIndex;
    uint8_t DirectIdx; /* current index for writes to DirectBuffer */
    uint8_t Started;  /* 1=DMA transfer started */
    uint8_t Auto; /* 1=auto-initialize mode active */
    uint8_t Silent;
    uint8_t Signed;
    uint8_t HighSpeed;
    uint8_t ResetState;
    uint8_t TestReg;
    uint8_t WS;
    uint8_t RS;
    uint8_t DMAID_A;
    uint8_t DMAID_X;
    uint8_t DataBytes; /* # of bytes to read from DataBuffer */
    uint8_t DataBuffer[48];
    uint8_t bSpeaker;
#if DISPSTAT
    uint8_t bDispStat;
#endif
#if LATERATE
    uint8_t bTimeConst;
#endif
    uint8_t MixerRegs[256];

    uint8_t DirectBuffer[VSB_DIRECTBUFFER_SIZE];
};

static struct VSB_Status vsb;

/* search item in table, return index if found, else -1 */

static int FindItem(const uint8_t* array, int count, uint8_t  val)
//////////////////////////////////////////////////////////////////
{
    int i;
    for( i = 0; i < count; ++i ) {
        if(array[i] == val)
            return i;
    }
    return -1;
}

/* write port 2x4 - set mixer index register */

static void VSB_Mixer_WriteAddr( uint8_t value )
////////////////////////////////////////////////
{
    dbgprintf(("VSB_Mixer_WriteAddr: Set MixerRegIndex to %u\n", value));
    vsb.MixerRegIndex = value;
}

static void VSB_MixerReset( void )
//////////////////////////////////
{
	vsb.MixerRegs[SB_MIXERREG_MASTERVOL] = 0xD; /* 02: bits 1-3, L&R?, default 0x99?, for SBPro+: map to 0x22? */
	vsb.MixerRegs[SB_MIXERREG_MIDIVOL] = 0xD;   /* 06: bits 1-3 */
	/* todo: SB_MIXERREG_VOICEVOL is for SB20 only, for SBPro+ it's MIC level 2/3 bits */
	vsb.MixerRegs[SB_MIXERREG_VOICEVOL] = 0x6;  /* 0A: bits 1-2, default ? */

	vsb.MixerRegs[SB_MIXERREG_VOICESTEREO] = 0xDD;  /* 04: */
	vsb.MixerRegs[SB_MIXERREG_MASTERSTEREO] = 0xDD; /* 22: */
	vsb.MixerRegs[SB_MIXERREG_MIDISTEREO] = 0xDD;   /* 26: */
#if SB16
	if(vsb.DSPVER >= 0x0400) { //SB16
		vsb.MixerRegs[SB16_MIXERREG_MASTERL] = 0xC0; /* 5 bits only (3-7) */
		vsb.MixerRegs[SB16_MIXERREG_MASTERR] = 0xC0;
		vsb.MixerRegs[SB16_MIXERREG_VOICEL] = 0xC0;
		vsb.MixerRegs[SB16_MIXERREG_VOICER] = 0xC0;
		vsb.MixerRegs[SB16_MIXERREG_MIDIL] = 0xC0;
		vsb.MixerRegs[SB16_MIXERREG_MIDIR] = 0xC0;
		vsb.MixerRegs[SB16_MIXERREG_TREBLEL] = 0x80;
		vsb.MixerRegs[SB16_MIXERREG_TREBLER] = 0x80;
		vsb.MixerRegs[SB16_MIXERREG_BASSL] = 0x80;
		vsb.MixerRegs[SB16_MIXERREG_BASSR] = 0x80;
	}
#endif
}

/* write port 2x5 */

static void VSB_Mixer_Write( uint8_t value )
////////////////////////////////////////////
{
    dbgprintf(("VSB_Mixer_Write[%u]: value=%x\n", vsb.MixerRegIndex, value));
    vsb.MixerRegs[vsb.MixerRegIndex] = value;
    if ( vsb.MixerRegIndex == SB_MIXERREG_RESET )
        VSB_MixerReset();
    /* INT and DMA setup are readonly */
    if ( vsb.MixerRegIndex == SB_MIXERREG_INT_SETUP || vsb.MixerRegIndex == SB_MIXERREG_DMA_SETUP )
        return;
#if SB16
    if( vsb.DSPVER >= 0x0400 ) { //SB16
        if( vsb.MixerRegIndex >= SB16_MIXERREG_MASTERL && vsb.MixerRegIndex <= SB16_MIXERREG_MIDIR ) {
            //5bits, drop lowest bit
            value = value >> 4;
            switch(vsb.MixerRegIndex) {
            case SB16_MIXERREG_MASTERL:
                vsb.MixerRegs[SB_MIXERREG_MASTERSTEREO] &= 0x0F;
                vsb.MixerRegs[SB_MIXERREG_MASTERSTEREO] |= (value << 4);
                break;
            case SB16_MIXERREG_MASTERR:
                vsb.MixerRegs[SB_MIXERREG_MASTERSTEREO] &= 0xF0;
                vsb.MixerRegs[SB_MIXERREG_MASTERSTEREO] |= value;
                break;
            case SB16_MIXERREG_VOICEL:
                vsb.MixerRegs[SB_MIXERREG_VOICESTEREO]  &= 0x0F;
                vsb.MixerRegs[SB_MIXERREG_VOICESTEREO]  |= (value << 4);
                break;
            case SB16_MIXERREG_VOICER:
                vsb.MixerRegs[SB_MIXERREG_VOICESTEREO]  &= 0xF0;
                vsb.MixerRegs[SB_MIXERREG_VOICESTEREO]  |= value;
                break;
            case SB16_MIXERREG_MIDIL:
                vsb.MixerRegs[SB_MIXERREG_MIDISTEREO]   &= 0x0F;
                vsb.MixerRegs[SB_MIXERREG_MIDISTEREO]   |= (value << 4);
                break;
            case SB16_MIXERREG_MIDIR:
                vsb.MixerRegs[SB_MIXERREG_MIDISTEREO]   &= 0xF0;
                vsb.MixerRegs[SB_MIXERREG_MIDISTEREO]   |= value;
                break;
            }
        } else {
            /* map registers:
             * SB16: auto update MASTERL/MASTERR if MASTERSTEREO is set
             * SB16: auto update VOICEL/VOICER   if VOICESTEREO is set
             * SB16: auto update MIDIL/MIDIR     if MIDISTEREO is set
             */
            switch ( vsb.MixerRegIndex ) {
            case SB_MIXERREG_MASTERSTEREO: /* 22 - SB Pro+ */
                vsb.MixerRegs[SB16_MIXERREG_MASTERR] = ((value & 0xF) << 4) | 8;
                vsb.MixerRegs[SB16_MIXERREG_MASTERL] = (value & 0xF0) | 8;
                break;
            case SB_MIXERREG_VOICESTEREO: /* 04 - SB Pro+ */
                vsb.MixerRegs[SB16_MIXERREG_VOICER] = ((value & 0xF) << 4) | 8;
                vsb.MixerRegs[SB16_MIXERREG_VOICEL] = (value & 0xF0) | 8;
                break;
            case SB_MIXERREG_MIDISTEREO: /* 26 - SB Pro+ */
                vsb.MixerRegs[SB16_MIXERREG_MIDIR] = ((value & 0xF) << 4) | 8;
                vsb.MixerRegs[SB16_MIXERREG_MIDIL] = (value & 0xF0) | 8;
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
    dbgprintf(("VSB_Mixer_Read(%u): %x\n", vsb.MixerRegIndex, vsb.MixerRegs[vsb.MixerRegIndex]));
    return vsb.MixerRegs[vsb.MixerRegIndex];
}

static void DSP_AddData( uint8_t data )
///////////////////////////////////////
{
    vsb.DataBytes %= sizeof vsb.DataBuffer;
    vsb.DataBuffer[vsb.DataBytes] = data;
    vsb.DataBytes++;
}

#if DISPSTAT
static void VSB_DispStatus( void )
//////////////////////////////////
{
    vsb.bDispStat = true;
	printf("VSB_Samples/Pos/Bits: %u/0x%X/%u\n", vsb.Samples, vsb.Position, vsb.Bits );
	printf("VSB_Started/Auto/Silent/Signed: %u/%u/%u/%u\n", vsb.Started, vsb.Auto, vsb.Silent, vsb.Signed );
}
int VSB_GetDispStat( void )
///////////////////////////
{
    int rc = vsb.bDispStat;
    vsb.bDispStat = false;
    return( rc );
};
#endif

/* write port 2x6 */

static void DSP_Reset( uint8_t value )
//////////////////////////////////////
{
    dbgprintf(("DSP_Reset: %u\n",value));
    if(value == 1) {
        vsb.ResetState = VSB_RESET_START;
        /* v1.5: bits 4-7 are rsvd, set to 1? DosBox sets to 0 - check a real SB16! */
        vsb.MixerRegs[SB_MIXERREG_INT_SETUP] = 0xF0 | (1 << FindItem(VSB_IRQMap, countof(VSB_IRQMap), vsb.Irq));
        vsb.MixerRegs[SB_MIXERREG_IRQ_STATUS] &= ~0x7;

        //vsb.MixerRegs[SB_MIXERREG_DMA_SETUP] = (1 << vsb.Dma8) & 0xEB;
#if SB16
        vsb.MixerRegs[SB_MIXERREG_DMA_SETUP] = ( (1 << vsb.Dma8) | ( vsb.Dma16 ? (1 << vsb.Dma16) : 0)) & 0xEB;
#else
        vsb.MixerRegs[SB_MIXERREG_DMA_SETUP] = (1 << vsb.Dma8) & 0xB;
#endif
        vsb.MixerRegs[SB_MIXERREG_MODEFILTER] = 0x11; //SB Pro: mono, no filter
        vsb.MixerRegIndex = 0;
        vsb.dsp_cmd = SB_DSP_NOCMD;
        vsb.DataBytes = 0;
        VSB_Stop();
        vsb.SampleRate = 22050;
        vsb.Samples = 0;
        vsb.Auto = false;
        vsb.Signed = false;
        vsb.Silent = false;
        vsb.Bits = 8;
        vsb.DMAID_A = 0xAA;
        vsb.DMAID_X = 0x96;
        vsb.DirectIdx = 0;
        vsb.WS = 0x80;
        vsb.RS = 0x7F;
#if LATERATE
        vsb.bTimeConst = 0xD2; /* = 22050 */
#endif
        VSB_Mixer_WriteAddr( SB_MIXERREG_RESET );
        VSB_Mixer_Write( 1 );
#if REINITOPL
        MAIN_ReinitOPL();
#endif
    } else if ( vsb.ResetState == VSB_RESET_START ) {
        switch (value) {
        case 0:
            DSP_AddData( 0xAA );
            vsb.ResetState = VSB_RESET_END;
            break;
        case 0x55:  /* uninstall */
            MAIN_Uninstall();
            break;
#if DISPSTAT
        case 0x56:
            VSB_DispStatus();
            break;
#endif
        }
    }
}

int VSB_CalcSampleRate( uint8_t value )
///////////////////////////////////////
{
    int rc = 0;
    int i;
#if 0
    for( i = 0; i < 3; ++i ) {
        if(value >= VSB_TimeConstantMapMono[i][0] - 3 && value <= VSB_TimeConstantMapMono[i][0] + 3 ) {
            rc  = VSB_TimeConstantMapMono[i][1] / VSB_GetChannels();
            break;
        }
    }
#else
    uint8_t limit = 212; //23K Hz. limit time constant. reference: sblaster.cpp from DOSBox-X.
    if( vsb.DSPVER >= 0x0400 )
        limit = vsb.Bits == 2 ? 165 : (vsb.Bits == 3 ? 179 : (vsb.Bits == 4 ? 172 : 234));
    else if(vsb.DSPVER >= 0x0200 && vsb.DSPVER < 0x300)
        limit = vsb.Bits == 2 ? 189 : (vsb.Bits <= 4 ? 172 : ((vsb.HighSpeed ? 234 : 210)));
    else
        limit = vsb.Bits == 2 ? 165 : (vsb.Bits == 3 ? 179 : (vsb.Bits == 4 ? 172 : (vsb.HighSpeed ? 234 : 212)));
#endif
    value = min(value, limit);
    //rc = 1000000 / ( 256 - value ) / VSB_GetChannels();
    rc = 256000000/( 65536 - (value << 8) ) / VSB_GetChannels();
    return rc;
}

/* write port 2xC */

static void DSP_DoCommand( void );

static void DSP_Write( uint8_t value )
//////////////////////////////////////
{
    vsb.WS = 0x80;
    if ( vsb.dsp_cmd == SB_DSP_NOCMD ) {
        if( vsb.HighSpeed ) {//highspeed won't accept further commands, need reset
            dbgprintf(("DSP_Write: cmd %X ignored, HighSpeed active\n", value ));
            return;
        }
        vsb.dsp_cmd = value;
#if SB16
        if (vsb.DSPVER >= 0x400)
            vsb.dsp_cmd_len = DSP_cmd_len_sb16[value];
        else
#endif
            vsb.dsp_cmd_len = DSP_cmd_len_sb[value];
        vsb.dsp_in_pos = 0;
        if (!vsb.dsp_cmd_len)
            DSP_DoCommand();
    } else {
        vsb.dsp_in_data[vsb.dsp_in_pos] = value;
        vsb.dsp_in_pos++;
        if ( vsb.dsp_in_pos >= vsb.dsp_cmd_len )
            DSP_DoCommand();
    }
}

static void DSP_DoCommand( void )
/////////////////////////////////
{
    switch ( vsb.dsp_cmd ) {
    case SB_DSP_SPEAKER_ON: /* D1 */
        vsb.bSpeaker = true;
        break;
    case SB_DSP_SPEAKER_OFF: /* D3 */
        vsb.bSpeaker = false;
        break;
    case SB_DSP_SPEAKER_STATUS: /* D8 */
        DSP_AddData( vsb.bSpeaker ? 0xff : 0 );
        dbgprintf(("DSP_DoCommand: cmd %X, databytes=%u\n", vsb.dsp_cmd, vsb.DataBytes ));
        break;
    case SB_DSP_HALT_DMA16: SB16_ONLY(); /* D5 */
    case SB_DSP_HALT_DMA: /* D0 */
        vsb.Started = false;
        dbgprintf(("DSP_DoCommand: cmd %X, stopped\n", vsb.dsp_cmd ));
        break;
    case SB_DSP_CONTINUE_DMA16: SB16_ONLY(); /* D6 */
    case SB_DSP_CONTINUE_DMA: /* D4 */
        vsb.Started = true;
        dbgprintf(("DSP_DoCommand: cmd %X, continued\n", vsb.dsp_cmd ));
        break;
    case SB_DSP_CONT_16BIT_AUTO: /* 47 - SB16 only */
    case SB_DSP_CONT_8BIT_AUTO: SB16_ONLY(); /* 45 - SB16 only */
        vsb.Auto = true;
        dbgprintf(("DSP_DoCommand: cmd %X, continue autoinit\n", vsb.dsp_cmd ));
        break;
    case SB_DSP_EXIT_16BIT_AUTO: SB16_ONLY(); /* D9 */
    case SB_DSP_EXIT_8BIT_AUTO:  /* DA */
        //if( vsb.Auto ) {
            vsb.Auto = false;
        //  vsb.Started = false;
        //}
        break;
    case SB_DSP_8BIT_OUT_1: /* 14 */
    case 0x15: /* 15 */
        vsb.MixerRegs[SB_MIXERREG_IRQ_STATUS] &= ~0x7;
        vsb.Samples = vsb.dsp_in_data[0] | ( vsb.dsp_in_data[1] << 8 ); /* actually it's length (=samples-1) */
        vsb.Bits = 8;
        vsb.Signed = false;
        vsb.Silent = false;
        vsb.Started = true;
        vsb.Position = 0;
        dbgprintf(("DSP_DoCommand: cmd %X, length=%u started=%u\n", vsb.dsp_cmd, vsb.Samples, vsb.Started ));
        break;
    case SB_DSP_8BIT_OUT_1_HS: /* 91 */
    case SB_DSP_8BIT_OUT_AUTO_HS: /* 90 */
    case SB_DSP_8BIT_OUT_AUTO: /* 1C */
        vsb.MixerRegs[SB_MIXERREG_IRQ_STATUS] &= ~0x7;
        vsb.Auto = ( vsb.dsp_cmd == SB_DSP_8BIT_OUT_AUTO || vsb.dsp_cmd == SB_DSP_8BIT_OUT_AUTO_HS );
        vsb.Bits = 8;
        vsb.HighSpeed = ( vsb.dsp_cmd == SB_DSP_8BIT_OUT_1_HS || vsb.dsp_cmd == SB_DSP_8BIT_OUT_AUTO_HS );
        vsb.Signed = false;
        vsb.Silent = false;
        vsb.Started = true; //start transfer
        vsb.Position = 0;
        dbgprintf(("DSP_DoCommand: cmd %X, started=%u\n", vsb.dsp_cmd, vsb.Started ));
        break;
    case SB_DSP_2BIT_OUT_AUTO: /* 1F; AUTO: bit 3=1 */
    case SB_DSP_3BIT_OUT_AUTO: /* 7F */
    case SB_DSP_4BIT_OUT_AUTO: /* 7D */
        vsb.MixerRegs[SB_MIXERREG_IRQ_STATUS] &= ~0x7;
        vsb.Auto = true;
        ISR_adpcm_state.useRef = true;
        ISR_adpcm_state.step = 0;
        vsb.Bits = (vsb.dsp_cmd == SB_DSP_2BIT_OUT_AUTO) ? 2 : (vsb.dsp_cmd == SB_DSP_3BIT_OUT_AUTO) ? 3 : 4;
        vsb.MixerRegs[SB_MIXERREG_MODEFILTER] &= ~SB_MIXERREG_MODEFILTER_STEREO;
        vsb.Silent = false;
        vsb.Started = true; //start transfer here
        vsb.Position = 0;
        dbgprintf(("DSP_DoCommand: cmd %X, started=%u\n", vsb.dsp_cmd, vsb.Started ));
        break;
    case 0xb0:  case 0xb1:  case 0xb2:  case 0xb3:  case 0xb4:  case 0xb5:  case 0xb6:  case 0xb7:
    case 0xb8:  case 0xb9:  case 0xba:  case 0xbb:  case 0xbc:  case 0xbd:  case 0xbe:  case 0xbf:
    case 0xc0:  case 0xc1:  case 0xc2:  case 0xc3:  case 0xc4:  case 0xc5:  case 0xc6:  case 0xc7:
    case 0xc8:  case 0xc9:  case 0xca:  case 0xcb:  case 0xcc:  case 0xcd:  case 0xce:  case 0xcf:
        SB16_ONLY();
        vsb.MixerRegs[SB_MIXERREG_IRQ_STATUS] &= ~0x7;
        /* bit1=0: nofifo
        /* bit2=1: auto (B4, C4, B6, C6)
         */
        vsb.Auto = ( ( vsb.dsp_cmd & 0x4 ) ? true : false );
        vsb.Bits = ( ( vsb.dsp_cmd & 0x40 ) ? 8 : 16 );
        /* bit 4 of value: 1=signed */
        vsb.Signed = ( vsb.dsp_in_data[0] & 0x10 ) ? true : false;
        /* bit 5 of value: 1=stereo */
        if ( vsb.dsp_in_data[0] & 0x20 )
            vsb.MixerRegs[SB_MIXERREG_MODEFILTER] |= SB_MIXERREG_MODEFILTER_STEREO;
        else
            vsb.MixerRegs[SB_MIXERREG_MODEFILTER] &= ~SB_MIXERREG_MODEFILTER_STEREO;

        //vsb.Samples = ( vsb.dsp_in_data[1] | ( vsb.dsp_in_data[2] << 8 ) ) - 1;
        vsb.Samples = vsb.dsp_in_data[1] | ( vsb.dsp_in_data[2] << 8 );
        vsb.Silent = false;
        vsb.Started = true;
        vsb.Position = 0;
        dbgprintf(("DSP_DoCommand: cmd %X, started=%u\n", vsb.dsp_cmd, vsb.Started ));
        break;
    case SB_DSP_SET_TIMECONST: /* 40 */
#if !LATERATE
        vsb.SampleRate = VSB_CalcSampleRate( vsb.dsp_in_data[0] );
        dbgprintf(("DSP_DoCommand: time constant=%X, rate=%u\n", vsb.dsp_in_data[0], vsb.SampleRate ));
#else
        vsb.SampleRate = 0;
        vsb.bTimeConst = vsb.dsp_in_data[0];
        dbgprintf(("DSP_DoCommand: time constant=%X\n", vsb.bTimeConst ));
#endif
        break;
    case SB_DSP_SET_SAMPLERATE: /* 41 - set output sample rate; SB16 only */
    case SB_DSP_SET_SAMPLERATE_I: SB16_ONLY(); /* 42 - set input sample rate; SB16 only */
        vsb.SampleRate = ( vsb.dsp_in_data[0] << 8 ) | vsb.dsp_in_data[1]; /* hibyte first */
        break;
    case SB_DSP_8BIT_DIRECT: /* 10 */
        vsb.DirectBuffer[vsb.DirectIdx++] = vsb.dsp_in_data[0];
        vsb.DirectIdx %= VSB_DIRECTBUFFER_SIZE;
        dbgprintf(("DSP_DoCommand: 8Bit Direct, data=%X\n", vsb.dsp_in_data[0] ));
        break;
    case SB_DSP_SET_SIZE: /* 48 - set DMA block size - used for auto cmds */
        vsb.Samples = vsb.dsp_in_data[0] | ( vsb.dsp_in_data[1] << 8 );
        break;
    case SB_DSP_2BIT_OUT_1_NREF: /* 16 */
    case SB_DSP_2BIT_OUT_1:      /* 17 */
    case SB_DSP_4BIT_OUT_1_NREF: /* 74 */
    case SB_DSP_4BIT_OUT_1:      /* 75 */
    case SB_DSP_3BIT_OUT_1_NREF: /* 76 */
    case SB_DSP_3BIT_OUT_1:      /* 77 */
        vsb.MixerRegs[SB_MIXERREG_IRQ_STATUS] &= ~0x7;
        vsb.Samples = vsb.dsp_in_data[0] | ( vsb.dsp_in_data[1] << 8 ); /* the value is #samples-1! */
        vsb.Auto = false;
        /* useref is bit 0 */
        ISR_adpcm_state.useRef = (vsb.dsp_cmd & 1 );
        ISR_adpcm_state.step = 0;
        vsb.Bits = (vsb.dsp_cmd <= SB_DSP_2BIT_OUT_1) ? 2 : (vsb.dsp_cmd >= SB_DSP_3BIT_OUT_1_NREF) ? 3 : 4;
        vsb.MixerRegs[SB_MIXERREG_MODEFILTER] &= ~SB_MIXERREG_MODEFILTER_STEREO; /* reset stereo */
        vsb.Silent = false;
        vsb.Signed = false;
        vsb.Started = true;
        vsb.Position = 0;
        dbgprintf(("DSP_DoCommand: cmd %X, started=%u\n", vsb.dsp_cmd, vsb.Started ));
        break;
    case SB_DSP_SILENCE_DAC: /* 80 - output silence samples */
        vsb.MixerRegs[SB_MIXERREG_IRQ_STATUS] &= ~0x7;
        vsb.Samples = vsb.dsp_in_data[0] | ( vsb.dsp_in_data[1] << 8 ); /* the value is #samples-1! */
        vsb.MixerRegs[SB_MIXERREG_MODEFILTER] &= ~SB_MIXERREG_MODEFILTER_STEREO; /* reset stereo */
        vsb.Signed = false;
        vsb.Bits = 8;
        vsb.Silent = true;
        vsb.Started = true;
        vsb.Position = 0;
        dbgprintf(("DSP_DoCommand: cmd %X, started=%u\n", vsb.dsp_cmd, vsb.Started ));
        break;
    case 0x0E: /* SB16 "ASP set register" - used by diagnose.exe, expect 2 bytes */
        SB16_ONLY();
        dbgprintf(("DSP_DoCommand: cmd=%X, data[0]=%X data[1]=%X\n", vsb.dsp_cmd, vsb.dsp_in_data[0], vsb.dsp_in_data[1] ));
        /* data[0] is index for "ASP register table", data[1] is value of that register; */
#if 0 /* what data is expected to be returned here? */
        DSP_AddData(0);
        dbgprintf(("DSP_DoCommand: cmd %X, databytes=%u\n", vsb.dsp_cmd, vsb.DataBytes ));
#endif
        break;
    case 0x0F: SB16_ONLY(); /* SB16 "ASP get register" - used by diagnose.exe, expect 1 byte */
        dbgprintf(("DSP_DoCommand: cmd=%X, byte[0]=%X\n", vsb.dsp_cmd, vsb.dsp_in_data[0] ));
        /* data[0] is index of "ASP register" to read */
        DSP_AddData(0); /* should return ASP register value */
        dbgprintf(("DSP_DoCommand: cmd %X, databytes=%u\n", vsb.dsp_cmd, vsb.DataBytes ));
        break;
#if VMPU
    case 0x38: /* write SB MIDI data */
        VMPU_SBMidi_RawWrite( vsb.dsp_in_data[0] );
        break;
#endif
    case SB_DSP_ID: /* E0: supposed to return bitwise NOT of data byte */
        DSP_AddData( vsb.dsp_in_data[0] ^ 0xFF );
        dbgprintf(("DSP_DoCommand: cmd %X, databytes=%u\n", vsb.dsp_cmd, vsb.DataBytes ));
        break;
    case SB_DSP_GETVER: /* E1 */
        DSP_AddData( vsb.DSPVER >> 8 );
        DSP_AddData( vsb.DSPVER & 0xFF );
        dbgprintf(("DSP_DoCommand: cmd %X, databytes=%u\n", vsb.dsp_cmd, vsb.DataBytes ));
        break;
    case SB_DSP_DMA_ID: /* E2 */
        vsb.DMAID_A += vsb.dsp_in_data[0] ^ vsb.DMAID_X;
        vsb.DMAID_X = (vsb.DMAID_X >> 2u) | (vsb.DMAID_X << 6u);
        dbgprintf(("DSP_DoCommand: cmd %X\n", vsb.dsp_cmd ));
        VDMA_WriteData( vsb.Dma8, vsb.DMAID_A ); /* write to low dma channel */
        break;
    case SB_DSP_COPYRIGHT: /* E3 */
        strcpy( vsb.DataBuffer, SB_Copyright );
        vsb.DataBytes = sizeof( SB_Copyright );
        dbgprintf(("DSP_DoCommand: cmd %X, databytes=%u\n", vsb.dsp_cmd, vsb.DataBytes ));
        break;
    case SB_DSP_WRITE_TESTREG: /* E4 */
        vsb.TestReg = vsb.dsp_in_data[0];
        break;
    case SB_DSP_READ_TESTREG: /* E8 */
        DSP_AddData( vsb.TestReg );
        dbgprintf(("DSP_DoCommand: cmd %X, databytes=%u\n", vsb.dsp_cmd, vsb.DataBytes ));
        break;
    case SB_DSP_TRIGGER_IRQ: /* F2 */
        VSB_SetIRQStatus( SB_MIXERREG_IRQ_STAT8BIT );
        break;
    case SB_DSP_TRIGGER_IRQ16: /* F3 */
        VSB_SetIRQStatus( SB_MIXERREG_IRQ_STAT16BIT );
        break;
    case SB_DSP_STATUS: /* FB */
        DSP_AddData( vsb.Started ? (( vsb.Bits <= 8 ? 1 : 4 ) ) : 0 );
        dbgprintf(("DSP_DoCommand: cmd %X, databytes=%u\n", vsb.dsp_cmd, vsb.DataBytes ));
        break;
    case SB_DSP_DSP_AUX_STATUS: /* FC */
        /* aux status:
         b1: DAC/ADC DMA
         b2: Auto 8-bit
         b4: Auto 16-bit
         */
        DSP_AddData( (vsb.Auto << 4) | (vsb.Auto << 2) | (vsb.Started << 1) );
        dbgprintf(("DSP_DoCommand: cmd %X, databytes=%u\n", vsb.dsp_cmd, vsb.DataBytes ));
        break;
    case 0x05: /* ASP cmd */
        dbgprintf(("DSP_DoCommand: ASP cmd %X\n", vsb.dsp_cmd ));
        break;
    default:
        dbgprintf(("DSP_DoCommand: unhandled cmd %X\n", vsb.dsp_cmd ));
        break;
    }

    vsb.dsp_cmd = SB_DSP_NOCMD;

}

/* read port 02xA */

static uint8_t DSP_Read( void )
///////////////////////////////
{
    if ( vsb.DataBytes ) {
        uint8_t rc;
        rc = vsb.DataBuffer[0];
        vsb.DataBytes--;
        if (vsb.DataBytes) memcpy( vsb.DataBuffer, &vsb.DataBuffer[1], vsb.DataBytes );
        vsb.RS &= 0x7F;
        return( rc );
    } else  {
        dbgprintf(("DSP_Read: read buffer empty, returning %X\n", vsb.DataBuffer[0] ));
        return vsb.DataBuffer[0];
    }
    dbgprintf(("DSP_Read: read buffer empty (FFh)\n" ));
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
    vsb.WS += 0x80; //some games will wait on busy first
    return vsb.WS;
}

/* read status register 02xE */

static uint8_t DSP_ReadStatus( void )
/////////////////////////////////////
{
    vsb.MixerRegs[SB_MIXERREG_IRQ_STATUS] &= ~SB_MIXERREG_IRQ_STAT8BIT;

    if ( vsb.DataBytes )
        vsb.RS |= 0x80;

    //dbgprintf(("DSP_ReadStatus=%X\n", vsb.RS ));
    return vsb.RS;
}

/* read port 02xF */

static uint8_t DSP_INT16ACK( void )
///////////////////////////////////
{
    //dbgprintf(("DSP_INT16ACK\n"));
    vsb.MixerRegs[SB_MIXERREG_IRQ_STATUS] &= ~SB_MIXERREG_IRQ_STAT16BIT;
    return 0xFF;
}

void VSB_Init(int irq, int dma, int hdma, int type )
////////////////////////////////////////////////////
{
    vsb.Irq = irq;
    vsb.Dma8 = dma;
    vsb.Dma16 = hdma;
    vsb.DSPVER = VSB_DSPVersion[type];
    if ( vsb.DSPVER >= 0x400 ) {
        switch ( vsb.DSPVER & 0xFF ) {
        case 0x5: vsb.MixerRegs[SB_MIXERREG_IRQ_STATUS] |= 0x20; break;
        case 0x12: vsb.MixerRegs[SB_MIXERREG_IRQ_STATUS] |= 0x80; break;
        default: vsb.MixerRegs[SB_MIXERREG_IRQ_STATUS] |= 0x10;
        }
    }
    VSB_Mixer_WriteAddr( SB_MIXERREG_RESET );
    VSB_Mixer_Write( 1 );
}

uint8_t VSB_GetIRQ()
////////////////////
{
    if( !( vsb.MixerRegs[SB_MIXERREG_INT_SETUP] & 0xF ) )
        return 0xFF;
    return VSB_IRQMap[BSF(vsb.MixerRegs[SB_MIXERREG_INT_SETUP])];
}

/* get current DMA channel */

int VSB_GetDMA()
////////////////
{
#if SB16
    if ( vsb.Bits > 8 ) {
        if( vsb.MixerRegs[SB_MIXERREG_DMA_SETUP] & 0xF0 )
            return( BSF(vsb.MixerRegs[SB_MIXERREG_DMA_SETUP] & 0xF0 ) );
    }
#endif
    if( vsb.MixerRegs[SB_MIXERREG_DMA_SETUP] )
        return( BSF(vsb.MixerRegs[SB_MIXERREG_DMA_SETUP] ) );
    return -1;
}

#if 0 //SB16
int VSB_GetDma16()
//////////////////
{
    int bit;
    if( !(vsb.MixerRegs[SB_MIXERREG_DMA_SETUP] & 0xF0 ))
        return -1;
    bit = BSF(vsb.MixerRegs[SB_MIXERREG_DMA_SETUP] >> 4) + 4;
    return bit;
}
#endif

int VSB_Running()
/////////////////
{
    return vsb.Started;
}

int VSB_IsSilent()
/////////////////
{
    return vsb.Silent;
}

void VSB_Stop()
///////////////
{
    vsb.Started = false;
    vsb.HighSpeed = false;
    vsb.Position = 0;
}

#if 0
int VSB_GetDACSpeaker()
///////////////////////
{
    return vsb.DACSpeaker;
}
#endif

unsigned int VSB_GetBits()
//////////////////////////
{
    return vsb.Bits;
}

int VSB_IsSigned()
//////////////////
{
    return vsb.Signed;
}

int VSB_GetChannels()
/////////////////////
{
    /* MIXERREG_MODEFILTER is for SB Pro only, but in vsbhda also set for sb16 */
    return (vsb.MixerRegs[SB_MIXERREG_MODEFILTER] & SB_MIXERREG_MODEFILTER_STEREO) ? 2 : 1;
}

int VSB_GetSampleRate()
///////////////////////
{
#if LATERATE
    if ( !vsb.SampleRate )
        vsb.SampleRate = VSB_CalcSampleRate( vsb.bTimeConst );
#endif
    return vsb.SampleRate;
}

/* returns size of sample buffer in bytes */

uint32_t VSB_GetSampleBufferSize()
//////////////////////////////////
{
    //return vsb.Samples + 1;
    //return(( vsb.Samples + 1 ) * vsb.Bits / 8 );
    //return((vsb.Samples + 1) * max(1, vsb.Bits >> 3));
    //if ( !vsb.Samples ) asm("int3"); /* 1 sample, used by card detection software */
    return((vsb.Samples + 1) * ((vsb.Bits+7) >> 3));
}

int VSB_GetAuto()
/////////////////
{
    return vsb.Auto;
}

uint32_t VSB_GetPos()
/////////////////////
{
    return vsb.Position;
}

/* set pos (and IRQ status if pos beyond sample buffer) */

uint32_t VSB_SetPos(uint32_t pos)
/////////////////////////////////
{
    /* new pos above size of sample buffer? */
    if( pos >= VSB_GetSampleBufferSize() )
        VSB_SetIRQStatus( (VSB_GetBits() <= 8 ) ? SB_MIXERREG_IRQ_STAT8BIT : SB_MIXERREG_IRQ_STAT16BIT );
    return vsb.Position = pos;
}

void VSB_SetIRQStatus( uint8_t flag )
/////////////////////////////////////
{
    vsb.MixerRegs[SB_MIXERREG_IRQ_STATUS] |= flag;
}

/* checks if a SB IRQ is supposed to happen.
 * Just the SB status flags are checked.
 */

int VSB_GetIRQStatus( void )
////////////////////////////
{
    /* Probably should also check the VPIC mask flags? */
    //if ( VPIC_Acc( 0x21, 0, 0 ) & (1 << vsb.Irq ) )
    //    return 0;
    return( vsb.MixerRegs[SB_MIXERREG_IRQ_STATUS] & ( SB_MIXERREG_IRQ_STAT8BIT | SB_MIXERREG_IRQ_STAT16BIT ) );
}

int VSB_GetDirectCount( uint8_t * *pBuffer )
////////////////////////////////////////////
{
    *pBuffer = vsb.DirectBuffer;
    return vsb.DirectIdx;
}
void VSB_ResetDirectCount( void )
/////////////////////////////////
{
    vsb.DirectIdx = 0;
    return;
}

uint8_t VSB_GetMixerReg(uint8_t index)
//////////////////////////////////////
{
    return vsb.MixerRegs[index];
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
