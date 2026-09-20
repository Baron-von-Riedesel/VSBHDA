
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include "CONFIG.H"
#include "PLATFORM.H"
#include "DPMI.H"
#include "LINEAR.H"
#include "DMA.H"
#include "VDMA.H"
#include "VSB.H"
#include "PTRAP.H"

/* mode: bit 2-3: operation, 00=verify, 01=write, 10=read
 *       bit 4:   1=auto initialize
 *       bit 5:   direction, 0=increment
 *       bit 6-7: operation mode: 00=demand, 01=single, 10=block, 11=cascade
 */

//#define DMAREADLOG
//#define DMAWRITELOG

struct VDMA_Status {
	uint16_t IdxCntRegs[4];  /* 0-1 values for ldma idx/cnt regs, 2-3 values for hdma idx/cnt regs */
	uint16_t Base[2];        /* base (A00-A15) ldma/hdma */
	uint16_t MaxPos[2];      /* initial count (=max position) ldma/hdma */
	uint16_t CurPos[2];      /* current position ldma/hdma */
	uint8_t  PageRegs[2];    /* page registers ldma/hdma */
	uint8_t  FlipFlop[2];    /* flipflop for ldma/hdma */
	uint8_t  InIO[2];        /* 1=in the middle of reading count/addr; ldma/hdma */
	uint8_t  DelayUpdate[2]; /* 1=delayed update because InIO=1; ldma/hdma */
	uint8_t  Modes[8];       /* bits[2-7] written to DMA_REG_MODE */

	uint8_t  Virtualized;    // bool: 1=channel virtualized
	uint8_t  Masked;         // bool: 1=channel masked
	uint8_t  Complete;       // bool: set by VDMA_SetComplete() - will set DMA_REG_STATUS[0-3]
	uint8_t  e2value;        // byte value written by SB DSP cmd E2 (stored if channel is masked)
	uint8_t  e2channel;      // byte value written by SB DSP cmd E2 (stored if channel is masked)
};

static struct VDMA_Status vdma;

/* write to ISA DMA controller ports 08-0F & D0-DE */

static void VDMA_Write(uint16_t port, uint8_t byte)
///////////////////////////////////////////////////
{

    int index;
    int channelbase;
    int channel;
#ifdef DMAWRITELOG
    dbgprintf(("VDMA_Write(0x%x, 0x%x)\n", port, byte));
#endif
    /* ports 08-0F or D0-DE? */
    if ( !(port & 0x80 )) {
        index = port;
        channelbase = 0;
    } else {
        index = (port >> 1) & 0xf;
        channelbase = 4;
    }

    switch ( index ) {
    case DMA_REG_SINGLEMASK: /* port 0x0A */
        channel = (byte & 0x3) + channelbase;
        if ( byte & 4 )
            vdma.Masked |= 1 << channel;
        else
            vdma.Masked &= ~(1 << channel);
        break;
    case DMA_REG_MODE:     /* port 0x0B */
        channel = (byte & 0x3) + channelbase;
        vdma.Modes[channel] = byte & ~0x3;
        break;
    case DMA_REG_FLIPFLOP: /* port 0x0C */
        vdma.FlipFlop[channelbase >> 2] = 0;
        break;
    case DMA_REG_IMM_RESET:/* port 0x0D: mask all 4 channels */
        vdma.FlipFlop[channelbase >> 2] = 0;
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
    //default:
    //    vdma.Regs[base + index] = byte; /* just store the value, it isn't used */
    }
    /* v1.7: if SB low DMA is unmasked, check if an DSP cmd E2 byte is waiting */
    if ( vdma.e2channel != 0xff && !(vdma.Masked & (1 << vdma.e2channel )))
        VDMA_WriteData(vdma.e2channel,0,1);

    UntrappedIO_OUT(port, byte);
}

/* read ISA DMA controller ports 08-0F & D0-DE */

#define IsVirtualized(chn) (vdma.Virtualized & ( 1 << chn ))

static uint8_t VDMA_Read(uint16_t port)
///////////////////////////////////////
{
    //int channel;
    uint8_t result;
    //int index = port;

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
    channel &= 0x7;
    if ( enable )
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
    return ( vdma.PageRegs[channel>>2] << 16) | (vdma.Base[channel>>2] * size ); //addr reg for 16 bit is real addr/2.
}

int32_t VDMA_GetCount(int channel)
//////////////////////////////////
{
    int idx = channel >> 2;
    /* v1.8: return 0 if count == 0xffff, for both 8- and 16-bit */
    if ( vdma.CurPos[idx] > vdma.MaxPos[idx] && ( vdma.Complete & (1 << channel )))
        return 0;
    /* v1.8: fixed */
    return ((vdma.MaxPos[idx] - vdma.CurPos[idx] + 1 ) << idx);
}

