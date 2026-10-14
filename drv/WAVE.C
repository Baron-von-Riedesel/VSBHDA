/* WAVE.C: wave output of VSBHDA.DRV.  The wave buffers of the applications
 * are passed to VSBVXD.386, which mixes them into its output; a multimedia
 * timer collects the finished buffers and returns them to the application
 * (WOM_DONE). */

#include "VSBDRV.H"

/* --- driver state ------------------------------------------------------ */
static struct {
	BOOL open;
	WAVEOPENDESC desc;
	UINT cbflags;             /* callback type (DCB_xxx) */
	PCMWAVEFORMAT fmt;
	UINT timer;
	int queued;               /* buffers given to the VxD */
} wo;
static DWORD volume = 0xFFFFFFFF;

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
	UINT i;
	caps.wMid = 0;
	caps.wPid = 0;
	caps.vDriverVersion = 0x0100;
	copyname( caps.szPname, "VSBHDA Wave" );
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
