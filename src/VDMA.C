
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "CONFIG.H"
#include "PLATFORM.H"
#include "DMA.H"
#include "VDMA.H"
#include "VSB.H"
#include "PTRAP.H"
#include "LINEAR.H"

//registers
static uint16_t VDMA_Regs[32];  /* 0-15 values for ports 00-07, 16-31 values for channel C0-CF */
static uint8_t VDMA_PageRegs[8];
static uint8_t VDMA_Modes[8]; /* bits[2-7] written to DMA_REG_MODE */

/* mode: bit 2-3: operation, 00=verify, 01=write, 10=read
 *       bit 4:   1=auto initialize
 *       bit 5:   direction, 0=increment
 *       bit 6-7: operation mode: 00=demand, 01=single, 10=block, 11=cascade
 */

//internal datas
static uint32_t VDMA_Base[8];      // initial addr (base)
static uint32_t VDMA_CurIdx[8];    // current offset
static int32_t VDMA_MaxCnt[8];     // initial count+1
static int32_t VDMA_CurCnt[8];     // current count+1
static uint8_t VDMA_VMask;         // bool: 1=channel virtualized
static uint8_t VDMA_Complete;      // bool: set by VDMA_SetComplete() - will set DMA_REG_STATUS[0-3]
static uint8_t VDMA_InIO[8];       // bool: 1=in the middle of reading count/addr
static uint8_t VDMA_DelayUpdate[8]; // bool
static const int8_t VDMA_PortChannelMap[16] =
{
    -1, 2, 3, 1, -1, -1, -1, 0, /* ports 80-87 */
    -1, 6, 7, 5, -1, -1, -1, 4, /* ports 88-8F */
};

void VDMA_Write(uint16_t port, uint8_t byte)
////////////////////////////////////////////
{

    int index = port;
    dbgprintf(("VDMA_Write(%x, %x )\n", port, byte));
    /* ports 08-0F or D0-DE? */
    if(( port >= DMA_REG_STATUS_CMD && port <= DMA_REG_MULTIMASK) ||
       ( port >= DMA_REG_STATUS_CMD16 && port <= DMA_REG_MULTIMASK16 ) ) {
        int base = 0;
        int channelbase = 0;
        /* ports D0-DE? */
        if( port >= DMA_REG_STATUS_CMD16 ) {
            index = (port - DMA_REG_STATUS_CMD16) / 2 + 8; /* convert to 8-15 */
            base = 16; /* index base, so base+index are 24-31 */
            channelbase = 4;
        }

        if( index == DMA_REG_FLIPFLOP )
            VDMA_Regs[base + index] = 0;
        else if( index == DMA_REG_MODE ) {
            int channel = byte & 0x3; //0~3
            VDMA_Modes[channelbase + channel] = byte & ~0x3;
        } else
            VDMA_Regs[base + index] = byte;
    } else if(( (int16_t)port >= DMA_REG_CH0_ADDR && port <= DMA_REG_CH3_COUNTER ) || /* ports 00-07? */
            ( port >= DMA_REG_CH4_ADDR && port <= DMA_REG_CH7_COUNTER )) { /* or ports C0-CE? */
        int channel = ( port >> 1 );
        int base = 0;

        if(  port >= DMA_REG_CH4_ADDR ) {
            index = ( port - DMA_REG_CH4_ADDR ) / 2; /* map to 0-7 */
            base = 16; /* index base, so base+index are 16-23 */
            channel = ( index >> 1 ) + 4;
        }
        //dbgprintf(("VDMA_Write: base=%d idx=%x\n", base, index));

        if( ( ( VDMA_Regs[base + DMA_REG_FLIPFLOP]++) & 0x1 ) == 0 ) {
            VDMA_InIO[channel] = true;
            VDMA_Regs[base + index] = (VDMA_Regs[base + index] & ~0xFF) | byte;
        } else {
            VDMA_Regs[base + index] = (VDMA_Regs[base + index] & ~0xFF00) | ( byte << 8 );
            VDMA_InIO[channel] = false;
            VDMA_DelayUpdate[channel] = false;
        }

        /* update base or count */
        if(( index & 0x1 ) == 0 ) {
            VDMA_CurIdx[channel] = 0;
            VDMA_Base[channel] = (VDMA_Base[channel] & ~0xFFFF) | VDMA_Regs[base + index];
        } else
            VDMA_CurCnt[channel] = VDMA_MaxCnt[channel] = VDMA_Regs[base + index] + 1;

    } else if(port >= 0x80 && port <= 0x8F)  {
        int channel = VDMA_PortChannelMap[port - 0x80];
        if( channel != -1 ) {
            VDMA_PageRegs[channel] = byte;
            VDMA_CurIdx[channel] = 0;
            VDMA_Base[channel] &= 0xFFFF;
            VDMA_Base[channel] |= byte << 16;
        }
    }
    UntrappedIO_OUT(port, byte);
}

