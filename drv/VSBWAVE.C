/* VSBWAVE.C: Windows 3.1 wave output driver (MMSYSTEM) for VSBVXD.386.
 * The sound card is driven by the VxD (any card supported by VSBHDA); this
 * driver passes the wave buffers of the applications to the VxD, which
 * mixes them into its output.  A multimedia timer collects the finished
 * buffers and returns them to the application (WOM_DONE).
 *
 * Install: SYSTEM.INI [drivers] wave=vsbwave.drv, and VSBVXD.386 in
 * [386Enh] (Windows enhanced mode only). */

#include <windows.h>
#include <mmsystem.h>

/* --- from the DDK (mmddk.h) ------------------------------------------ */
#define WODM_GETNUMDEVS     3
#define WODM_GETDEVCAPS     4
#define WODM_OPEN           5
#define WODM_CLOSE          6
#define WODM_PREPARE        7
#define WODM_UNPREPARE      8
#define WODM_WRITE          9
#define WODM_PAUSE          10
#define WODM_RESTART        11
#define WODM_RESET          12
#define WODM_GETPOS         13
#define WODM_GETPITCH       14
#define WODM_SETPITCH       15
#define WODM_GETVOLUME      16
#define WODM_SETVOLUME      17
#define WODM_GETPLAYBACKRATE 18
#define WODM_SETPLAYBACKRATE 19
#define WODM_BREAKLOOP      20

#define DCB_TYPEMASK        0x0007

typedef struct {
	HWAVE hWave;
	const WAVEFORMAT FAR *lpFormat;
	DWORD dwCallback;
	DWORD dwInstance;
} WAVEOPENDESC, FAR *LPWAVEOPENDESC;

BOOL WINAPI DriverCallback( DWORD dwCallback, UINT uFlags, HANDLE hDevice, UINT uMessage,
							DWORD dwUser, DWORD dwParam1, DWORD dwParam2 );
DWORD WINAPI GetSelectorBase( UINT uSelector );

/* --- VSBVXD API (VXDCALL.ASM, vxd/VXDWAVE.C) -------------------------- */
struct vxdregs { DWORD eax, ebx, ecx, edx, esi, edi; };
extern int __cdecl vxd_init( void );
extern int __cdecl vxd_call( struct vxdregs far * );

#define VW_GETVERSION 0
#define VW_OPEN       1
#define VW_CLOSE      2
#define VW_WRITE      3
#define VW_GETDONE    4
#define VW_RESET      5
#define VW_PAUSE      6
#define VW_RESTART    7
#define VW_GETPOS     8
#define VW_SETVOLUME  9

/* --- driver state ------------------------------------------------------ */
static BOOL vxdok;            /* VSBVXD.386 is installed */
static DWORD hwrate;          /* output sample rate of the VxD */
static struct {
	BOOL open;
	WAVEOPENDESC desc;
	UINT cbflags;             /* callback type (DCB_xxx) */
	PCMWAVEFORMAT fmt;
	UINT timer;
	int queued;               /* buffers given to the VxD */
} wo;
static DWORD volume = 0xFFFFFFFF;

static DWORD vcall( DWORD fn, DWORD ebx, DWORD ecx, DWORD edx, DWORD esi, DWORD FAR *out )
{
	struct vxdregs r;
	r.eax = fn; r.ebx = ebx; r.ecx = ecx; r.edx = edx; r.esi = esi; r.edi = 0;
	if ( vxd_call( &r ) )
		return (DWORD)-1;
	if ( out ) {
		out[0] = r.eax; out[1] = r.ebx; out[2] = r.edx;
	}
	return 0;
}

static void callback( UINT msg, DWORD p1 )
{
	DriverCallback( wo.desc.dwCallback, wo.cbflags, (HANDLE)wo.desc.hWave, msg,
					wo.desc.dwInstance, p1, 0 );
}

/* return the buffers the VxD has finished; called by the timer
 * (interrupt time) and by WODM_RESET */
