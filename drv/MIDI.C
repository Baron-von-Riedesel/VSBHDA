/* MIDI.C: MIDI output of VSBHDA.DRV - a General MIDI synthesizer on the
 * OPL3 (FM) of VSBVXD.386: the OPL3 register writes to ports 388h-38Bh are
 * trapped by the VxD and go to its OPL3 emulation, which is mixed into its
 * output.  18 two-operator voices, instruments from GENMIDI.C (Freedoom's
 * DMX bank; double-voice instruments use two voices), percussion on MIDI
 * channel 10, pitch bend (+/- 2 semitones), volume, expression, pan,
 * sustain pedal. */

#include <conio.h>
#include "VSBDRV.H"

extern const unsigned char genmidi[175][36];
extern const unsigned short fnum_tab[12 * 32];
extern const unsigned char atten_tab[128];

const char midi_port_name[] = "VSBHDA OPL3 FM";   /* also used by MIDIMAP.C */

#define NVOICES 18
#define PERCUSSION 9    /* MIDI channel 10 */

#define GM_FIXED   0x01 /* instrument flags */
#define GM_DOUBLE  0x04

static const unsigned char opoff[9] = { 0, 1, 2, 8, 9, 10, 16, 17, 18 };

static struct voice {
	unsigned char used;
	unsigned char chan;     /* MIDI channel */
	unsigned char note;     /* MIDI note (key) */
	unsigned char inst;     /* GENMIDI instrument */
	unsigned char sub;      /* voice 0/1 of the instrument */
	unsigned char vel;
	unsigned char sustained;/* note off while the pedal is down */
	unsigned char playnote; /* note played (fixed or with offset) */
	unsigned long age;
} voices[NVOICES];

static struct chan {
	unsigned char prog, vol, expr, pan, pedal;
	int bend;               /* -8192..8191 */
} chans[16];

static unsigned long agecnt;
static unsigned char master = 127;   /* MIDI volume, 0-127 */
static DWORD mastervol = 0xFFFFFFFF;

static struct {
	BOOL open;
	MIDIOPENDESC desc;
	UINT cbflags;
	unsigned char status;   /* running status (MODM_LONGDATA) */
	unsigned char data[2];
	int ndata;
} mo;

/* --- OPL3 access -------------------------------------------------------- */

static void opl( int bank, int reg, int val )
{
	unsigned port = bank ? 0x38A : 0x388;
	outp( port, reg );
	outp( port + 1, val );
}

static void opl_reset( void )
{
	int b, r;
	opl( 1, 0x05, 0x01 );   /* OPL3 mode */
	opl( 1, 0x04, 0x00 );   /* no 4-operator voices */
	opl( 0, 0x01, 0x20 );   /* waveform select enable */
	opl( 0, 0x08, 0x00 );
	opl( 0, 0xBD, 0x00 );   /* melodic mode */
	for ( b = 0; b < 2; b++ ) {
		for ( r = 0; r < 9; r++ )
			opl( b, 0xB0 + r, 0 );          /* key off */
		for ( r = 0x40; r < 0x56; r++ )
			opl( b, r, 0x3F );              /* operators silent */
	}
}

/* --- voices -------------------------------------------------------------- */

static int levelof( int instlevel, struct voice *v )
{
	struct chan *c = &chans[v->chan];
	int l = instlevel + atten_tab[v->vel] + atten_tab[c->vol] + atten_tab[c->expr] + atten_tab[master] / 2;
	return l > 63 ? 63 : l;
}

static void voice_volume( int n )
{
	struct voice *v = &voices[n];
	const unsigned char *iv = genmidi[v->inst] + 4 + v->sub * 16;
	int bank = n / 9, ch = n % 9;
	opl( bank, 0x43 + opoff[ch], ( iv[11] & 0xC0 ) | levelof( iv[12] & 0x3F, v ) );
	if ( iv[6] & 1 )   /* additive: the modulator is heard as well */
		opl( bank, 0x40 + opoff[ch], ( iv[4] & 0xC0 ) | levelof( iv[5] & 0x3F, v ) );
}

