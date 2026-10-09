/* VXDLIB.H: C interface of VSBVXD.ASM (VMM/VPICD/VDMAD wrappers) and of
 * the VxD platform layer */

#ifndef _VXDLIB_H_
#define _VXDLIB_H_

#include <stdint.h>

/* VM control block */
#define CB_HIGH_LINEAR(vm) (*(uint32_t *)((vm) + 4))

/* I/O trap types */
#define IOT_OUTPUT 4

/* VDMAD virtual state */
struct dmastate {
	uint32_t addr;   /* linear address */
	uint32_t count;
	uint8_t  mode;
	uint8_t  flags;
	uint16_t res;
};
#define DMAST_MASKED    0x01
#define DMAST_REQUESTED 0x02

unsigned    vxd_get_sys_vm( void );
unsigned    vxd_get_cur_vm( void );
unsigned    vxd_get_time( void );
void        vxd_start_timer( unsigned ms );
void       *vxd_heap_alloc( unsigned size );
void        vxd_heap_free( void * );
void       *vxd_page_alloc( unsigned pages );
void       *vxd_map_phys( uint32_t phys, unsigned size );
uint32_t    vxd_lin_to_phys( void *lin );
int         vxd_profile_int( const char *key, int def );
const char *vxd_profile_str( const char *key );
int         vxd_install_io( unsigned port );
unsigned    vxd_virtualize_irq( unsigned irq, int hw );
void        vxd_phys_eoi( unsigned irqh );
void        vxd_phys_unmask( unsigned irqh );
void        vxd_set_int_request( unsigned irqh, unsigned vm );
void        vxd_clear_int_request( unsigned irqh, unsigned vm );
unsigned    vxd_virtualize_dma( unsigned channel );
void        vxd_get_dma_state( unsigned dmah, unsigned vm, struct dmastate * );
void        vxd_set_dma_state( unsigned dmah, unsigned vm, struct dmastate * );
unsigned    vxd_fpu_begin( void *buf108 );
void        vxd_fpu_end( void *buf108, unsigned oldcr0 );

void vlog_init( unsigned port );
void vlog( const char *fmt, ... );

#endif