static void collect( void )
{
	DWORD out[3];
	while ( wo.open && vcall( VW_GETDONE, 0, 0, 0, 0, out ) == 0 ) {
		LPWAVEHDR h = (LPWAVEHDR)out[2];
		h->dwFlags &= ~WHDR_INQUEUE;
		h->dwFlags |= WHDR_DONE;
		wo.queued--;
		callback( WOM_DONE, (DWORD)h );
	}
}

void CALLBACK __loadds timer_cb( UINT id, UINT msg, DWORD user, DWORD d1, DWORD d2 )
{
	collect();
}

static BOOL format_ok( const WAVEFORMAT FAR *f )
{
	const PCMWAVEFORMAT FAR *p = (const PCMWAVEFORMAT FAR *)f;
	return f->wFormatTag == WAVE_FORMAT_PCM &&
		( f->nChannels == 1 || f->nChannels == 2 ) &&
		( p->wBitsPerSample == 8 || p->wBitsPerSample == 16 ) &&
		f->nSamplesPerSec >= 4000 && f->nSamplesPerSec <= 48000 &&
		f->nBlockAlign == f->nChannels * p->wBitsPerSample / 8;
}

static DWORD wod_open( LPWAVEOPENDESC d, DWORD flags )
{
	const PCMWAVEFORMAT FAR *p = (const PCMWAVEFORMAT FAR *)d->lpFormat;
	if ( !format_ok( d->lpFormat ) )
		return WAVERR_BADFORMAT;
	if ( flags & WAVE_FORMAT_QUERY )
		return MMSYSERR_NOERROR;
	if ( wo.open )
		return MMSYSERR_ALLOCATED;
	if ( vcall( VW_OPEN, p->wf.nSamplesPerSec, p->wf.nChannels, p->wBitsPerSample, 0, 0 ) )
		return MMSYSERR_ERROR;
	wo.desc = *d;
	wo.fmt = *p;
	wo.cbflags = HIWORD( flags );
	wo.queued = 0;
	vcall( VW_SETVOLUME, volume, 0, 0, 0, 0 );
	wo.timer = timeSetEvent( 10, 5, timer_cb, 0, TIME_PERIODIC );
	if ( !wo.timer ) {
		vcall( VW_CLOSE, 0, 0, 0, 0, 0 );
		return MMSYSERR_NOMEM;
	}
	wo.open = TRUE;
	callback( WOM_OPEN, 0 );
	return MMSYSERR_NOERROR;
}

static DWORD wod_close( void )
{
	if ( wo.queued > 0 )
		return WAVERR_STILLPLAYING;
	timeKillEvent( wo.timer );
	vcall( VW_CLOSE, 0, 0, 0, 0, 0 );
	wo.open = FALSE;
	callback( WOM_CLOSE, 0 );
	return MMSYSERR_NOERROR;
}

static DWORD wod_write( LPWAVEHDR h )
{
	DWORD lin;
	if ( !( h->dwFlags & WHDR_PREPARED ) )
		return WAVERR_UNPREPARED;
	if ( h->dwFlags & WHDR_INQUEUE )
		return WAVERR_STILLPLAYING;
	lin = GetSelectorBase( SELECTOROF( h->lpData ) ) + OFFSETOF( h->lpData );
	h->dwFlags &= ~WHDR_DONE;
	h->dwFlags |= WHDR_INQUEUE;
	if ( vcall( VW_WRITE, 0, h->dwBufferLength, (DWORD)h, lin, 0 ) ) {
		h->dwFlags &= ~WHDR_INQUEUE;
		return MMSYSERR_NOMEM;
	}
	wo.queued++;
	return MMSYSERR_NOERROR;
}

static DWORD wod_getpos( LPMMTIME t, UINT size )
{
	DWORD out[3], bytes;
	if ( size < sizeof(MMTIME) )
		return MMSYSERR_ERROR;
	vcall( VW_GETPOS, 0, 0, 0, 0, out );
	bytes = out[0];
	if ( t->wType == TIME_SAMPLES )
		t->u.sample = bytes / wo.fmt.wf.nBlockAlign;
	else {
		t->wType = TIME_BYTES;
		t->u.cb = bytes;
	}
	return MMSYSERR_NOERROR;
}