static void voice_freq( int n, int keyon )
{
	struct voice *v = &voices[n];
	const unsigned char *in = genmidi[v->inst];
	int bank = n / 9, ch = n % 9;
	long p;
	int oct, block;
	unsigned fnum;

	/* pitch in 1/32 semitones */
	p = (long)v->playnote * 32 + chans[v->chan].bend / 128;
	if ( v->sub )
		p += ( (int)in[2] - 128 ) / 2;   /* fine tune of the second voice */
	if ( p < 0 )
		p = 0;
	oct = (int)( p / ( 12 * 32 ) );
	fnum = fnum_tab[p % ( 12 * 32 )];
	block = oct;    /* DMX note numbers: one octave above MIDI (the bank's
	                 * base note offsets are -12 for most instruments) */
	if ( block < 0 ) {
		fnum >>= -block;
		block = 0;
	} else if ( block > 7 ) {
		fnum = 1023;
		block = 7;
	}
	opl( bank, 0xA0 + ch, fnum & 0xFF );
	opl( bank, 0xB0 + ch, ( keyon ? 0x20 : 0 ) | ( block << 2 ) | ( ( fnum >> 8 ) & 3 ) );
}

static void voice_pan( int n )
{
	struct voice *v = &voices[n];
	const unsigned char *iv = genmidi[v->inst] + 4 + v->sub * 16;
	int pan = chans[v->chan].pan;
	int lr = pan < 48 ? 0x10 : pan > 80 ? 0x20 : 0x30;
	opl( n / 9, 0xC0 + n % 9, lr | ( iv[6] & 0x0F ) );
}

static void voice_off( int n )
{
	if ( voices[n].used ) {
		voice_freq( n, 0 );
		voices[n].used = 0;
	}
}

static int voice_alloc( void )
{
	int n, best = 0;
	for ( n = 0; n < NVOICES; n++ )
		if ( !voices[n].used )
			return n;
	for ( n = 1; n < NVOICES; n++ )
		if ( voices[n].age < voices[best].age )
			best = n;
	voice_off( best );
	return best;
}

static void voice_start( int chan, int note, int vel, int inst, int sub )
{
	int n = voice_alloc();
	struct voice *v = &voices[n];
	const unsigned char *in = genmidi[inst];
	const unsigned char *iv = in + 4 + sub * 16;
	int bank = n / 9, mod = opoff[n % 9], car = mod + 3;
	int play;

	v->used = 1;
	v->chan = chan;
	v->note = note;
	v->inst = inst;
	v->sub = sub;
	v->vel = vel;
	v->sustained = 0;
	v->age = ++agecnt;
	if ( in[0] & GM_FIXED )
		play = in[3];
	else
		play = note + (short)( iv[14] | ( iv[15] << 8 ) );   /* base note offset */
	while ( play < 0 )      /* like DMX */
		play += 12;
	while ( play > 95 )
		play -= 12;
	v->playnote = play;

	opl( bank, 0x20 + mod, iv[0] );
	opl( bank, 0x60 + mod, iv[1] );
	opl( bank, 0x80 + mod, iv[2] );
	opl( bank, 0xE0 + mod, iv[3] & 7 );
	opl( bank, 0x40 + mod, ( iv[4] & 0xC0 ) | ( iv[5] & 0x3F ) );
	opl( bank, 0x20 + car, iv[7] );
	opl( bank, 0x60 + car, iv[8] );
	opl( bank, 0x80 + car, iv[9] );
	opl( bank, 0xE0 + car, iv[10] & 7 );
	voice_pan( n );
	voice_volume( n );
	voice_freq( n, 1 );
}

/* --- MIDI messages ----------------------------------------------------- */

