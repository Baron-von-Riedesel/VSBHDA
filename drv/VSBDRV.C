/* VSBDRV.C: VSBHDA.DRV - Windows 3.1 multimedia driver for VSBVXD.386.
 * The sound card is driven by the VxD (any card supported by VSBHDA):
 * - wave output (WAVE.C): the wave buffers are mixed into the VxD's output
 * - MIDI output (MIDI.C): FM synthesizer on the VxD's OPL3 emulation
 * - aux (AUX.C): master volume of the sound card
 *
 * Install: SYSTEM.INI [drivers] wave=vsbhda.drv, midi=vsbhda.drv,
 * aux=vsbhda.drv; [386Enh] device=vsbvxd.386 (enhanced mode only). */

#include "VSBDRV.H"

struct vxdregs { DWORD eax, ebx, ecx, edx, esi, edi; };
extern int __cdecl vxd_init( void );
extern int __cdecl vxd_call( struct vxdregs far * );

BOOL vxdok;            /* VSBVXD.386 is installed */

int midimap_setup( void );

DWORD vcall( DWORD fn, DWORD ebx, DWORD ecx, DWORD edx, DWORD esi, DWORD FAR *out )
{
	struct vxdregs r;
	if ( !vxdok )
		return (DWORD)-1;
	r.eax = fn; r.ebx = ebx; r.ecx = ecx; r.edx = edx; r.esi = esi; r.edi = 0;
	if ( vxd_call( &r ) )
		return (DWORD)-1;
	if ( out ) {
		out[0] = r.eax; out[1] = r.ebx; out[2] = r.edx;
	}
	return 0;
}

void copyname( char FAR *dst, const char *src )
{
	while ( ( *dst++ = *src++ ) != 0 );
}

/* installable driver entry */
LRESULT FAR PASCAL __export __loadds DriverProc( DWORD id, HDRVR hdrv, UINT msg, LPARAM p1, LPARAM p2 )
{
	switch ( msg ) {
	case DRV_LOAD:
		vxdok = vxd_init() == 0;
		return 1;
	case DRV_FREE:
	case DRV_ENABLE:
	case DRV_DISABLE:
	case DRV_OPEN:
	case DRV_CLOSE:
		return 1;
	case DRV_INSTALL:
		midimap_setup();    /* MIDI Mapper: General MIDI on our FM device */
		return DRVCNF_RESTART;
	case DRV_REMOVE:
		return DRVCNF_RESTART;
	case DRV_QUERYCONFIGURE:
		return 0;
	default:
		return DefDriverProc( id, hdrv, msg, p1, p2 );
	}
}

int FAR PASCAL LibMain( HINSTANCE hinst, WORD dataseg, WORD heapsize, LPSTR cmdline )
{
	return 1;
}

int FAR PASCAL __export WEP( int param )
{
	return 1;
}