uint8_t VDMA_Read(uint16_t port)
////////////////////////////////
{
	int channel = -1;
    uint8_t result;
    int index = port;

    //dbgprintf(("VDMA_Read: port=%x\n", port));

    if( port <= DMA_REG_CH3_COUNTER ) /* ports 00-07? */
        channel = (port >> 1);
    else if( port >= DMA_REG_CH4_ADDR && port <= DMA_REG_CH7_COUNTER ) /* ports C0-CE */
        channel = (( port - DMA_REG_CH4_ADDR ) >> 2 ) + 4;
    else if( port >= 0x80 && port <= 0x8F ) /* ports 80-8F */
        channel = VDMA_PortChannelMap[port - 0x80];

    if( channel > 0 && (VDMA_VMask & (1 << channel))) {
        /* select ports 00-03 or 0xC0-0xCE; meaning loword("addr") or "counter" */
        if( ( (int16_t)port >= DMA_REG_CH0_ADDR && port <= DMA_REG_CH3_COUNTER ) ||
           ( port >= DMA_REG_CH4_ADDR && port <= DMA_REG_CH7_COUNTER ) ) {
            int base = 0;
            int value;
            uint8_t ret;
            if( port >= DMA_REG_CH4_ADDR ) {
                index = ( port - DMA_REG_CH4_ADDR ) / 2; /* [index] now 0-15 */
                base = 16;
            }

            value = VDMA_Regs[base + index];
            dbgprintf(("VDMA_Read chn=%u, %s: %X (%X)\n", channel, ( (index & 0x1) == 1) ? "counter" : "addr", value, VDMA_InIO[channel]));
            if( ( ( VDMA_Regs[base + DMA_REG_FLIPFLOP]++) & 0x1 ) == 0 ) {
                VDMA_InIO[channel] = true;
                return value & 0xFF;
            }
            ret = ((value >> 8) & 0xFF);
            /* update addr & counter regs? - page regs?? */
            if( VDMA_DelayUpdate[channel] ) {
                int size = channel <= 3 ? 1 : 2;
                int base2 = channel <= 3 ? (channel << 1) : 16 + ((channel - 4) << 1);
                VDMA_Regs[base2+1] = VDMA_CurCnt[channel] - 1;
                /* v1.4: take care that the base won't go beyond addr+maxcnt (tyrian2k!) */
                //VDMA_Regs[base2] = VDMA_Base[channel] + VDMA_CurIdx[channel];
                VDMA_Regs[base2] = VDMA_Base[channel] + min( VDMA_CurIdx[channel], VDMA_MaxCnt[channel] );
                /* v1.4: no auto update of the page regs */
                //VDMA_PageRegs[channel] = (VDMA_Base[channel] + VDMA_CurIdx[channel]) >> 16;
            }
            VDMA_InIO[channel] = false;
            VDMA_DelayUpdate[channel] = false;
            return ret;
        }
        /* must be a page register */
        dbgprintf(("VDMA_Read(port %X)=%02x (chn=%u)\n", port, VDMA_PageRegs[channel], channel));
        return VDMA_PageRegs[channel];
    }

    /* regs 08-0F and 0D0-DE */
    result = UntrappedIO_IN(port);

    if ( port == DMA_REG_STATUS_CMD || port == DMA_REG_STATUS_CMD16 ) {
        int vchannel = VSB_GetDMA();
        if (( port == DMA_REG_STATUS_CMD && vchannel < 4) || ( port == DMA_REG_STATUS_CMD16 && vchannel >= 4 )) {
            vchannel &= 0x3;
            result &= ~(0x11 << vchannel);  /* bits 0-3: terminal count, bits 4-7: DREQ */
            if ( VSB_Running() )
                result |= (1 << (vchannel+4) );
        }
        dbgprintf(("VDMA_Read(status port %X)=%02x\n", port, result));
    }
    return result;
}

