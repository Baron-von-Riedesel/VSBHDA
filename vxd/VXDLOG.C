/* VXDLOG.C: debug log of VSBVXD on a serial port (polled, ring 0) */

#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <conio.h>

#include "VXDLIB.H"

static unsigned logport;

void vlog_init( unsigned port )
{
	logport = port;
	if ( !logport || logport == 0xE9 )  /* E9h: Bochs/QEMU debug port */
		return;
	outp( logport + 3, 0x80 );   /* DLAB */
	outp( logport + 0, 0x01 );   /* 115200 baud */
	outp( logport + 1, 0x00 );
	outp( logport + 3, 0x03 );   /* 8N1 */
	outp( logport + 4, 0x03 );   /* DTR, RTS */
}

static void vlog_putc( char c )
{
	int i;
	if ( !logport )
		return;
	if ( logport == 0xE9 ) {
		outp( 0xE9, c );
		return;
	}
	if ( c == '\n' )
		vlog_putc( '\r' );
	for ( i = 0; i < 100000 && !( inp( logport + 5 ) & 0x20 ); i++ );
	outp( logport, c );
}

static void putnum( uint32_t n, unsigned base, int width, char pad, int upper, int neg )
{
	char buf[12];
	int i = 0;
	const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
	do {
		buf[i++] = digits[n % base];
		n /= base;
	} while ( n );
	if ( neg )
		buf[i++] = '-';
	while ( i < width-- )
		vlog_putc( pad );
	while ( i )
		vlog_putc( buf[--i] );
}

/* printf subset: %s %c %d %u %x %X %p, width, '0' padding, 'l' ignored */
void vlog_vprintf( const char *fmt, va_list ap )
{
	for ( ; *fmt; fmt++ ) {
		int width = 0;
		char pad = ' ';
		if ( *fmt != '%' ) {
			vlog_putc( *fmt );
			continue;
		}
		fmt++;
		if ( *fmt == '0' )
			pad = '0';
		while ( *fmt >= '0' && *fmt <= '9' )
			width = width * 10 + *fmt++ - '0';
		while ( *fmt == 'l' || *fmt == 'h' )
			fmt++;
		switch ( *fmt ) {
		case 's': {
			const char *s = va_arg( ap, const char * );
			int len = 0;
			if ( !s )
				s = "(null)";
			while ( s[len] )
				len++;
			while ( len < width-- )
				vlog_putc( ' ' );
			while ( *s )
				vlog_putc( *s++ );
			break;
		}
		case 'c':
			vlog_putc( (char)va_arg( ap, int ) );
			break;
		case 'd': {
			int v = va_arg( ap, int );
			putnum( v < 0 ? -v : v, 10, width, pad, 0, v < 0 );
			break;
		}
		case 'u':
			putnum( va_arg( ap, unsigned ), 10, width, pad, 0, 0 );
			break;
		case 'x':
		case 'p':
			putnum( va_arg( ap, unsigned ), 16, width, pad, 0, 0 );
			break;
		case 'X':
			putnum( va_arg( ap, unsigned ), 16, width, pad, 1, 0 );
			break;
		case 0:
			return;
		default:
			vlog_putc( *fmt );
		}
	}
}

void vlog( const char *fmt, ... )
{
	va_list ap;
	va_start( ap, fmt );
	vlog_vprintf( fmt, ap );
	va_end( ap );
}

/* VSBHDA's debug output and the printf calls of the hardware drivers */
int _dprintf( const char *fmt, ... )
{
	va_list ap;
	va_start( ap, fmt );
	vlog_vprintf( fmt, ap );
	va_end( ap );
	return 0;
}

int printf( const char *fmt, ... )
{
	va_list ap;
	va_start( ap, fmt );
	vlog_vprintf( fmt, ap );
	va_end( ap );
	return 0;
}