uint32_t VDMA_GetIndex(int channel)
///////////////////////////////////
{
    int idx = channel >> 2;
    return (vdma.CurPos[idx] << idx);
}

static void VDMA_SetComplete(int channel)
/////////////////////////////////////////
{
    vdma.Complete |= 1 << channel;
}

/* update CurPos[] of ldma/hdma.
 * if no update process is currently active ( InIO[] == false ),
 * Regs[] are also updated here; else DelayUpdate[] is set to true
 * and Regs[] are updated in VDMA_Read().
 */

uint32_t VDMA_UpdatePos(int channel, uint32_t addbytes)
///////////////////////////////////////////////////////
{
    int shift;
    int base;

    if ( channel <= 3 ) {
        shift = 0;
        base = 0;
    } else {
        shift = 1;
        base = 1;
    }

    //dbgprintf(("VDMA_UpdatePos (chn %u, addbytes=%X): CurPos=%X\n", channel, addbytes, vdma.CurPos[base] ));
    vdma.CurPos[base] += addbytes >> shift;

    if( vdma.CurPos[base] > vdma.MaxPos[base] ) {
        VDMA_SetComplete( channel );
        if( VDMA_IsAuto( channel ) ) {
            vdma.CurPos[base] = 0;
        }
    }

    /* the VDMA_Regs[] values are either set here or later when the regs are read (DelayUpdate=true) */
    /* v1.4: take care that Regs[base] isn't beyond addr+length */
    /* v1.4: auto update of the page regs removed */
    if(!vdma.InIO[base]) {
        vdma.IdxCntRegs[base*2] = vdma.Base[base] + min( vdma.CurPos[base], vdma.MaxPos[base] + 1 );
        vdma.IdxCntRegs[base*2+1] = vdma.MaxPos[base] - vdma.CurPos[base];
        //vdma.PageRegs[base] = (vdma.Base[base] + vdma.CurPos[base]) >> 16;
        //dbgprintf(("VDMA_UpdatePos(chn %u): IdxCnt=%X %X\n", channel, vdma.IdxCntRegs[base*2], vdma.IdxCntRegs[base*2+1] ));
    } else
        vdma.DelayUpdate[base] = true;

    //dbgprintf(("VDMA_UpdatePos(%u,%X): returns %X\n", channel, addbytes, vdma.CurPos[base] << shift ));
    return vdma.CurPos[base] << shift;
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

/* v2.0: function now called by VSB_Running();
 */

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

        VDMA_UpdatePos(channel, 1);
    } else {
        vdma.e2value = data;
        vdma.e2channel = channel;
    }
}

static uint8_t VDMA_Acc(uint16_t port, uint8_t val, uint16_t flags)
///////////////////////////////////////////////////////////////////
{
    return (flags & TRAPF_OUT) ? (VDMA_Write(port, val), val) : VDMA_Read(port);
}

/* handle DMA idx/cnt port access;
 * called for virtualized channels only.
 */

static void WriteIdxCnt(uint16_t port, uint8_t byte)
////////////////////////////////////////////////////
{

    int index;
    int base;

    base = port >> 7;
    index = ( port >> base ) & 1;
    index += base*2;

#ifdef DMAWRITELOG
    dbgprintf(("VDMA.WriteIdxCnt(0x%x, 0x%x)\n", port, byte));
#endif

    if( ( ( vdma.FlipFlop[base]++) & 0x1 ) == 0 ) {
        vdma.InIO[base] = true;
        vdma.IdxCntRegs[index] = (vdma.IdxCntRegs[index] & ~0xFF) | byte;
        return;
    }
    vdma.IdxCntRegs[index] = (vdma.IdxCntRegs[index] & ~0xFF00) | ( byte << 8 );
    vdma.InIO[base] = false;
    vdma.DelayUpdate[base] = false;
    /* update base or count */
    if(( index & 0x1 ) == 0 ) {
        vdma.CurPos[base] = 0;
        vdma.Base[base] = vdma.IdxCntRegs[index];
    } else
        vdma.MaxPos[base] = vdma.IdxCntRegs[index];
    dbgprintf(("VDMA.WriteIdxCnt: 16bit=%u, Base=%X CurPos/MaxPos=%X/%X Reg[%u]=%X\n",
               base, vdma.Base[base], vdma.CurPos[base], vdma.MaxPos[base], index, vdma.IdxCntRegs[index]));
    return;
}

/* handle DMA idx/cnt port access;
 * called for virtualized channels only.
 */

