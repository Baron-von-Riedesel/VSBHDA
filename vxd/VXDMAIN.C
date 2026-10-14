/* VXDMAIN.C: VSBVXD - Sound Blaster emulation for Windows 3.1 enhanced
 * mode, built on the VSBHDA emulation and sound hardware drivers.
 * System control handlers (called from VSBVXD.ASM). */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "CONFIG.H"
#include "AU.H"
#include "VSB.H"
#include "VOPL3.H"
#include "VMPU.H"
#include "VIRQ.H"
#include "VXDLIB.H"

void pci_list_audio( void );
int  heap_init( unsigned size );
int  physpool_init( unsigned size );
int  ptrap_install( void );
int  virq_install( int irq );
bool SNDISR_Init( void *hAU, uint16_t vol );
int  SNDISR_Interrupt( void );

/* settings, [VSBVXD] in SYSTEM.INI; see VSBHDA's command line options */
struct globalvars gvars = {
	0x220, 5, 1,         /* Base, IRQ, DMA */
#if SB16
	5,                   /* HDMA */
#endif
	6,                   /* Type: SB16 */
#if VMPU
	0,                   /* MPU */
#endif
	1, 1, 1, 9, 16,      /* OPL3, rm, pm, Volume, buffer size (4 kB pages) */
#if SLOWDOWN
	0,
#endif
#if SOUNDFONT
	NULL, VOICES_DEFAULT,
#endif
	0,                   /* compatibility flags */
	44100,               /* Freq (Windows wave output) */
};

static struct {
	void *hAU;
	unsigned hwirqh;     /* VPICD handle of the sound hardware IRQ */
	unsigned testms;     /* test tone: ms to play, 0 = off */
	int testlevel;       /* test tone amplitude */
	unsigned phase;      /* test tone generator */
	unsigned samples;
} vsb;

static unsigned profile_hex( const char *key, unsigned def )
{
	const char *s = vxd_profile_str( key );
	unsigned v = 0;
	if ( !s || !*s )
		return def;
	for ( ; *s; s++ ) {
		if ( *s >= '0' && *s <= '9' ) v = v * 16 + *s - '0';
		else if ( (*s | 0x20) >= 'a' && (*s | 0x20) <= 'f' ) v = v * 16 + (*s | 0x20) - 'a' + 10;
		else break;
	}
	return v;
}

int vsb_sys_critical_init( uint32_t refdata )
{
	vlog_init( profile_hex( "LogPort", 0 ) );
	vlog( "VSBVXD: Sys_Critical_Init\n" );
	(void)refdata;
	return 0;
}

int vsb_device_init( void )
{
	int irq;

	vlog( "VSBVXD: Device_Init\n" );
	gvars.base  = profile_hex( "Base", gvars.base );
	gvars.irq   = vxd_profile_int( "IRQ", gvars.irq );
	gvars.dma   = vxd_profile_int( "DMA", gvars.dma );
	gvars.hdma  = vxd_profile_int( "HDMA", gvars.hdma );
	gvars.type  = vxd_profile_int( "Type", gvars.type );
	gvars.vol   = vxd_profile_int( "Volume", gvars.vol );
	gvars.freq  = vxd_profile_int( "Freq", gvars.freq );
	gvars.opl3  = vxd_profile_int( "OPL3", gvars.opl3 );
	gvars.device = vxd_profile_int( "Device", 0 );
	gvars.pin   = vxd_profile_int( "Pin", 0 );
	gvars.buffers = vxd_profile_int( "Buffers", 0 );
	gvars.period_size = vxd_profile_int( "PeriodSize", 0 );
	gvars.compatflags = vxd_profile_int( "Compat", 0 );
	vsb.testms  = vxd_profile_int( "TestTone", 0 );
	vsb.testlevel = vxd_profile_int( "TestLevel", 8000 );

	pci_list_audio();
	/* memory: heap and physically contiguous memory for the sound hardware
	 * (allocated during Sys_Critical_Init, the VMM later placed a ring 0
	 * stack into the same linear pages) */
	if ( heap_init( 512 * 1024 ) || physpool_init( 128 * 1024 ) ) {
		vlog( "VSBVXD: out of memory\n" );
		return 1;
	}
	if ( !( vsb.hAU = AU_init( &gvars ) ) ) {
		vlog( "VSBVXD: no supported sound hardware found\n" );
		return 1;
	}
	irq = AU_getirq( vsb.hAU );
	vlog( "VSBVXD: sound card %s, IRQ %d\n", AU_getshortname( vsb.hAU ), irq );
	if ( irq <= 0 || irq > 15 || irq == gvars.irq ) {
		vlog( "VSBVXD: invalid sound card IRQ\n" );
		return 1;
	}
	AU_setmixer_init( vsb.hAU );
	AU_setmixer_outs( vsb.hAU, MIXER_SETMODE_ABSOLUTE, gvars.vol * 100 / 9 );
	gvars.freq = AU_setrate( vsb.hAU, gvars.freq, HW_CHANNELS, HW_BITS );
	vlog( "VSBVXD: output %u Hz\n", gvars.freq );

	/* the emulation (see VSBHDA's main()) */
	VPIC_Init( irq );
	if ( gvars.type < 6 )
		gvars.hdma = -1;
	VSB_Init( gvars.base, gvars.irq, gvars.dma, gvars.hdma, gvars.type, vsb.hAU );
#ifndef NOFM
	if ( gvars.opl3 ) {
		/* the OPL tables are computed with the FPU */
		static uint8_t fpubuf[108];
		unsigned cr0 = vxd_fpu_begin( fpubuf );
		VOPL3_Init( gvars.freq );
		vxd_fpu_end( fpubuf, cr0 );
	}
#endif
	if ( !SNDISR_Init( vsb.hAU, gvars.vol * 256 / 9 ) ) {
		vlog( "VSBVXD: no memory for the PCM buffer\n" );
		return 1;
	}
#if VMPU
	VMPU_Init( gvars.freq );
#endif
	if ( virq_install( gvars.irq ) || ptrap_install() )
		return 1;
	vlog( "VSBVXD: SB emulation A%x I%d D%d H%d T%d%s\n", gvars.base, gvars.irq, gvars.dma,
		 gvars.hdma, gvars.type, gvars.opl3 ? ", OPL3 at 388h" : "" );

	if ( !( vsb.hwirqh = vxd_virtualize_irq( irq, 1 ) ) ) {
		vlog( "VSBVXD: VPICD_Virtualize_IRQ(%d) failed\n", irq );
		return 1;
	}
	return 0;
}

