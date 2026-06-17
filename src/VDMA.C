
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

#define DMAREADLOG
#define DMAWRITELOG

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
	uint8_t  Masked;         // bool: 1=channel masked
	uint8_t  Complete;       // bool: set by VDMA_SetComplete() - will set DMA_REG_STATUS[0-3]
	uint8_t  e2value;        // byte value written by SB DSP cmd E2 (stored if channel is masked)
	uint8_t  e2channel;      // byte value written by SB DSP cmd E2 (stored if channel is masked)
	uint8_t  InIO[8];        // bool: 1=in the middle of reading count/addr
	uint8_t  DelayUpdate[8]; // bool
};

static struct VDMA_Status vdma;

static const int8_t VDMA_PortChannelMap[16] =
{
    -1, 2, 3, 1, -1, -1, -1, 0, /* ports 80-87 */
    -1, 6, 7, 5, -1, -1, -1, 4, /* ports 88-8F */
};

/* write to ISA DMA controller ports;
 * even if a channel is virtualized the ports are written;
 */

static void VDMA_Write(uint16_t port, uint8_t byte)
///////////////////////////////////////////////////
{

    int index = port;
#ifdef DMAWRITELOG
    dbgprintf(("VDMA_Write(0x%x, 0x%x)\n", port, byte));
#endif
    /* ports 08-0F or D0-DE? */
    if(( port >= DMA_REG_STATUS_CMD && port <= DMA_REG_MULTIMASK) ||
       ( port >= DMA_REG_STATUS_CMD16 && port <= DMA_REG_MULTIMASK16 ) ) {
        int base = 0;
        int channelbase = 0;
        int channel;
        /* ports D0-DE? */
        if( port >= DMA_REG_STATUS_CMD16 ) { /* > DMA16 control register? */
            index = (port - DMA_REG_STATUS_CMD16) / 2 + 8; /* 0xD0-0xDE -> 0x08-0x0F */
            base = 16; /* index base, so base+index are 24-31 */
            channelbase = 4;
        }

        switch ( index ) {
        case DMA_REG_SINGLEMASK: /* port 0x0A */
            channel = byte & 0x3;
            if ( byte & 4 )
                vdma.Masked |= (1 << ( channelbase + channel ));
            else {
                vdma.Masked &= ~(1 << ( channelbase + channel ));
            }
            break;
        case DMA_REG_MODE:     /* port 0x0B */
            channel = byte & 0x3;
            vdma.Modes[channelbase + channel] = byte & ~0x3;
            break;
        case DMA_REG_FLIPFLOP: /* port 0x0C */
            vdma.Regs[base + DMA_REG_FLIPFLOP] = 0;
            break;
        case DMA_REG_IMM_RESET:/* port 0x0D: mask all 4 channels */
            vdma.Regs[base + DMA_REG_FLIPFLOP] = 0;
            vdma.Complete &= ~(channelbase ? 0xf0 : 0x0f);
            vdma.Masked |= (channelbase ? 0xf0 : 0x0f);
            break;
        case DMA_REG_MASK_RESET: /* port 0x0E: unmask all 4 channels */
            vdma.Masked &= ~(channelbase ? 0xf0 : 0x0f);
            break;
        case DMA_REG_MULTIMASK: /* port 0x0F: */
            vdma.Masked &= ~(channelbase ? 0xf0 : 0x0f);
            vdma.Masked |= (channelbase ? (byte << 4) : (byte & 0x0f) );
            break;
        default:
            vdma.Regs[base + index] = byte; /* just store the value, it isn't used */
        }
        /* v1.7: if SB low DMA is unmasked, check if an DSP cmd E2 byte is waiting */
        if ( vdma.e2channel != 0xff && !(vdma.Masked & (1 << vdma.e2channel )))
            VDMA_WriteData(vdma.e2channel,0,1);

    } else if(( (int16_t)port >= DMA_REG_CH0_ADDR && port <= DMA_REG_CH3_COUNTER ) || /* ports 00-07? */
            ( port >= DMA_REG_CH4_ADDR && port <= DMA_REG_CH7_COUNTER )) { /* or ports C0-CE? */
        int channel = ( port >> 1 );
        int base = 0;

        if(  port >= DMA_REG_CH4_ADDR ) {
            index = ( port - DMA_REG_CH4_ADDR ) / 2; /* 0xC0-0xCF -> 0x00-0x07 */
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

/* read an ISA DMA controller port;
 * if channel is virtualized, the index/count registers are read from shadow registers;
 * the control registers are read even if channel is virtualized;
 * not really a problem, since the only control register that's useful to read is the status port.
 */

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
#ifdef DMAREADLOG
            dbgprintf(("VDMA_Read chn=%u, %s: %X (%X)\n", channel, ( (index & 0x1) == 1) ? "counter" : "addr", value, vdma.InIO[channel]));
#endif
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
        dbgprintf(("VDMA_Read(0x%X)=%02x (chn=%u)\n", port, vdma.PageRegs[channel], channel));
        return vdma.PageRegs[channel];
    }

    result = UntrappedIO_IN(port);

    /* the only control port that's interesting is the status;
     * to be changed: don't call VSB_ functions here!
     */
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
#ifdef DMAREADLOG
    dbgprintf(("VDMA_Read(0x%X)=%02x\n", port, result));
#endif
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

    vdma.Masked |= (1 << channel );
    vdma.e2channel = 0xff; /* reset SB DSP E2 callback mechanism */
}

uint32_t VDMA_GetBase(int channel)
//////////////////////////////////
{
    int size = channel < 4 ? 1 : 2;
    return ( vdma.PageRegs[channel] << 16) | (vdma.Base[channel] * size ); //addr reg for 16 bit is real addr/2.
}

int32_t VDMA_GetCount(int channel)
//////////////////////////////////
{
    /* v1.8: return 0 if count == 0xffff, for both 8- and 16-bit */
    if ( vdma.CurCnt[channel] == 0xffff && ( vdma.Complete & (1 << channel )))
        return 0;
	/* v1.8: fixed */
    //return vdma.CurCnt[channel] * ((channel < 4 ) ? 1 : 2) + 1;
    return (vdma.CurCnt[channel] + 1 ) * ((channel < 4 ) ? 1 : 2);
}

uint32_t VDMA_GetIndex(int channel)
///////////////////////////////////
{
    int size = channel < 4 ? 1 : 2;
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
    int shift;
    int base;

    if ( channel <= 3 ) {
        shift = 0;
        base = 0 + ( channel << 1 ); /* regs 0-7 */
    } else {
        shift = 1;
        base = 8 + ( channel << 1 ); /* regs 16-23 */
    }

    //dbgprintf(("VDMA_SetIndexCount (chn %u): Idx=%X, Cnt=%X CurIdx=%X, CurCnt=%X\n", channel, index, count, vdma.CurIdx[channel], vdma.CurCnt[channel] ));
    vdma.CurIdx[channel] = index >> shift;
    count--;
    if( count < 0 ) {
        vdma.CurCnt[channel] = 0xffff;
        VDMA_SetComplete( channel );
        if( VDMA_IsAuto( channel ) ) {
            vdma.CurIdx[channel] = 0;
            vdma.CurCnt[channel] = vdma.MaxCnt[channel];
        }
    } else
        vdma.CurCnt[channel] = count >> shift;

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
    return vdma.CurIdx[channel] << shift;
}

int VDMA_IsAuto(int channel)
////////////////////////////
{
    return( vdma.Modes[channel] & DMA_REG_MODE_AUTO );
}

#if 0
int VDMA_GetWriteMode(int channel)
//////////////////////////////////
{
    return ( (vdma.Modes[channel] & DMA_REG_MODE_OPERATION ) == DMA_REG_MODE_OP_WRITE );
}
#endif

/* v2.0: function now called by VSB_Running() */

int VDMA_IsMasked(int channel)
//////////////////////////////
{
    /* v2.0: the "multimask" port is unreliable to read!
     *       hence the masked bits are now managed by VDMA.
     */
    return( vdma.Masked & ( 1 << channel ) );
}

/* called by (weird and undocumented) DSP cmd 0xE2 */

void VDMA_WriteData(int channel, uint8_t data, uint8_t iscb)
////////////////////////////////////////////////////////////
{
    uint32_t index = VDMA_GetIndex(channel);
    uint32_t addr = VDMA_GetBase(channel) + index;

    //if(VDMA_GetWriteMode(channel)) {
    if ( iscb ) {
        data = vdma.e2value;
        vdma.e2channel = 0xff;
    } else if(!VDMA_IsMasked(channel))
        iscb = 1;

    if ( iscb ) {
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
    } else {
        vdma.e2value = data;
        vdma.e2channel = channel;
    }
}

uint8_t VDMA_Acc(uint16_t port, uint8_t val, uint16_t flags)
////////////////////////////////////////////////////////////
{
    return (flags & TRAPF_OUT) ? (VDMA_Write(port, val), val) : VDMA_Read(port);
}
