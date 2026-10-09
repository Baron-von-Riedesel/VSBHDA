/* VDMAVXD.C: virtual DMA of VSBVXD (replaces VDMA.C).  The SB DMA channels
 * are virtualized with VDMAD; VDMAD keeps emulating the DMA controller
 * registers for the VMs and calls vsb_dma_changed() when a VM changes the
 * state of a channel.  The emulation reads the samples itself and reports
 * its progress back with VDMAD_Set_Virt_State, so a program polling the
 * current address/count registers sees the transfer advance. */

#include <stdint.h>
#include <stdbool.h>

#include "CONFIG.H"
#include "DMA.H"
#include "VDMA.H"
#include "VXDLIB.H"

static struct vchan {
	unsigned handle;     /* VDMAD handle, 0 = not virtualized */
	unsigned vm;         /* VM that programmed the channel */
	uint32_t vaddr;      /* address as programmed (VDMAD virtual state) */
	uint32_t lin;        /* ring 0 linear address of the buffer */
	uint32_t total;      /* transfer size in bytes */
	uint32_t pos;        /* bytes transferred */
	uint8_t  mode;
	uint8_t  flags;      /* VDMAD flags */
	uint8_t  masked;
	uint8_t  complete;
	uint8_t  updating;   /* we're calling VDMAD_Set_Virt_State */
} ch[8];

extern unsigned vsb_owner_vm;

/* VDMAD virtual state (as observed with Windows 3.11): the address is the
 * VM's high linear address of the buffer (usable from ring 0), the count is
 * the transfer size in bytes; bit 2 of the flags is set while the channel
 * is unmasked.  A VM writing the count register replaces only the low 16
 * bits of the count, so the count written back must stay below 64 kB. */
#define DMAST_UNMASKED 0x04

void vsb_dma_changed( unsigned dmah, unsigned vm )
{
	struct dmastate st;
	struct vchan *c;
	int channel;

	for ( channel = 0; channel < 8 && ch[channel].handle != dmah; channel++ );
	if ( channel == 8 || ch[channel].updating )
		return;
	c = &ch[channel];
	vxd_get_dma_state( dmah, vm, &st );
	c->flags = st.flags;
	c->masked = !( st.flags & DMAST_UNMASKED );
	if ( c->vm != vm || st.addr != c->vaddr + c->pos ||
		( st.count != c->total - c->pos && !( c->pos == c->total && st.count == 0x10000 ) ) ) {
		/* (re)programmed */
		c->vm = vm;
		c->vaddr = st.addr;
		c->lin = st.addr;
		c->total = st.count & 0x1FFFF;
		c->pos = 0;
		c->complete = 0;
	}
	c->mode = st.mode;
	if ( !c->masked )
		vsb_owner_vm = vm;
	vlog( "VDMA%d: t=%u vm %x addr %x count %x mode %x flags %x -> lin %x size %x\n",
		 channel, vxd_get_time(), vm, st.addr, st.count, st.mode, st.flags, c->lin, c->total );
}

/* SB_Init: virtualize the SB DMA channels */
void VDMA_PortTrap( int ldma, int hdma )
{
	if ( ldma >= 0 && !( ch[ldma].handle = vxd_virtualize_dma( ldma ) ) )
		vlog( "VSBVXD: VDMAD_Virtualize_Channel(%d) failed\n", ldma );
	if ( hdma >= 4 && hdma < 8 && !( ch[hdma].handle = vxd_virtualize_dma( hdma ) ) )
		vlog( "VSBVXD: VDMAD_Virtualize_Channel(%d) failed\n", hdma );
}

/* the emulation (SNDISR.C) reads from VDMA_GetBase() + VDMA_GetPos() */
uint32_t VDMA_GetBase( int channel ) { return ch[channel].lin; }
uint32_t VDMA_GetPos( int channel ) { return ch[channel].pos; }

int32_t VDMA_GetCount( int channel )
{
	return ch[channel].complete && !VDMA_IsAuto( channel ) ? 0 : ch[channel].total - ch[channel].pos;
}

int VDMA_IsAuto( int channel ) { return ch[channel].mode & DMA_REG_MODE_AUTO; }
int VDMA_IsMasked( int channel ) { return ch[channel].masked || !ch[channel].total; }

uint32_t VDMA_UpdatePos( int channel, uint32_t addbytes )
{
	struct vchan *c = &ch[channel];
	struct dmastate st;

	c->pos += addbytes;
	if ( c->pos >= c->total ) {
		c->complete = 1;
		c->pos = VDMA_IsAuto( channel ) ? 0 : c->total;
	}
	/* report the progress: current address and remaining count */
	st.addr = c->vaddr + c->pos;
	/* VDMAD keeps count - 1; after the terminal count the register reads
	 * FFFFh like on a real 8237 (a count of 0 would make it FFFFFFFFh) */
	st.count = c->pos < c->total ? c->total - c->pos : 0x10000;
	st.mode = c->mode;
	st.flags = c->flags;
	c->updating = 1;
	vxd_set_dma_state( c->handle, c->vm, &st );
	c->updating = 0;
	return c->pos;
}

/* DSP command E2 writes a byte to DMA memory */
void VDMA_WriteData( int channel, uint8_t data, uint8_t iscb )
{
	(void)iscb;
	if ( ch[channel].total ) {
		*(uint8_t *)( ch[channel].lin + ch[channel].pos ) = data;
		VDMA_UpdatePos( channel, 1 );
	}
}
