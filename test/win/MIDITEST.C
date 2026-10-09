/* MIDITEST.C: test program for the MIDI and aux devices of VSBHDA.DRV
 * (Windows 3.1): lists the devices, plays A4 (440 Hz) for 1 s with the GM
 * program "Lead 1 (square)", a percussion note, tries the MIDI mapper,
 * plays the MIDI file given on the command line with MCI, sets the master
 * volume, writes the results to C:\VSB\OUT.TXT and ends
 * Windows. */

#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>

static HFILE out;
static char buf[200];

static void say( void )
{
	_lwrite( out, buf, strlen( buf ) );
}

static void wait( DWORD ms )
{
	DWORD t = timeGetTime();
	while ( timeGetTime() - t < ms );
}

int PASCAL WinMain( HINSTANCE hinst, HINSTANCE hprev, LPSTR cmdline, int show )
{
	MIDIOUTCAPS mc;
	AUXCAPS ac;
	HMIDIOUT h;
	UINT n, i, rc;

	out = _lcreat( "C:\\VSB\\OUT.TXT", 0 );
	if ( !_fstrncmp( cmdline, "/install", 8 ) ) {
		/* like Control Panel/Drivers: DRV_INSTALL (sets up MIDIMAP.CFG) */
		HDRVR hd = OpenDriver( "vsbhda.drv", NULL, 0 );
		LRESULT r = hd ? SendDriverMessage( hd, DRV_INSTALL, 0, 0 ) : -1;
		sprintf( buf, "DRV_INSTALL: driver %u result %ld\r\n", (UINT)hd, (long)r ); say();
		if ( hd )
			CloseDriver( hd, 0, 0 );
		cmdline += 8;
		while ( *cmdline == ' ' )
			cmdline++;
	}
	n = midiOutGetNumDevs();
	sprintf( buf, "midi out devices: %u\r\n", n ); say();
	for ( i = 0; i < n; i++ )
		if ( midiOutGetDevCaps( i, &mc, sizeof(mc) ) == 0 ) {
			sprintf( buf, "  %u: %s tech %u voices %u\r\n", i, mc.szPname, mc.wTechnology, mc.wVoices ); say();
		}
	n = auxGetNumDevs();
	sprintf( buf, "aux devices: %u\r\n", n ); say();
	for ( i = 0; i < n; i++ )
		if ( auxGetDevCaps( i, &ac, sizeof(ac) ) == 0 ) {
			sprintf( buf, "  %u: %s\r\n", i, ac.szPname ); say();
		}
	rc = midiOutOpen( &h, 0, 0, 0, 0 );
	sprintf( buf, "midiOutOpen(0) = %u\r\n", rc ); say();
	if ( !rc ) {
		midiOutShortMsg( h, 0x0050C0 );     /* program 80: lead 1 (square) */
		midiOutShortMsg( h, 0x7F4590 );     /* note on A4 */
		wait( 1000 );
		midiOutShortMsg( h, 0x004580 );     /* note off */
		wait( 300 );
		midiOutShortMsg( h, 0x7F2699 );     /* percussion: snare (38) */
		wait( 500 );
		midiOutReset( h );
		midiOutClose( h );
	}
	rc = midiOutOpen( &h, MIDI_MAPPER, 0, 0, 0 );
	sprintf( buf, "midiOutOpen(MIDI_MAPPER) = %u\r\n", rc ); say();
	if ( !rc )
		midiOutClose( h );
	if ( *cmdline ) {
		char cmd[160];
		DWORD t = timeGetTime();
		sprintf( cmd, "play %s wait", (char FAR *)cmdline );
		rc = (UINT)mciSendString( cmd, NULL, 0, NULL );
		sprintf( buf, "mciSendString(%s) = %u, %lu ms\r\n", cmd, rc, timeGetTime() - t ); say();
		mciSendString( "close all", NULL, 0, NULL );
	}
	rc = auxSetVolume( 0, 0x80008000 );
	sprintf( buf, "auxSetVolume(0) = %u\r\n", rc ); say();
	_lclose( out );
	ExitWindows( 0, 0 );
	return 0;
}
