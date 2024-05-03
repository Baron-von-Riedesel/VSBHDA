
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

/* mode: bit 2-3: operation, 00=verify, 01=write, 10=read
 *       bit 4:   1=auto initialize
 *       bit 5:   direction, 0=increment
 *       bit 6-7: operation mode: 00=demand, 01=single, 10=block, 11=cascade
 */

struct VDMA_Status {
	//registers
	uint16_t Regs[32];  /* 0-15 values for ports 00-0F, 16-31 values for ports C0-DE */
	uint8_t  PageRegs[8];
	uint8_t  Modes[8]; /* bits[2-7] written to DMA_REG_MODE */

	uint16_t CurIdx[8];      // current offset
	uint16_t CurCnt[8];      // current count
	uint16_t Base[8];        // initial offset (base)
	uint16_t MaxCnt[8];      // initial count
	uint8_t  Virtualized;    // bool: 1=channel virtualized
	uint8_t  Complete;       // bool: set by VDMA_SetComplete() - will set DMA_REG_STATUS[0-3]
	uint8_t  InIO[8];        // bool: 1=in the middle of reading count/addr
	uint8_t  DelayUpdate[8]; // bool
};

static struct VDMA_Status vdma;

static const int8_t VDMA_PortChannelMap[16] =
{
    -1, 2, 3, 1, -1, -1, -1, 0, /* ports 80-87 */
    -1, 6, 7, 5, -1, -1, -1, 4, /* ports 88-8F */
};

static void VDMA_Write(uint16_t port, uint8_t byte)
///////////////////////////////////////////////////
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
            vdma.Regs[base + index] = 0;
        else if( index == DMA_REG_MODE ) {
            int channel = byte & 0x3; //0~3
            vdma.Modes[channelbase + channel] = byte & ~0x3;
        } else
            vdma.Regs[base + index] = byte; /* just store the value, it isn't used */

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

        if( ( ( vdma.Regs[base + DMA_REG_FLIPFLOP]++) & 0x1 ) == 0 ) {
            vdma.InIO[channel] = true;
            vdma.Regs[base + index] = (vdma.Regs[base + index] & ~0xFF) | byte;
        } else {
            vdma.Regs[base + index] = (vdma.Regs[base + index] & ~0xFF00) | ( byte << 8 );
            vdma.InIO[channel] = false;
            vdma.DelayUpdate[channel] = false;
            /* update base or count */
            if(( index & 0x1 ) == 0 ) {
                vdma.CurIdx[channel] = 0;
                vdma.Base[channel] = vdma.Regs[base + index];
            } else
                vdma.CurCnt[channel] = vdma.MaxCnt[channel] = vdma.Regs[base + index];
            dbgprintf(("VDMA_Write: channel %u, Base/MaxCnt=%X/%X CurIdx/CurCnt=%X/%X Reg[%u]=%X\n",
                    channel, vdma.Base[channel], vdma.MaxCnt[channel], vdma.CurIdx[channel], vdma.CurCnt[channel], base+index, vdma.Regs[base+index]));
        }

    } else if(port >= 0x80 && port <= 0x8F)  {
        int channel = VDMA_PortChannelMap[port - 0x80];
        if( channel != -1 ) {
            vdma.PageRegs[channel] = byte;
            vdma.CurIdx[channel] = 0;
        }
    }
    UntrappedIO_OUT(port, byte);
}

