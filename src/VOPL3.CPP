
#include <stdint.h>
#include <stdbool.h>

#include "CONFIG.H"
#include "PLATFORM.H"
#include "PTRAP.H"
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
uint32_t ADLG_Volume[2];  /* AdLib Gold reg 9/10; todo: add ref to docs */
uint16_t IndexReg[2];
uint8_t ADLG_CtrlEnable;  //seems not working for Miles Sound, don't use it
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
# if NOSILENTGEN
    if ( opl3.bWritten ) {
        opl3.Chip->Setup(samplerate);
        opl3.bWritten = false;
    }
# else
    opl3.Chip->Setup(samplerate);
# endif
}
#endif

int VOPL3_GetMode()
///////////////////
{
    return opl3.Chip->opl3Active;
}

/* generate samples; called during interrupt time;
 * according to docs there's no FPU usage in Chip::Generate(),
 * so it's likely that there's no need to save/restore fpu state.
 */

int VOPL3_GenSamples(int16_t* pcm16, int count)
///////////////////////////////////////////////
{
    int rc;
    char buffer[FPU_SRSIZE];
    fpu_save(buffer);
    rc = opl3.Chip->Generate(pcm16, count);
    fpu_restore(buffer);
    return rc;
}

static uint8_t VOPL3_PrimaryRead( void )
////////////////////////////////////////
{
    uint8_t val = 0;
    /* some programs may distinguish SB Pro 1 from SB Pro 2 by value read from port 0x388:
     * 0x06 -> Pro1, 0x00 -> Pro 2.
     */
    if ( ( opl3.TimerCtrlReg[0] & (VOPL3_TIMER1_MASK | VOPL3_TIMER1_START ) ) == VOPL3_TIMER1_START )
        val |= VOPL3_TIMER1_TIMEOUT;
    if ( ( opl3.TimerCtrlReg[1] & (VOPL3_TIMER2_MASK | VOPL3_TIMER2_START ) ) == VOPL3_TIMER2_START )
        val |= VOPL3_TIMER2_TIMEOUT;
    return val;
}

static void VOPL3_PrimaryWriteIndex(uint8_t val)
////////////////////////////////////////////////
{
    opl3.IndexReg[VOPL3_PRIMARY] = (uint16_t)opl3.Chip->WriteAddr(0x388, val); /* returns val or 0 */
    return;
}

static void VOPL3_PrimaryWriteData(uint8_t val)
///////////////////////////////////////////////
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
    return;
}

static uint8_t VOPL3_SecondaryRead( void )
//////////////////////////////////////////
{
    /* v1.7: fixed access to opl3.ADLG_Volume[] */
    if( opl3.IndexReg[VOPL3_SECONDARY] == ( 0x100 + VOPL3_ADLG_VOLL_REG_INDEX ) )
        return opl3.ADLG_Volume[0];
    else if( opl3.IndexReg[VOPL3_SECONDARY] == ( 0x100 + VOPL3_ADLG_VOLR_REG_INDEX ) )
        return opl3.ADLG_Volume[1];

    return VOPL3_PrimaryRead();
}

static void VOPL3_SecondaryWriteIndex(uint8_t val)
//////////////////////////////////////////////////
{
    if(val == 0xFF)
        opl3.ADLG_CtrlEnable = 1;
    else if(val == 0xFE)
        opl3.ADLG_CtrlEnable = 0;

    opl3.IndexReg[VOPL3_SECONDARY] = (uint16_t)opl3.Chip->WriteAddr(0x38A, val);  /* returns val, val + 0x100 or 0 */
    return;
}

static void VOPL3_SecondaryWriteData(uint8_t val)
/////////////////////////////////////////////////
{
    /* v1.7: fixed access to opl3.ADLG_Volume[] */
    if( opl3.IndexReg[VOPL3_SECONDARY] == ( 0x100 + VOPL3_ADLG_VOLL_REG_INDEX ) )
        opl3.ADLG_Volume[0] = val;
    else if( opl3.IndexReg[VOPL3_SECONDARY] == (0x100 + VOPL3_ADLG_VOLR_REG_INDEX ) )
        opl3.ADLG_Volume[1] = val;

    opl3.Chip->WriteReg(opl3.IndexReg[VOPL3_SECONDARY], val);
    dbgprintf(("VOPL3_SecondaryWriteData(%X)=%X\n", opl3.IndexReg[VOPL3_SECONDARY], val ));

#if NOSILENTGEN
	opl3.bWritten = true;
#endif
    return;
}

uint8_t VOPL3_388(uint16_t port, uint8_t val, uint16_t flags)
/////////////////////////////////////////////////////////////
{
    return (flags & TRAPF_OUT) ? VOPL3_PrimaryWriteIndex(val), val : VOPL3_PrimaryRead();
}

uint8_t VOPL3_389(uint16_t port, uint8_t val, uint16_t flags)
/////////////////////////////////////////////////////////////
{
    return (flags & TRAPF_OUT) ? VOPL3_PrimaryWriteData(val), val : VOPL3_PrimaryRead();
}

uint8_t VOPL3_38A(uint16_t port, uint8_t val, uint16_t flags)
/////////////////////////////////////////////////////////////
{
    return (flags & TRAPF_OUT) ? VOPL3_SecondaryWriteIndex(val), val : VOPL3_SecondaryRead();
}
uint8_t VOPL3_38B(uint16_t port, uint8_t val, uint16_t flags)
/////////////////////////////////////////////////////////////
{
    return (flags & TRAPF_OUT) ? VOPL3_SecondaryWriteData(val), val : VOPL3_SecondaryRead();
}
#if NOSILENTGEN
bool VOPL3_IsActive( void )
///////////////////////////
{
    return opl3.bWritten;
}
#endif
