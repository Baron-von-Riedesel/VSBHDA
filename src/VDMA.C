
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "CONFIG.H"
#include "PLATFORM.H"
#include "DMA.H"
#include "VDMA.H"
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
static uint8_t VDMA_VMask[8];      // bool: 1=enabled, 0=disabled
static uint32_t VDMA_Addr[8];      // initial addr (base)
static uint32_t VDMA_CurIdx[8];    // current offset
static int32_t VDMA_Length[8];     // initial length
static int32_t VDMA_CurCnt[8];     // current count

static uint8_t VDMA_InIO[8];       // bool: 1=in the middle of reading counter/addr
static uint8_t VDMA_DelayUpdate[8]; // bool
static uint8_t VDMA_Complete[8];   //
static const uint8_t VDMA_PortChannelMap[16] =
{
    -1, 2, 3, 1, -1, -1, -1, 0, /* ports 80-87 */
    -1, 6, 7, 5, -1, -1, -1, 4, /* ports 88-8F */
};

#define VMDA_IS_CHANNEL_VIRTUALIZED(channel) (channel != -1 && VDMA_VMask[channel])

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

        /* update addr or counter */
        if(( index & 0x1 ) == 0 ) {
            VDMA_CurIdx[channel] = 0;
            VDMA_Addr[channel] = (VDMA_Addr[channel] & ~0xFFFF) | VDMA_Regs[base + index];
        } else
            VDMA_CurCnt[channel] = VDMA_Length[channel] = VDMA_Regs[base + index] + 1;

    } else if(port >= 0x80 && port <= 0x8F)  {
        int channel = VDMA_PortChannelMap[port - 0x80];
        if( channel != -1 ) {
            VDMA_PageRegs[channel] = byte;
            VDMA_CurIdx[channel] = 0;
            VDMA_Addr[channel] &= 0xFFFF;
            VDMA_Addr[channel] |= byte << 16;
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

    if( VMDA_IS_CHANNEL_VIRTUALIZED( channel ) ) {
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
            //dbgprintf(("base:%d port:%d\n", base, port));

            value = VDMA_Regs[base + index];
            //dbgprintf(("VDMA_Read %s: %d\n", ( (index & 0x1) == 1) ? "counter" : "addr", value));
            if( ( ( VDMA_Regs[base + DMA_REG_FLIPFLOP]++) & 0x1 ) == 0 ) {
                VDMA_InIO[channel] = true;
                dbgprintf(("VDMA_Read(port %X)=%02x (chn=%u %s)\n", port, value & 0xFF, channel, ( index & 0x1 ) ? "counter" : "addr" ));
                return value & 0xFF;
            }
            ret = ((value >> 8) & 0xFF);
            if( VDMA_DelayUpdate[channel] ) {
                int size = channel <= 3 ? 1 : 2;
                int base2 = channel <= 3 ? (channel << 1) : 16 + ((channel - 4) << 1);
                VDMA_Regs[base2+1] = VDMA_CurCnt[channel]-1; //update counter reg
                VDMA_Regs[base2] = VDMA_Addr[channel] + VDMA_CurIdx[channel]; //update addr reg
                VDMA_PageRegs[channel] = (VDMA_Addr[channel] + VDMA_CurIdx[channel]) >> 16;
            }
            VDMA_InIO[channel] = false;
            VDMA_DelayUpdate[channel] = false;
            dbgprintf(("VDMA_Read(port %X)=%02x (chn=%u)\n", port, ret, channel));
            return ret;
        }
        /* must be a page register */
        dbgprintf(("VDMA_Read(port %X)=%02x (chn=%u)\n", port, VDMA_PageRegs[channel], channel));
        return VDMA_PageRegs[channel];
    }

    /* regs 08-0F and 0D0-DE */
    result = UntrappedIO_IN(port);

    if( port == DMA_REG_STATUS_CMD || port == DMA_REG_STATUS_CMD16 ) {
        int channel = (( port == DMA_REG_STATUS_CMD ) ? 0 : 4);
        int i;
        for( i = 0; i < 4; i++, channel++ ) {
            if( VMDA_IS_CHANNEL_VIRTUALIZED( channel ) ) {
                result &= ~((1 << i) | (1 << (i+4)));  /* bits 0-3: terminal count, bits 4-7: DREQ */
                if ( VDMA_Complete[channel] )
                    result |= ( 1 << i );
                else if ( VDMA_CurIdx[channel] != -1 && VDMA_CurIdx[channel] < VDMA_GetCounter(channel) )
                    result |= ( 1 << (i+4) );
                VDMA_Complete[channel] = 0; //clear on read
            }
        }
        dbgprintf(("VDMA_Read(status port %X)=%02x\n", port, result));
    }
    return result;
}