static uint8_t VDMA_Read(uint16_t port)
///////////////////////////////////////
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

    if( channel >= 0 && (vdma.Virtualized & (1 << channel))) {
        /* select ports 00-07 or 0xC0-0xCE; it's either "addr" or "counter" */
        if( ( (int16_t)port >= DMA_REG_CH0_ADDR && port <= DMA_REG_CH3_COUNTER ) ||
           ( port >= DMA_REG_CH4_ADDR && port <= DMA_REG_CH7_COUNTER ) ) {
            int base = 0;
            int value;
            uint8_t ret;
            if( channel > 3 ) {
                index = ( port - DMA_REG_CH4_ADDR ) / 2; /* [index] now 0-7 */
                base = 16;
            }

            value = vdma.Regs[base + index];
            //dbgprintf(("VDMA_Read chn=%u, %s: %X (%X)\n", channel, ( (index & 0x1) == 1) ? "counter" : "addr", value, vdma.InIO[channel]));
            if( ( ( vdma.Regs[base + DMA_REG_FLIPFLOP]++) & 0x1 ) == 0 ) {
                vdma.InIO[channel] = true;
                return value & 0xFF;
            }
            ret = ((value >> 8) & 0xFF);
            /* update Index & Count regs ... but page regs?? */
            /* v1.4: take care that the base won't go beyond addr+maxcnt (tyrian2k!) */
            /* v1.4: no auto update of the page regs */
            if( vdma.DelayUpdate[channel] ) {
                base += (index & ~1);
                //vdma.Regs[base] = vdma.Base[channel] + vdma.CurIdx[channel];
                vdma.Regs[base]   = vdma.Base[channel] + min( vdma.CurIdx[channel], vdma.MaxCnt[channel] + 1 );
                vdma.Regs[base+1] = vdma.CurCnt[channel];
                //vdma.PageRegs[channel] = (vdma.Base[channel] + vdma.CurIdx[channel]) >> 16;
                vdma.DelayUpdate[channel] = false;
            }
            vdma.InIO[channel] = false;
            return ret;
        }
        /* must be a page register */
        dbgprintf(("VDMA_Read(port %X)=%02x (chn=%u)\n", port, vdma.PageRegs[channel], channel));
        return vdma.PageRegs[channel];
    }

    /* regs 08-0F and 0D0-DE */
    result = UntrappedIO_IN(port);

    if ( port == DMA_REG_STATUS_CMD || port == DMA_REG_STATUS_CMD16 ) {
        int vchannel = VSB_GetDMA();
        if (( port == DMA_REG_STATUS_CMD && vchannel < 4) || ( port == DMA_REG_STATUS_CMD16 && vchannel >= 4 )) {
            uint8_t bComplete = ( vdma.Complete & (1 << vchannel ) );
            uint8_t bBitPos = vchannel & 0x3;
            result &= ~(0x11 << bBitPos);  /* bits 0-3: terminal count, bits 4-7: DREQ */
            if ( VSB_Running() )
                result |= (1 << (bBitPos+4) );
            result |= ( vchannel > 3 ) ? ( bComplete >> 4) : bComplete;
            vdma.Complete &= ~bComplete; /* reset on read? */
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
            vdma.Virtualized |= (1 << channel);
        else
            vdma.Virtualized &= ~(1 << channel);
}

uint32_t VDMA_GetBase(int channel)
//////////////////////////////////
{
    int size = channel <= 3 ? 1 : 2;
    return ( vdma.PageRegs[channel] << 16) | (vdma.Base[channel] * size ); //addr reg for 16 bit is real addr/2.
}

int32_t VDMA_GetCount(int channel)
//////////////////////////////////
{
    int size = channel <= 3 ? 1 : 2;
    return vdma.CurCnt[channel] * size + 1;
}

uint32_t VDMA_GetIndex(int channel)
///////////////////////////////////
{
    int size = channel <= 3 ? 1 : 2;
    return vdma.CurIdx[channel] * size;
}

static void VDMA_SetComplete(int channel)
/////////////////////////////////////////
{
    vdma.Complete |= 1 << channel;
}

/* set
 * - CurIdx[] & CurCnt[] of a channel.
 * if no update process is currently active ( InIO[] == false ),
 * Regs[] are also updated here; else DelayUpdate[] is set to true
 * and Regs[] are updated in VDMA_Read().
 */

uint32_t VDMA_SetIndexCount(int channel, uint32_t index, int32_t count)
///////////////////////////////////////////////////////////////////////
{
    int size;
    int base;

    if ( channel <= 3 ) {
        size = 1;
        base = 0 + ( channel << 1 ); /* regs 0-7 */
    } else {
        size = 2;
        base = 8 + ( channel << 1 ); /* regs 16-23 */
    }

    //dbgprintf(("VDMA_SetIndexCount (chn %u): Idx=%X, Cnt=%X CurIdx=%X, CurCnt=%X\n", channel, index, count, vdma.CurIdx[channel], vdma.CurCnt[channel] ));
    vdma.CurIdx[channel] = index / size;
    count--;
    if( count < 0 ) {
        vdma.CurCnt[channel] = 0xffff;
        VDMA_SetComplete( channel );
        if(VDMA_GetAuto(channel)) {
            vdma.CurIdx[channel] = 0;
            vdma.CurCnt[channel] = vdma.MaxCnt[channel];
        }
    } else
        vdma.CurCnt[channel] = count / size;

    /* the VDMA_Regs[] values are either set here or later when the regs are read (DelayUpdate=true) */
    /* v1.4: take care that Regs[base] isn't beyond addr+length */
    /* v1.4: auto update of the page regs removed */
    if(!vdma.InIO[channel]) {
        vdma.Regs[base] = vdma.Base[channel] + min( vdma.CurIdx[channel], vdma.MaxCnt[channel] + 1 );
        vdma.Regs[base+1] = vdma.CurCnt[channel];
        //vdma.PageRegs[channel] = (vdma.Base[channel] + vdma.CurIdx[channel]) >> 16;
        //dbgprintf(("VDMA_SetIndexCount (chn %u): Idx/Cnt=%X/%X\n", channel, vdma.Regs[base], vdma.Regs[base+1] ));
    } else
        vdma.DelayUpdate[channel] = true;

    //dbgprintf(("VDMA_SetIndexCount(%u,%X,%X): returns %X\n", channel, index, count, vdma.CurIdx[channel] * size ));
    return vdma.CurIdx[channel] * size;
}

int VDMA_GetAuto(int channel)
/////////////////////////////
{
    return vdma.Modes[channel] & DMA_REG_MODE_AUTO;
}
int VDMA_GetWriteMode(int channel)
//////////////////////////////////
{
    return ( (vdma.Modes[channel] & DMA_REG_MODE_OPERATION ) == DMA_REG_MODE_OP_WRITE );
}

/* called by DSP cmd 0xE2 */

void VDMA_WriteData(int channel, uint8_t data)
//////////////////////////////////////////////
{
    if(VDMA_GetWriteMode(channel)) {
        uint32_t index = VDMA_GetIndex(channel);
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

        VDMA_SetIndexCount(channel, index+1, VDMA_GetCount(channel) - 1);
    }
}

uint32_t VDMA_Acc(uint32_t port, uint32_t val, uint32_t out)
////////////////////////////////////////////////////////////
{
    return out ? (VDMA_Write(port, val), val) : (val &= ~0xFF, val |= VDMA_Read(port));
}