static uint8_t ReadIdxCnt(uint16_t port)
////////////////////////////////////////
{
    uint8_t ret;
    int index;
    int base;
    int value;

    base = port >> 7;
    index = ( port >> base ) & 1;
    index += base*2;

#ifdef DMAREADLOG
    dbgprintf(("VDMA.ReadIdxCnt 16bit=%u, %s: %X (%X)\n", base, (index & 0x1) ? "counter" : "addr", value, vdma.InIO[base]));
#endif

    value = vdma.IdxCntRegs[index];
    if( ( ( vdma.FlipFlop[base]++) & 0x1 ) == 0 ) {
        vdma.InIO[base] = true;
        return value & 0xFF;
    }
    ret = ((value >> 8) & 0xFF);
    /* update Index & Count regs ... but page regs?? */
    /* v1.4: take care that the base won't go beyond addr+maxcnt (tyrian2k!) */
    /* v1.4: no auto update of the page regs */
    if( vdma.DelayUpdate[base] ) {
        index &= ~1;
        //vdma.IdxCntRegs[index] = vdma.Base[base] + vdma.CurPos[base];
        vdma.IdxCntRegs[index]   = vdma.Base[base] + min( vdma.CurPos[base], vdma.MaxPos[base] + 1 );
        vdma.IdxCntRegs[index+1] = vdma.MaxPos[base] - vdma.CurPos[base];
        //vdma.PageRegs[base] = (vdma.Base[base] + vdma.CurPos[base]) >> 16;
        vdma.DelayUpdate[base] = false;
    }
    vdma.InIO[base] = false;
    return ret;
}

static uint8_t AccIdxCnt(uint16_t port, uint8_t val, uint16_t flags)
////////////////////////////////////////////////////////////////////
{
    return (flags & TRAPF_OUT) ? (WriteIdxCnt(port, val), val) : ReadIdxCnt(port);
}

/* handle trapping of DMA page registers */

static uint8_t AccPage(uint16_t port, uint8_t val, uint16_t flags)
//////////////////////////////////////////////////////////////////
{
    static const int8_t PortChannelMap[] = {
        2, 3, 1, -1, -1, -1, 0, /* ports 81-87 */
    -1, 6, 7, 5, -1, -1, -1, 4, /* ports 88-8F */
    };
    int channel = PortChannelMap[port - 0x81];
    int idx = channel >> 2;

    /* next if() should always be true since this function is called only
     * for the virtualized channels */
    if( channel >= 0 && IsVirtualized( channel ) ) {
        if ( flags & TRAPF_OUT ) {
            dbgprintf(("VDMA.WritePage(0x%X [chn=%u], %X)\n", port, channel, val ));
            vdma.PageRegs[idx] = val;
            vdma.CurPos[idx] = 0; /* ??? */
            return val;
        } else {
            dbgprintf(("VDMA.ReadPage(0x%X [chn=%u])=%02x\n", port, channel, vdma.PageRegs[idx] ));
            return vdma.PageRegs[idx];
        }
    }
    /* should never reach here */
    dbgprintf(("VDMA.AccPage(0x%X, 0x%X, 0x%X) - shouldn't happen!\n", port, val, flags ));
    return (( flags & TRAPF_OUT ) ? (UntrappedIO_OUT( port, val ), val ) : UntrappedIO_IN(port) );
}

/* called by VSB_Init() */

void VDMA_PortTrap( int ldma, int hdma )
////////////////////////////////////////
{
    static const PORT_TRAP_HANDLER LDMA_ph[] = {
        AccIdxCnt, AccIdxCnt,    /* idx+cnt for low dma */
        VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, /* 0x08-0x0F */
    };
#if 0//SB16 /* trap handler vector is the same as for ldma */
    static const PORT_TRAP_HANDLER HDMA_ph[] = {
        AccIdxCnt, AccIdxCnt,    /* base+cnt for high dma */
        VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, VDMA_Acc, /* 0xD0-0xDE */
    };
#endif
    static const PORT_TRAP_HANDLER DMAPG_ph[] = {
        AccPage, /* page port for low dma */
#if SB16
        AccPage, /* page port for high dma */
#endif
    };
    /* translate channels -> ports */
    static const uint8_t ChannelPageMap[] = { 0x87, 0x83, 0x81, 0x82, 0x8f, 0x8b, 0x89, 0x8a };
    unsigned int portmap;
    unsigned int portmappg = 0;
    /* low dma: adjust the entry for DMA channel addr/count */
    portmap = 0xff00;
    portmap >>= ldma << 1;
    portmap |= 3;
    PTRAP_AddRange( ldma << 1, portmap, LDMA_ph );
    /* low dma: adjust the entry for DMA page reg */
    portmappg |= (1 << (ChannelPageMap[ ldma ] - 0x81));
#if SB16
    if ( hdma > 4 ) {
        portmap = 0x55550000;
        portmap >>= ((hdma - 4) << 2);
        portmap |= 5;
        PTRAP_AddRange( ((hdma - 4) << 2) + 0xC0, portmap, LDMA_ph );
        portmappg |= (1 << (ChannelPageMap[ hdma ] - 0x81));
    }
#endif
    PTRAP_AddRange( 0x81, portmappg, DMAPG_ph );
    return;
}

