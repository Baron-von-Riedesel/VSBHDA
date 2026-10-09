/* PLAYWAV.C: test program for VSBHDA.DRV (Windows 3.1): lists the wave
 * output devices, plays the WAV files given on the command line with
 * sndPlaySound (synchronously), writes the results to C:\VSB\OUT.TXT and
 * ends Windows. */

#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>

static HFILE out;

static void say( const char *s )
{
	_lwrite( out, s, strlen( s ) );
}

int PASCAL WinMain( HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int show )
{
	char buf[200], name[128];
	WAVEOUTCAPS caps;
	UINT n, i;
	DWORD t;
	char *p, *f;

	out = _lcreat( "C:\\VSB\\OUT.TXT", 0 );
	n = waveOutGetNumDevs();
	sprintf( buf, "wave out devices: %u\r\n", n );
	say( buf );
	for ( i = 0; i < n; i++ ) {
		if ( waveOutGetDevCaps( i, &caps, sizeof(caps) ) == 0 ) {
			sprintf( buf, "  %u: %s formats %lx\r\n", i, caps.szPname, caps.dwFormats );
			say( buf );
		}
	}
	_fstrncpy( name, cmdline, sizeof(name) - 1 );
	name[sizeof(name) - 1] = 0;
	for ( f = strtok( name, " " ); f; f = strtok( NULL, " " ) ) {
		t = timeGetTime();
		i = sndPlaySound( f, SND_SYNC );
		sprintf( buf, "sndPlaySound(%s) = %u, %lu ms\r\n", f, i, timeGetTime() - t );
		say( buf );
	}
	_lclose( out );
	ExitWindows( 0, 0 );
	return 0;
}