/* output sample rate, 0 if there's no sound hardware (VXDWAVE.C) */
int vsb_hwfreq( void )
{
	return vsb.hAU ? gvars.freq : 0;
}

/* master volume of the sound card in percent (VXDWAVE.C) */
void vsb_setmaster( int percent )
{
	if ( vsb.hAU )
		AU_setmixer_outs( vsb.hAU, MIXER_SETMODE_ABSOLUTE, percent );
}

/* called by VSB.C on DSP reset (REINITOPL) */
void MAIN_ReinitOPL( void )
{
#ifndef NOFM
	if ( gvars.opl3 )
		VOPL3_Reinit( AU_getfreq( vsb.hAU ) );
#endif
}

/* DSP command used by VSBHDA's UNINST.EXE: not for the VxD */
void MAIN_Uninstall( void )
{
}

void fatal_error( int nError )
{
	vlog( "VSBVXD: fatal error %d\n", nError );
}

int vsb_init_complete( void )
{
	vlog( "VSBVXD: Init_Complete\n" );
	if ( vsb.hAU ) {
		vxd_phys_unmask( vsb.hwirqh );
		AU_start( vsb.hAU );
		vlog( "VSBVXD: sound hardware started\n" );
	}
	return 0;
}

int vsb_system_exit( void )
{
	vlog( "VSBVXD: System_Exit\n" );
	if ( vsb.hAU ) {
		AU_stop( vsb.hAU );
		AU_close( vsb.hAU );
	}
	return 0;
}

int vsb_destroy_vm( unsigned vm )
{
	(void)vm;
	return 0;
}

/* test tone (TestTone=ms): a 440 Hz square wave instead of the emulation,
 * to check the sound hardware */
static int testtone( void )
{
	static int16_t buf[2 * 4096];
	unsigned n, i;

	if ( !AU_isirq( vsb.hAU ) )
		return 0;
	n = AU_cardbuf_space( vsb.hAU ) / 4;
	if ( n > 4096 )
		n = 4096;
	for ( i = 0; i < n; i++ ) {
		int16_t v = 0;
		/* test tone: 440 Hz square wave, quarter amplitude */
		if ( vsb.samples < (uint32_t)vsb.testms * (gvars.freq / 1000) ) {
			vsb.phase += 440 * 2;
			if ( vsb.phase >= (unsigned)gvars.freq * 2 )
				vsb.phase -= gvars.freq * 2;
			v = vsb.phase < (unsigned)gvars.freq ? vsb.testlevel : -vsb.testlevel;
			vsb.samples++;
		}
		buf[2 * i] = buf[2 * i + 1] = v;
	}
	if ( n )
		AU_writedata( vsb.hAU, buf, n * 2 ); /* 16-bit values */
	if ( vsb.samples >= (uint32_t)vsb.testms * (gvars.freq / 1000) )
		vsb.testms = 0;
	return 1;
}

/* sound hardware interrupt; returns 0 if it was ours */
int vsb_hwint( unsigned irqh )
{
	int rc = vsb.testms ? testtone() : SNDISR_Interrupt();
	
	if ( !rc )
		return 1;
	
	vxd_phys_eoi( irqh );
	return 0;
}

/* global time-out (vxd_start_timer): returns the ms until the next call */
unsigned vsb_timer( void )
{
	return 0;
}

/* other system control messages */
void vsb_control( unsigned msg, unsigned vm )
{
	(void)msg; (void)vm;
}