static void note_off( int chan, int note )
{
	int n;
	for ( n = 0; n < NVOICES; n++ )
		if ( voices[n].used && voices[n].chan == chan && voices[n].note == note && !voices[n].sustained ) {
			if ( chans[chan].pedal )
				voices[n].sustained = 1;
			else
				voice_off( n );
		}
}

static void note_on( int chan, int note, int vel )
{
	int inst;
	if ( !vel ) {
		note_off( chan, note );
		return;
	}
	if ( chan == PERCUSSION ) {
		if ( note < 35 || note > 81 )
			return;
		inst = 128 + note - 35;
	} else
		inst = chans[chan].prog;
	note_off( chan, note );   /* retrigger */
	voice_start( chan, note, vel, inst, 0 );
	if ( genmidi[inst][0] & GM_DOUBLE )
		voice_start( chan, note, vel, inst, 1 );
}

static void all_notes_off( int chan )
{
	int n;
	for ( n = 0; n < NVOICES; n++ )
		if ( voices[n].used && ( chan < 0 || voices[n].chan == chan ) )
			voice_off( n );
}

static void reset_controllers( int chan )
{
	chans[chan].vol = 100;
	chans[chan].expr = 127;
	chans[chan].pan = 64;
	chans[chan].pedal = 0;
	chans[chan].bend = 0;
}

static void controller( int chan, int ctl, int val )
{
	int n;
	switch ( ctl ) {
	case 7:   /* volume */
	case 11:  /* expression */
		if ( ctl == 7 )
			chans[chan].vol = val;
		else
			chans[chan].expr = val;
		for ( n = 0; n < NVOICES; n++ )
			if ( voices[n].used && voices[n].chan == chan )
				voice_volume( n );
		break;
	case 10:  /* pan */
		chans[chan].pan = val;
		for ( n = 0; n < NVOICES; n++ )
			if ( voices[n].used && voices[n].chan == chan )
				voice_pan( n );
		break;
	case 64:  /* sustain pedal */
		chans[chan].pedal = val >= 64;
		if ( !chans[chan].pedal )
			for ( n = 0; n < NVOICES; n++ )
				if ( voices[n].used && voices[n].chan == chan && voices[n].sustained )
					voice_off( n );
		break;
	case 120: /* all sound off */
	case 123: /* all notes off */
		all_notes_off( chan );
		break;
	case 121: /* reset all controllers */
		reset_controllers( chan );
		break;
	}
}

static void midi_msg( int status, int d1, int d2 )
{
	int chan = status & 0x0F, n;
	switch ( status & 0xF0 ) {
	case 0x80: note_off( chan, d1 ); break;
	case 0x90: note_on( chan, d1, d2 ); break;
	case 0xB0: controller( chan, d1, d2 ); break;
	case 0xC0: chans[chan].prog = d1; break;
	case 0xE0:
		chans[chan].bend = ( ( d2 << 7 ) | d1 ) - 8192;
		for ( n = 0; n < NVOICES; n++ )
			if ( voices[n].used && voices[n].chan == chan )
				voice_freq( n, 1 );
		break;
	}
}

static void midi_reset( void )
{
	int c;
	all_notes_off( -1 );
	for ( c = 0; c < 16; c++ ) {
		reset_controllers( c );
		chans[c].prog = 0;
	}
	mo.status = 0;
	mo.ndata = 0;
}

/* bytes of a MIDI stream (MODM_LONGDATA): running status, no sysex */
static void midi_byte( unsigned char b )
{
	int need;
	if ( b >= 0xF8 )            /* real time */
		return;
	if ( b >= 0x80 ) {
		mo.status = b < 0xF0 ? b : 0;   /* sysex/common: ignored */
		mo.ndata = 0;
		return;
	}
	if ( !mo.status )
		return;
	mo.data[mo.ndata++] = b;
	need = ( ( mo.status & 0xE0 ) == 0xC0 ) ? 1 : 2;   /* Cx, Dx: 1 byte */
	if ( mo.ndata == need ) {
		midi_msg( mo.status, mo.data[0], need > 1 ? mo.data[1] : 0 );
		mo.ndata = 0;
	}
}

