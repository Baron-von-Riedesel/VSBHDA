/* PLATVXD.C: platform layer of VSBVXD for the VSBHDA sound hardware
 * drivers and emulation: memory allocation, physical memory for the
 * sound hardware buffers, MMIO mapping, delays (replaces PHYSMEM.C,
 * TIMER.C, the DPMI functions and the C library heap). */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>

#include "VXDLIB.H"
#include "DPMI.H"
#include "PHYSMEM.H"

/* --- interrupt-safe heap ------------------------------------------
 * The emulation allocates in the sound interrupt (SNDISR.C cv_rate),
 * where _HeapAllocate can't be called: first-fit allocator with
 * coalescing on a pool allocated once during initialization. */

struct blk {
	uint32_t size;      /* size of the block incl. header, bit 0 = used */
	uint32_t pad[3];    /* 16-byte alignment of the user data */
};

static struct blk *pool;
static uint32_t poolsize;

static uint32_t irqsave( void );
#pragma aux irqsave = "pushfd" "pop eax" "cli" value [eax]
static void irqrestore( uint32_t );
#pragma aux irqrestore = "push eax" "popfd" parm [eax]

int heap_init( unsigned size )
{
	pool = vxd_page_alloc( (size + 4095) >> 12 );
	if ( !pool )
		return -1;
	poolsize = (size + 4095) & ~4095;
	pool->size = poolsize;
	return 0;
}

void *malloc( size_t n )
{
	struct blk *b, *end;
	uint32_t need = ( n + sizeof(struct blk) + 15 ) & ~15;
	uint32_t fl = irqsave();

	end = (struct blk *)((char *)pool + poolsize);
	for ( b = pool; b < end; b = (struct blk *)((char *)b + (b->size & ~1)) ) {
		if ( b->size & 1 )
			continue;
		/* merge following free blocks */
		for ( ;; ) {
			struct blk *n2 = (struct blk *)((char *)b + b->size);
			if ( n2 >= end || (n2->size & 1) )
				break;
			b->size += n2->size;
		}
		if ( b->size >= need ) {
			if ( b->size - need >= 64 ) {
				struct blk *rest = (struct blk *)((char *)b + need);
				rest->size = b->size - need;
				b->size = need;
			}
			b->size |= 1;
			irqrestore( fl );
			return b + 1;
		}
	}
	irqrestore( fl );
	vlog( "VSBVXD: malloc(%u) failed\n", n );
	return NULL;
}

void free( void *p )
{
	if ( p ) {
		uint32_t fl = irqsave();
		((struct blk *)p - 1)->size &= ~1;
		irqrestore( fl );
	}
}

void *calloc( size_t n, size_t m )
{
	void *p = malloc( n * m );
	if ( p )
		memset( p, 0, n * m );
	return p;
}

void *realloc( void *p, size_t n )
{
	void *q;
	size_t old;
	if ( !p )
		return malloc( n );
	old = (((struct blk *)p - 1)->size & ~1) - sizeof(struct blk);
	if ( old >= n )
		return p;
	if ( q = malloc( n ) ) {
		memcpy( q, p, old );
		free( p );
	}
	return q;
}

/* --- physical memory for the sound hardware (PHYSMEM.C) ----------
 * One block of fixed pages, allocated during Sys_Critical_Init and handed
 * out by _alloc_physical_memory().  It must be physically contiguous, but
 * _PageAllocate with PageUseAlign can't be used: Windows 3.11 hands these
 * pages to protected mode programs of the system VM as well (DPMI memory,
 * the KRNL386 heap); the sound hardware then plays Windows' data.  Normal
 * fixed pages are contiguous this early, which is checked. */

static uint8_t *physpool;
static uint32_t physpool_phys, physpool_size, physpool_used;

static int contiguous( uint8_t *lin, unsigned size )
{
	uint32_t phys = vxd_lin_to_phys( lin );
	unsigned i;
	for ( i = 4096; i < size; i += 4096 )
		if ( vxd_lin_to_phys( lin + i ) != phys + i )
			return 0;
	return 1;
}

int physpool_init( unsigned size )
{
	int tries;
	size = (size + 4095) & ~4095;
	for ( tries = 0; tries < 8; tries++ ) {
		/* a block that isn't contiguous stays allocated */
		if ( !( physpool = vxd_page_alloc( size >> 12 ) ) )
			return -1;
		if ( contiguous( physpool, size ) )
			break;
		vlog( "VSBVXD: memory at %x not physically contiguous\n", physpool );
	}
	if ( tries == 8 )
		return -1;
	physpool_phys = vxd_lin_to_phys( physpool );
	physpool_size = (size + 4095) & ~4095;
	vlog( "VSBVXD: physical pool %u bytes: lin %x phys %x\n", physpool_size, physpool, physpool_phys );
	return 0;
}

int _alloc_physical_memory( struct xmsmem_s *xms, uint32_t size )
{
	uint32_t phys;
	void *lin;
	size = (size + 4095) & ~4095;
	if ( physpool_used + size > physpool_size ) {
		vlog( "VSBVXD: physical pool exhausted (%u bytes requested)\n", size );
		return 0;
	}
	lin = physpool + physpool_used;
	phys = physpool_phys + physpool_used;
	physpool_used += size;
	xms->pMem = lin;
	xms->physicalptr = phys;
	xms->handle = 1;
	vlog( "VSBVXD: phys mem %u bytes: lin %x phys %x\n", size, lin, phys );
	return 1;
}

void _free_physical_memory( struct xmsmem_s *xms )
{
	/* memory allocated during initialization stays until Windows exits */
	xms->handle = 0;
}

/* --- MMIO mapping (DPMI 0800h/0801h) ------------------------------ */

int __dpmi_physical_address_mapping( __dpmi_meminfo *info )
{
	void *lin = vxd_map_phys( info->address, info->size );
	if ( lin == (void *)-1 || !lin )
		return -1;
	info->address = (unsigned long)lin;
	return 0;
}

int __dpmi_free_physical_address_mapping( __dpmi_meminfo *info )
{
	(void)info;
	return 0;
}

/* --- delay (TIMER.C): PIT channel 0, 1.19318 MHz ------------------ */

static uint16_t pitcount( void )
{
	uint16_t v;
	uint32_t fl = irqsave();
	outp( 0x43, 0x00 );     /* latch counter 0 */
	v = inp( 0x40 );
	v |= inp( 0x40 ) << 8;
	irqrestore( fl );
	return v;
}

void pds_delay_10us( unsigned int ticks )
{
	/* the counter counts down; it may run in mode 2 or 3 (mode 3 counts by 2),
	 * so measure by elapsed counts with wrap-around handling */
	uint16_t old = pitcount(), now;
	uint32_t elapsed = 0, need = ticks * 24; /* mode 3: 2 counts per clock */
	while ( elapsed < need ) {
		now = pitcount();
		elapsed += (uint16_t)(old - now);
		old = now;
	}
}
