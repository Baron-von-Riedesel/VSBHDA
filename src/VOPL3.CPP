
#include "SBEMUCFG.H"
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
static uint32_t VOPL3_TimerCtrlReg[2]; //if start 1 and 2 seperately we will miss one, so use 2 cache
static uint32_t VOPL3_IndexReg[2];

//secondary index read (Adlib Gold). reference: AIL2.0 source code, dosbox
#define VOPL3_ADLG_IOBUSY 0x40UL
#define VOPL3_ADLG_VOLL_REG_INDEX 9 //left volume
#define VOPL3_ADLG_VOLR_REG_INDEX 10 //right volume
//data
#define KEY_ON 0x10 //channel on bit
static uint32_t VOPL3_ADLG_CtrlEnable = 0;    //seems not working for Miles Sound, don't use it
static uint32_t VOPL3_ADLG_Volume[2] = {0x08,0x08};

static DBOPL::Chip* VOPL3_Chip;

void VOPL3_Init(int samplerate)
///////////////////////////////
{
    VOPL3_Chip = new DBOPL::Chip(true); /* true=OPL3 mode */
    VOPL3_Chip->Setup(samplerate);
}

#if REINITOPL
void VOPL3_Reinit(int samplerate)
/////////////////////////////////
{
	VOPL3_Chip->Setup(samplerate);
}
#endif

int VOPL3_GetMode()
///////////////////
{
    return VOPL3_Chip->opl3Active;
}

int VOPL3_GenSamples(int16_t* pcm16, int count)
///////////////////////////////////////////////
{
    return VOPL3_Chip->Generate(pcm16, count);
}

static uint32_t VOPL3_PrimaryRead(uint32_t val)
///////////////////////////////////////////////
{
    val &= ~0xFF;
    if ((VOPL3_TimerCtrlReg[0] & (VOPL3_TIMER1_MASK | VOPL3_TIMER1_START)) == VOPL3_TIMER1_START)
        val |= VOPL3_TIMER1_TIMEOUT;
    if ((VOPL3_TimerCtrlReg[1] & (VOPL3_TIMER2_MASK | VOPL3_TIMER2_START)) == VOPL3_TIMER2_START)
        val |= VOPL3_TIMER2_TIMEOUT;
    return val;
}

static uint32_t VOPL3_PrimaryWriteIndex(uint32_t val)
/////////////////////////////////////////////////////
{
    VOPL3_IndexReg[VOPL3_PRIMARY] = VOPL3_Chip->WriteAddr(0x388, val);
    return val;
}

static uint32_t VOPL3_PrimaryWriteData(uint32_t val)
////////////////////////////////////////////////////
{
    if(VOPL3_IndexReg[VOPL3_PRIMARY] == VOPL3_TIMER_REG_INDEX)
    {
        if(val & (VOPL3_TIMER1_START | VOPL3_TIMER1_MASK))
            VOPL3_TimerCtrlReg[0] = val;
        if(val & (VOPL3_TIMER2_START | VOPL3_TIMER2_MASK))
            VOPL3_TimerCtrlReg[1] = val;
    }
    VOPL3_Chip->WriteReg(VOPL3_IndexReg[VOPL3_PRIMARY], val);
    return val;
}

static uint32_t VOPL3_SecondaryRead(uint32_t val)
/////////////////////////////////////////////////
{
    if(VOPL3_IndexReg[VOPL3_SECONDARY] == 0x100 + VOPL3_ADLG_VOLL_REG_INDEX || VOPL3_IndexReg[VOPL3_SECONDARY] == 0x100+VOPL3_ADLG_VOLR_REG_INDEX)
            return VOPL3_ADLG_Volume[VOPL3_IndexReg[VOPL3_SECONDARY]-VOPL3_ADLG_VOLL_REG_INDEX];
    return VOPL3_PrimaryRead(val);
}

static uint32_t VOPL3_SecondaryWriteIndex(uint32_t val)
///////////////////////////////////////////////////////
{
    if(val == 0xFF)
        VOPL3_ADLG_CtrlEnable = 1;
    else if(val == 0xFE)
        VOPL3_ADLG_CtrlEnable = 0;

    VOPL3_IndexReg[VOPL3_SECONDARY] = VOPL3_Chip->WriteAddr(0x38A, val);
    return val;
}

static uint32_t VOPL3_SecondaryWriteData(uint32_t val)
//////////////////////////////////////////////////////
{
    if(/*VOPL3_ADLG_CtrlEnable && */(VOPL3_IndexReg[VOPL3_SECONDARY] == 0x100+VOPL3_ADLG_VOLL_REG_INDEX || VOPL3_IndexReg[VOPL3_SECONDARY] == 0x100+VOPL3_ADLG_VOLR_REG_INDEX))
        VOPL3_ADLG_Volume[VOPL3_IndexReg[VOPL3_SECONDARY]-VOPL3_ADLG_VOLL_REG_INDEX] = val;
    VOPL3_Chip->WriteReg(VOPL3_IndexReg[VOPL3_SECONDARY], val);
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