static void callback( UINT msg, DWORD p1 )
{
	DriverCallback( mo.desc.dwCallback, mo.cbflags, (HANDLE)mo.desc.hMidi, msg,
					mo.desc.dwInstance, p1, 0 );
}

static DWORD mod_getdevcaps( LPMIDIOUTCAPS c, UINT size )
{
	MIDIOUTCAPS caps;
	UINT i;
	caps.wMid = 0;
	caps.wPid = 0;
	caps.vDriverVersion = 0x0100;
	copyname( caps.szPname, midi_port_name );
	caps.wTechnology = MOD_FMSYNTH;
	caps.wVoices = NVOICES;
	caps.wNotes = NVOICES;
	caps.wChannelMask = 0xFFFF;
	caps.dwSupport = MIDICAPS_VOLUME | MIDICAPS_LRVOLUME;
	if ( size > sizeof(caps) )
		size = sizeof(caps);
	for ( i = 0; i < size; i++ )
		((char FAR *)c)[i] = ((char *)&caps)[i];
	return MMSYSERR_NOERROR;
}

DWORD FAR PASCAL __export __loadds modMessage( UINT id, UINT msg, DWORD user, DWORD p1, DWORD p2 )
{
	if ( id != 0 && msg != MODM_GETNUMDEVS )
		return MMSYSERR_BADDEVICEID;
	switch ( msg ) {
	case MODM_GETNUMDEVS:
		return vxdok ? 1 : 0;
	case MODM_GETDEVCAPS:
		return mod_getdevcaps( (LPMIDIOUTCAPS)p1, (UINT)p2 );
	case MODM_OPEN:
		if ( mo.open )
			return MMSYSERR_ALLOCATED;
		mo.desc = *(LPMIDIOPENDESC)p1;
		mo.cbflags = HIWORD( p2 );
		mo.open = TRUE;
		opl_reset();
		midi_reset();
		callback( MOM_OPEN, 0 );
		return MMSYSERR_NOERROR;
	case MODM_CLOSE:
		midi_reset();
		mo.open = FALSE;
		callback( MOM_CLOSE, 0 );
		return MMSYSERR_NOERROR;
	case MODM_DATA:
		midi_msg( (int)( p1 & 0xFF ), (int)( p1 >> 8 ) & 0x7F, (int)( p1 >> 16 ) & 0x7F );
		return MMSYSERR_NOERROR;
	case MODM_LONGDATA: {
		LPMIDIHDR h = (LPMIDIHDR)p1;
		DWORD i;
		if ( !( h->dwFlags & MHDR_PREPARED ) )
			return MIDIERR_UNPREPARED;
		for ( i = 0; i < h->dwBufferLength; i++ )
			midi_byte( ((unsigned char huge *)h->lpData)[i] );
		h->dwFlags |= MHDR_DONE;
		callback( MOM_DONE, (DWORD)h );
		return MMSYSERR_NOERROR;
	}
	case MODM_RESET:
		midi_reset();
		return MMSYSERR_NOERROR;
	case MODM_GETVOLUME:
		*(DWORD FAR *)p1 = mastervol;
		return MMSYSERR_NOERROR;
	case MODM_SETVOLUME: {
		int n;
		mastervol = p1;
		master = (unsigned char)( ( ( p1 & 0xFFFF ) + ( p1 >> 16 ) ) >> 10 );   /* average, 0-127 */
		for ( n = 0; n < NVOICES; n++ )
			if ( voices[n].used )
				voice_volume( n );
		return MMSYSERR_NOERROR;
	}
	case MODM_PREPARE:
	case MODM_UNPREPARE:
	default:
		return MMSYSERR_NOTSUPPORTED;
	}
}