void VDMA_Virtualize(int channel, int enable)
/////////////////////////////////////////////
{
    if( channel >= 0 && channel <= 7 )
        if (enable)
            VDMA_VMask |= (1 << channel);
        else
            VDMA_VMask &= ~(1 << channel);
}

uint32_t VDMA_GetBase(int channel)
//////////////////////////////////
{
    int size = channel <= 3 ? 1 : 2;
    uint32_t page = VDMA_Base[channel] & 0xFF0000; //page not /2
    return (page | (VDMA_Base[channel] & 0xFFFF ) * size ); //addr reg for 16 bit is real addr/2.
}

int32_t VDMA_GetCount(int channel)
//////////////////////////////////
{
    int size = channel <= 3 ? 1 : 2;
    return VDMA_CurCnt[channel] * size;
}

uint32_t VDMA_GetIndex(int channel)
///////////////////////////////////
{
    int size = channel <= 3 ? 1 : 2;
    return VDMA_CurIdx[channel] * size;
}

static void VDMA_SetComplete(int channel)
/////////////////////////////////////////
{
    VDMA_Complete |= 1 << channel;
}

/* set both address and count of a channel.
 * this is called quite often.
 */

uint32_t VDMA_SetIndexCount(int channel, uint32_t index, int32_t count)
///////////////////////////////////////////////////////////////////////
{
    const int size = channel <= 3 ? 1 : 2;
    int base = channel <= 3 ? ( channel << 1 ) : 16 + ( (channel-4) << 1);

    //dbgprintf(("VDMA_SetIndexCount (chn %u): Idx=%X, Cnt=%X CurIdx=%X, CurCnt=%X\n", channel, index, count, VDMA_CurIdx[channel], VDMA_CurCnt[channel] ));
    if( count <= 0 ) {
        count = 0x10000 * size;
        VDMA_SetComplete( channel );
        if(VDMA_GetAuto(channel)) {
            index = 0;
            count = VDMA_MaxCnt[channel] * size;
        }
    }
    VDMA_CurCnt[channel] = count / size;
    VDMA_CurIdx[channel] = index / size;

    /* the VDMA_Regs[] values are either set here or later when the regs are read (DelayUpdate=true) */
    if(!VDMA_InIO[channel]) {
        //dbgprintf(("VDMA_SetIndexCount (chn %u): base: %x\n", channel, VDMA_Base[channel]));
        VDMA_Regs[base+1] = VDMA_CurCnt[channel] - 1; //update counter reg
        /* v1.4: take care that base isn't beyond addr+length */
        VDMA_Regs[base] = VDMA_Base[channel] + min( VDMA_CurIdx[channel], VDMA_MaxCnt[channel] );
        /* v1.4: no auto update of the page regs */
        //VDMA_PageRegs[channel] = (VDMA_Base[channel] + VDMA_CurIdx[channel]) >> 16;
    }
    else
        VDMA_DelayUpdate[channel] = true;
    //dbgprintf(("VDMA_SetIndexCount(%u,%X,%X): returns %X\n", channel, index, count, VDMA_CurIdx[channel] * size ));
    return VDMA_CurIdx[channel] * size;
}

int VDMA_GetAuto(int channel)
/////////////////////////////
{
    return VDMA_Modes[channel] & DMA_REG_MODE_AUTO;
}
int VDMA_GetWriteMode(int channel)
//////////////////////////////////
{
    return ( (VDMA_Modes[channel] & DMA_REG_MODE_OPERATION ) == DMA_REG_MODE_OP_WRITE );
}

void VDMA_WriteData(int channel, uint8_t data)
//////////////////////////////////////////////
{
    if(VDMA_GetWriteMode(channel)) {
        int32_t index = VDMA_GetIndex(channel);
        uint32_t addr = VDMA_GetBase(channel) + index;
        
        if( addr > 1024*1024 ) {
            __dpmi_meminfo info;
            info.address = addr;
            info.size = 1;
            __dpmi_physical_address_mapping( &info );
            *(uint8_t *)(NearPtr(info.address)) = data;
            __dpmi_free_physical_address_mapping( &info );
        } else
            *(uint8_t *)(NearPtr(addr)) = data;

        VDMA_SetIndexCount(channel, index+1, VDMA_GetCount(channel)-1);
    }
}

uint32_t VDMA_DMA(uint32_t port, uint32_t val, uint32_t out)
////////////////////////////////////////////////////////////
{
    return out ? (VDMA_Write(port, val), val) : (val &= ~0xFF, val |= VDMA_Read(port));
}
