/* VIRQVXD.C: emulated SB interrupt of VSBVXD (replaces VIRQ.C).  The SB
 * IRQ is virtualized with VPICD; VIRQ_Invoke() requests it in the VM that
 * uses the emulated SB (the VM of the last trapped port access), VPICD
 * delivers it when that VM runs with interrupts enabled.  The VM's EOI
 * clears the request.  Unlike VSBHDA, the program's interrupt handler
 * doesn't run synchronously inside VIRQ_Invoke(). */

#include <stdint.h>
#include <stdbool.h>

#include "CONFIG.H"
#include "VIRQ.H"
#include "VSB.H"
#include "VXDLIB.H"

unsigned vsb_owner_vm;    /* VM using the emulated SB */
static unsigned sbirqh;   /* VPICD handle of the SB IRQ */
static unsigned irqvm;    /* VM the IRQ was requested in */

int virq_install( int irq )
{
	if ( !( sbirqh = vxd_virtualize_irq( irq, 0 ) ) ) {
		vlog( "VSBVXD: VPICD_Virtualize_IRQ(%d) for the SB IRQ failed\n", irq );
		return -1;
	}
	return 0;
}

void VIRQ_Invoke( void )
{
	if ( sbirqh && vsb_owner_vm ) {
		irqvm = vsb_owner_vm;
		vxd_set_int_request( sbirqh, irqvm );
	}
}

/* virtual EOI of the SB IRQ by a VM */
void vsb_virt_eoi( unsigned irqh, unsigned vm )
{
	vxd_clear_int_request( irqh, vm );
}

/* DSP commands F2/F3 (trigger IRQ): VSBHDA waits for the next sound
 * hardware interrupt; here the request can be made at once */
void VIRQ_WaitForSndIrq( void )
{
	if ( VSB_GetIRQStatus() )
		VIRQ_Invoke();
}

void VIRQ_Check( void ) { }
void VPIC_Init( uint8_t hwirq ) { (void)hwirq; }