void VDMA_Virtualize(int channel, int enable)
/////////////////////////////////////////////
{
    if( channel >= 0 && channel <= 7 )
        VDMA_VMask[channel] = enable ? 1 : 0;
}

uint32_t VDMA_GetAddress(int channel)
/////////////////////////////////////
{
    int size = channel <= 3 ? 1 : 2;
    uint32_t page = VDMA_Addr[channel] & 0xFF0000; //page not /2
    return (page | (VDMA_Addr[channel] & 0xFFFF ) * size ); //addr reg for 16 bit is real addr/2.
}

int32_t VDMA_GetCounter(int channel)
////////////////////////////////////
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

static void VDMA_ToggleComplete(int channel)
////////////////////////////////////////////
{
    VDMA_Complete[channel] = 1;
}

/* set both address and count of a channel.
 * this is called quite often.
 */

uint32_t VDMA_SetIndexCounter(int channel, uint32_t index, int32_t counter)
///////////////////////////////////////////////////////////////////////////
{
    const int size = channel <= 3 ? 1 : 2;
    int base = channel <= 3 ? ( channel << 1 ) : 16 + ( (channel-4) << 1);

    if( counter <= 0 ) {
        counter = 0x10000 * size;
        VDMA_ToggleComplete( channel );
        if(VDMA_GetAuto(channel)) {
            index = 0;
            counter = VDMA_Length[channel] * size;
        }
    }
    //dbgprintf(("counter: %d, index: %d, size: %d\n", counter, index, size));
    VDMA_CurCnt[channel] = counter / size;
    VDMA_CurIdx[channel] = index / size;

    if(!VDMA_InIO[channel]) {
        //dbgprintf(("VDMA channel: %d, addr: %x\n", channel, VDMA_Addr[channel]));
        VDMA_Regs[base+1] = VDMA_CurCnt[channel] - 1; //update counter reg
        VDMA_Regs[base] = VDMA_Addr[channel] + VDMA_CurIdx[channel] % (VDMA_Length[channel]+1); //update addr reg
        //VDMA_PageRegs[channel] = (VDMA_Addr[channel] + VDMA_CurIdx[channel]) >> 16;
    }
    else
        VDMA_DelayUpdate[channel] = true;
    //dbgprintf(("VDMA_SetIndexCounter(%u,%X,%X): returns %X\n", channel, index, counter, VDMA_CurIdx[channel] * size ));
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
        uint32_t addr = VDMA_GetAddress(channel) + index;
        
        if( addr > 1024*1024 ) {
            __dpmi_meminfo info;
            info.address = addr;
            info.size = 1;
            __dpmi_physical_address_mapping( &info );
            *(uint8_t *)(NearPtr(info.address)) = data;
            __dpmi_free_physical_address_mapping( &info );
        } else
            *(uint8_t *)(NearPtr(addr)) = data;

        VDMA_SetIndexCounter(channel, index+1, VDMA_GetCounter(channel)-1);
    }
}

uint32_t VDMA_DMA(uint32_t port, uint32_t val, uint32_t out)
////////////////////////////////////////////////////////////
{
    return out ? (VDMA_Write(port, val), val) : (val &= ~0xFF, val |= VDMA_Read(port));
}
