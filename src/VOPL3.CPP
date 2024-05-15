
#include <stdbool.h>

#include "CONFIG.H"
#include "PLATFORM.H"
#include "DBOPL.H"
#include "VOPL3.H"

#define VOPL3_PRIMARY 0
#define VOPL3_SECONDARY 1
//primary index read
#define VOPL3_TIMER_REG_INDEX 4
#define VOPL3_TIMER1_MASK 0xC0
#define VOPL3_TIMER2_MASK 0xA0
#define VOPL3_TIMER1_START 0x01
#define VOPL3_TIMER2_START 0x02
#define VOPL3_TIMER1_TIMEOUT VOPL3_TIMER1_MASK
#define VOPL3_TIMER2_TIMEOUT VOPL3_TIMER2_MASK

#define NOSILENTGEN 1

//secondary index read (Adlib Gold). reference: AIL2.0 source code, dosbox
#define VOPL3_ADLG_IOBUSY 0x40UL
#define VOPL3_ADLG_VOLL_REG_INDEX 9 //left volume
#define VOPL3_ADLG_VOLR_REG_INDEX 10 //right volume
//data
#define KEY_ON 0x10 //channel on bit

/* 388: R: adlib status port
 *         bits 5-7: status timer 1, timer2, global timer, bits 0-4: rsvd.
 *      W: adlib register port
 *         select register index for port 389 (244 registers)
 * 389: W: adlib data register
 *      02: Timer 1 data
 *      03: Timer 2 data
 *      04: Timer control flags; T1 CTL, T2 CTL, 2-4 unused, T2 MSK, T1 MSK, IRQ RST
 *      08: Speech syntheses mode/Keyboard split note select  
 *     20-35: Amp Mod / Vibrato / EG type / Key Scaling / Multiple
 *     40-55: Key Scaling level / Operator output level
 *     60-75: Attack Rate / Decay Rate
 *     80-95: Sustain level / Release Rate
 *     A0-A8: Frequency ( low 8 bits )
 *     B0-B8: Key on / Octave / Frequency ( high 2 bits )
 *      BD:  AM depth / Vibrato depth, Rhythm control
 *     C0-C8: Feedback strength / Connection type
 *     E0-F5: Wave Select
 */

struct VOpl3_s {
DBOPL::Chip * Chip;
uint32_t TimerCtrlReg[2]; //if start 1 and 2 seperately we will miss one, so use 2 cache
uint32_t IndexReg[2];
uint32_t ADLG_CtrlEnable;    //seems not working for Miles Sound, don't use it
uint32_t ADLG_Volume[2];
#if NOSILENTGEN
bool bWritten;
#endif
};

static struct VOpl3_s opl3;

void VOPL3_Init(int samplerate)
///////////////////////////////
{
	opl3.Chip = new DBOPL::Chip(true); /* true=OPL3 mode */
	opl3.ADLG_Volume[0] = 0x08;
	opl3.ADLG_Volume[1] = 0x08;
    opl3.Chip->Setup(samplerate);
#if NOSILENTGEN
	opl3.bWritten = false;
#endif
}

#if REINITOPL
void VOPL3_Reinit(int samplerate)
/////////////////////////////////
{
	opl3.Chip->Setup(samplerate);
#if NOSILENTGEN
	opl3.bWritten = false;
#endif
}
#endif

int VOPL3_GetMode()
///////////////////
{
    return opl3.Chip->opl3Active;
}

int VOPL3_GenSamples(int16_t* pcm16, int count)
///////////////////////////////////////////////
{
    char buffer[108];
	int rc;
	fpu_save(buffer);
	rc = opl3.Chip->Generate(pcm16, count);
	fpu_restore(buffer);
	return rc;
}

static uint32_t VOPL3_PrimaryRead(uint32_t val)
///////////////////////////////////////////////
{
    val &= ~0xFF;
    if ( ( opl3.TimerCtrlReg[0] & (VOPL3_TIMER1_MASK | VOPL3_TIMER1_START ) ) == VOPL3_TIMER1_START )
        val |= VOPL3_TIMER1_TIMEOUT;
    if ( ( opl3.TimerCtrlReg[1] & (VOPL3_TIMER2_MASK | VOPL3_TIMER2_START ) ) == VOPL3_TIMER2_START )
        val |= VOPL3_TIMER2_TIMEOUT;
    return val;
}

static uint32_t VOPL3_PrimaryWriteIndex(uint32_t val)
/////////////////////////////////////////////////////
{
    opl3.IndexReg[VOPL3_PRIMARY] = opl3.Chip->WriteAddr(0x388, val); /* returns val or 0 */
    return val;
}