static DWORD wod_getdevcaps( LPWAVEOUTCAPS c, UINT size )
{
	WAVEOUTCAPS caps;
	static const char name[] = "VSBHDA Wave Out (VSBVXD)";
	UINT i;
	caps.wMid = 0;
	caps.wPid = 0;
	caps.vDriverVersion = 0x0100;
	for ( i = 0; i < sizeof(name); i++ )
		caps.szPname[i] = name[i];
	caps.dwFormats = WAVE_FORMAT_1M08 | WAVE_FORMAT_1S08 | WAVE_FORMAT_1M16 | WAVE_FORMAT_1S16 |
		WAVE_FORMAT_2M08 | WAVE_FORMAT_2S08 | WAVE_FORMAT_2M16 | WAVE_FORMAT_2S16 |
		WAVE_FORMAT_4M08 | WAVE_FORMAT_4S08 | WAVE_FORMAT_4M16 | WAVE_FORMAT_4S16;
	caps.wChannels = 2;
	caps.dwSupport = WAVECAPS_VOLUME | WAVECAPS_LRVOLUME;
	if ( size > sizeof(caps) )
		size = sizeof(caps);
	for ( i = 0; i < size; i++ )
		((char FAR *)c)[i] = ((char *)&caps)[i];
	return MMSYSERR_NOERROR;
}

DWORD FAR PASCAL __export __loadds wodMessage( UINT id, UINT msg, DWORD user, DWORD p1, DWORD p2 )
{
	if ( id != 0 && msg != WODM_GETNUMDEVS )
		return MMSYSERR_BADDEVICEID;
	switch ( msg ) {
	case WODM_GETNUMDEVS:
		return vxdok ? 1 : 0;
	case WODM_GETDEVCAPS:
		return wod_getdevcaps( (LPWAVEOUTCAPS)p1, (UINT)p2 );
	case WODM_OPEN:
		return wod_open( (LPWAVEOPENDESC)p1, p2 );
	case WODM_CLOSE:
		return wod_close();
	case WODM_WRITE:
		return wod_write( (LPWAVEHDR)p1 );
	case WODM_PAUSE:
		vcall( VW_PAUSE, 0, 0, 0, 0, 0 );
		return MMSYSERR_NOERROR;
	case WODM_RESTART:
		vcall( VW_RESTART, 0, 0, 0, 0, 0 );
		return MMSYSERR_NOERROR;
	case WODM_RESET:
		vcall( VW_RESET, 0, 0, 0, 0, 0 );
		collect();
		return MMSYSERR_NOERROR;
	case WODM_BREAKLOOP:
		return MMSYSERR_NOERROR;
	case WODM_GETPOS:
		return wod_getpos( (LPMMTIME)p1, (UINT)p2 );
	case WODM_GETVOLUME:
		*(DWORD FAR *)p1 = volume;
		return MMSYSERR_NOERROR;
	case WODM_SETVOLUME:
		volume = p1;
		if ( wo.open )
			vcall( VW_SETVOLUME, volume, 0, 0, 0, 0 );
		return MMSYSERR_NOERROR;
	case WODM_PREPARE:   /* MMSYSTEM prepares (page locks) the buffers */
	case WODM_UNPREPARE:
	default:
		return MMSYSERR_NOTSUPPORTED;
	}
}

/* installable driver entry */
LRESULT FAR PASCAL __export __loadds DriverProc( DWORD id, HDRVR hdrv, UINT msg, LPARAM p1, LPARAM p2 )
{
	switch ( msg ) {
	case DRV_LOAD:
		vxdok = vxd_init() == 0;
		if ( vxdok ) {
			DWORD out[3];
			vcall( VW_GETVERSION, 0, 0, 0, 0, out );
			hwrate = out[1];
		}
		return 1;
	case DRV_FREE:
	case DRV_ENABLE:
	case DRV_DISABLE:
	case DRV_OPEN:
	case DRV_CLOSE:
		return 1;
	case DRV_INSTALL:
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
