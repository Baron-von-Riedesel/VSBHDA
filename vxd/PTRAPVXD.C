/* PTRAPVXD.C: port trapping of VSBVXD (replaces PTRAP.C): the emulation
 * modules register their port handlers with PTRAP_AddRange() during
 * initialization; ptrap_install() installs a VMM I/O handler for each of
 * these ports, vsb_io() (called by the I/O handler for byte accesses of
 * any VM) dispatches to the emulation. */

#include <stdint.h>
#include <stdbool.h>
#include <conio.h>

#include "CONFIG.H"
#include "PTRAP.H"
#include "VXDLIB.H"

extern struct globalvars gvars;

#define MAXPORTS 64

static struct {
	uint16_t port;
	PORT_TRAP_HANDLER handler;
} traps[MAXPORTS];
static int ntraps;

extern unsigned vsb_owner_vm;   /* VM using the emulated SB (VIRQVXD.C) */

/* ports start + n for each bit n set in portmap */
int PTRAP_AddRange( int start, unsigned int portmap, const PORT_TRAP_HANDLER *handlers )
{
	int i;
	for ( i = 0; portmap; i++, portmap >>= 1 ) {
		if ( !( portmap & 1 ) )
			continue;
		if ( ntraps == MAXPORTS ) {
			vlog( "VSBVXD: too many trapped ports\n" );
			return 0;
		}
		traps[ntraps].port = start + i;
		traps[ntraps].handler = *handlers++;
		ntraps++;
	}
	return 1;
}

int ptrap_install( void )
{
	int i;
	for ( i = 0; i < ntraps; i++ )
		if ( vxd_install_io( traps[i].port ) ) {
			vlog( "VSBVXD: Install_IO_Handler(%x) failed\n", traps[i].port );
			return -1;
		}
	vlog( "VSBVXD: %d ports trapped\n", ntraps );
	return 0;
}


uint32_t vsb_io( unsigned vm, unsigned type, unsigned port, uint32_t data )
{
	int i;
	/* the VM using the SB: only the DSP/mixer ports count (a VM touching
	 * the OPL or MPU ports must not get the SB interrupt) */
	if ( port >= (unsigned)gvars.base && port < (unsigned)gvars.base + 0x10 )
		vsb_owner_vm = vm;
	for ( i = 0; i < ntraps; i++ )
		if ( traps[i].port == port ) {
			uint8_t v = traps[i].handler( port, (uint8_t)data, (type & IOT_OUTPUT) ? TRAPF_OUT | TRAPF_IF : TRAPF_IF );
			return ( type & IOT_OUTPUT ) ? data : v;
		}
	return ( type & IOT_OUTPUT ) ? ( outp( port, data ), data ) : inp( port );
}

void    PTRAP_UntrappedIO_OUT( uint16_t port, uint8_t value ) { outp( port, value ); }
uint8_t PTRAP_UntrappedIO_IN( uint16_t port ) { return inp( port ); }
void    (*UntrappedIO_OUT_Handler)( uint16_t port, uint8_t value ) = PTRAP_UntrappedIO_OUT;
uint8_t (*UntrappedIO_IN_Handler)( uint16_t port ) = PTRAP_UntrappedIO_IN;

void PTRAP_SetPICPortTrap( int on ) { (void)on; }