static uint32_t VOPL3_PrimaryWriteData(uint32_t val)
////////////////////////////////////////////////////
{
    if ( opl3.IndexReg[VOPL3_PRIMARY] == VOPL3_TIMER_REG_INDEX ) {
        if ( val & (VOPL3_TIMER1_START | VOPL3_TIMER1_MASK ))
            opl3.TimerCtrlReg[0] = val;
        if ( val & (VOPL3_TIMER2_START | VOPL3_TIMER2_MASK ))
            opl3.TimerCtrlReg[1] = val;
    }
    opl3.Chip->WriteReg(opl3.IndexReg[VOPL3_PRIMARY], val);
    dbgprintf(("VOPL3_PrimaryWriteData(%X)=%X\n", opl3.IndexReg[VOPL3_PRIMARY], val ));

#if NOSILENTGEN
	opl3.bWritten = true;
#endif
    return val;
}

static uint32_t VOPL3_SecondaryRead(uint32_t val)
/////////////////////////////////////////////////
{
    if( ( opl3.IndexReg[VOPL3_SECONDARY] == ( 0x100 + VOPL3_ADLG_VOLL_REG_INDEX )) || ( opl3.IndexReg[VOPL3_SECONDARY] == ( 0x100 + VOPL3_ADLG_VOLR_REG_INDEX) ) )
            return opl3.ADLG_Volume[opl3.IndexReg[VOPL3_SECONDARY] - VOPL3_ADLG_VOLL_REG_INDEX];
    return VOPL3_PrimaryRead(val);
}

static uint32_t VOPL3_SecondaryWriteIndex(uint32_t val)
///////////////////////////////////////////////////////
{
    if(val == 0xFF)
        opl3.ADLG_CtrlEnable = 1;
    else if(val == 0xFE)
        opl3.ADLG_CtrlEnable = 0;

    opl3.IndexReg[VOPL3_SECONDARY] = opl3.Chip->WriteAddr(0x38A, val);  /* returns val or 0 */
    return val;
}

static uint32_t VOPL3_SecondaryWriteData(uint32_t val)
//////////////////////////////////////////////////////
{
    //if(/*opl3.ADLG_CtrlEnable && */(opl3.IndexReg[VOPL3_SECONDARY] == 0x100 + VOPL3_ADLG_VOLL_REG_INDEX || opl3.IndexReg[VOPL3_SECONDARY] == 0x100 + VOPL3_ADLG_VOLR_REG_INDEX))
    if( (opl3.IndexReg[VOPL3_SECONDARY] == 0x100 + VOPL3_ADLG_VOLL_REG_INDEX ) || ( opl3.IndexReg[VOPL3_SECONDARY] == 0x100 + VOPL3_ADLG_VOLR_REG_INDEX))
        opl3.ADLG_Volume[opl3.IndexReg[VOPL3_SECONDARY] - VOPL3_ADLG_VOLL_REG_INDEX] = val;
    opl3.Chip->WriteReg(opl3.IndexReg[VOPL3_SECONDARY], val);
    dbgprintf(("VOPL3_SecondaryWriteData(%X)=%X\n", opl3.IndexReg[VOPL3_SECONDARY], val ));

#if NOSILENTGEN
	opl3.bWritten = true;
#endif
    return val;
}

uint32_t VOPL3_388(uint32_t port, uint32_t val, uint32_t out)
/////////////////////////////////////////////////////////////
{
    return out ? VOPL3_PrimaryWriteIndex(val) : VOPL3_PrimaryRead(val);
}

uint32_t VOPL3_389(uint32_t port, uint32_t val, uint32_t out)
/////////////////////////////////////////////////////////////
{
    return out ? VOPL3_PrimaryWriteData(val) : VOPL3_PrimaryRead(val);
}

uint32_t VOPL3_38A(uint32_t port, uint32_t val, uint32_t out)
/////////////////////////////////////////////////////////////
{
    return out ? VOPL3_SecondaryWriteIndex(val) : VOPL3_SecondaryRead(val);
}
uint32_t VOPL3_38B(uint32_t port, uint32_t val, uint32_t out)
/////////////////////////////////////////////////////////////
{
    return out ? VOPL3_SecondaryWriteData(val) : VOPL3_SecondaryRead(val);
}
#if NOSILENTGEN
bool VOPL3_IsActive( void )
///////////////////////////
{
    return opl3.bWritten;
}
#endif
