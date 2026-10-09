/* MIDIMAP.C: set up the MIDI Mapper of Windows 3.1 for VSBHDA.DRV (called
 * on DRV_INSTALL, like the drivers of the sound card makers did).
 * MIDIMAP.CFG (Windows SYSTEM directory) as written by the Windows 3.1
 * MIDI Mapper:
 *   header (12h bytes): word 0: version (1), word 6: id of the active
 *     setup; followed by the setup table at 48h: entries of 54 bytes:
 *     name (16), description (32), id, offset of the channel map, 0
 *   channel map: 16 entries of 40 bytes: destination channel (word),
 *     port name (32), patchmap id (word, 0 = none), active (word), 0
 * The General MIDI setup "Ad Lib general" (made for Microsoft's Ad Lib
 * driver) is changed to play all 16 channels unchanged on our FM device and
 * made the active setup; the original file is kept as MIDIMAP.BAK. */

#include <string.h>
#include "VSBDRV.H"

#define SETUP_TABLE  0x48
#define SETUP_SIZE   54
#define CHAN_SIZE    40

extern const char midi_port_name[];

static WORD getw( const char huge *p ) { return (BYTE)p[0] | ( (BYTE)p[1] << 8 ); }
static void putw( char huge *p, WORD w ) { p[0] = (char)w; p[1] = (char)( w >> 8 ); }

int midimap_setup( void )
{
	char path[144], bak[144];
	OFSTRUCT of;
	HFILE f;
	HGLOBAL hmem;
	char huge *d;
	long size, e, map;
	int k, rc = -1;

	GetSystemDirectory( path, sizeof(path) - 16 );
	lstrcpy( bak, path );
	lstrcat( path, "\\MIDIMAP.CFG" );
	lstrcat( bak, "\\MIDIMAP.BAK" );
	if ( ( f = _lopen( path, READ ) ) == HFILE_ERROR )
		return -1;
	size = _llseek( f, 0, 2 );
	_llseek( f, 0, 0 );
	if ( size < SETUP_TABLE || !( hmem = GlobalAlloc( GMEM_MOVEABLE, size ) ) ) {
		_lclose( f );
		return -1;
	}
	d = (char huge *)GlobalLock( hmem );
	_hread( f, d, size );
	_lclose( f );

	if ( getw( d ) == 1 ) {
		for ( e = SETUP_TABLE; e + SETUP_SIZE <= size && d[e]; e += SETUP_SIZE ) {
			if ( lstrcmpi( (LPCSTR)( d + e ), "Ad Lib general" ) )
				continue;
			map = getw( d + e + 50 );
			if ( map + 16 * CHAN_SIZE > size )
				break;
			for ( k = 0; k < 16; k++ ) {
				char huge *c = d + map + (long)k * CHAN_SIZE;
				int i;
				putw( c, k );                       /* channel unchanged */
				for ( i = 0; i < 32; i++ )
					c[2 + i] = 0;
				lstrcpy( (LPSTR)( c + 2 ), midi_port_name );
				putw( c + 34, 0 );                  /* no patchmap */
				putw( c + 36, 1 );                  /* active */
			}
			putw( d + 6, getw( d + e + 48 ) );      /* active setup */
			/* keep the original */
			if ( OpenFile( bak, &of, OF_EXIST ) == HFILE_ERROR )
				if ( ( f = _lcreat( bak, 0 ) ) != HFILE_ERROR ) {
					HFILE o = _lopen( path, READ );
					char huge *b = (char huge *)GlobalLock( GlobalAlloc( GMEM_MOVEABLE, size ) );
					if ( b && o != HFILE_ERROR ) {
						_hread( o, b, size );
						_hwrite( f, b, size );
					}
					if ( o != HFILE_ERROR )
						_lclose( o );
					_lclose( f );
					if ( b )
						GlobalFree( GlobalHandle( SELECTOROF( b ) ) );
				}
			if ( ( f = _lcreat( path, 0 ) ) != HFILE_ERROR ) {
				if ( _hwrite( f, d, size ) == size )
					rc = 0;
				_lclose( f );
			}
			break;
		}
	}
	GlobalUnlock( hmem );
	GlobalFree( hmem );
	return rc;
}
