/* VXDWAVE.C: wave output of VSBVXD for the Windows wave driver VSBHDA.DRV.
 * The driver queues the (page locked) WAVEHDR buffers of the applications
 * by their linear addresses; the sound hardware interrupt mixes them into
 * the output of the SB emulation (vxdwave_mix(), called by SNDISR.C) with
 * sample rate conversion.  The driver collects the finished buffers with
 * a timer (VWAPI_GETDONE).
 *
 * Protected mode API (far call to the entry of Int 2Fh, AX=1684h,
 * BX=VSBVXD_DEVICE_ID), AX = function:
 *   0 GETVERSION  out: AX = version, EBX = output sample rate
 *   1 OPEN        EBX = sample rate, ECX = channels, EDX = bits per sample
 *   2 CLOSE
 *   3 WRITE       ESI = linear address, ECX = length, EDX = id
 *   4 GETDONE     out: EDX = id of a finished buffer, CF if none
 *   5 RESET       all queued buffers are finished, position = 0
 *   6 PAUSE
 *   7 RESTART
 *   8 GETPOS      out: EAX = bytes played since open/reset
 *   9 SETVOLUME   EBX = volume (low word left, high word right, 0-FFFFh)
 *  10 SETMASTER   EBX = master volume of the sound card (like SETVOLUME)
 * Errors: CF set. */

#include <stdint.h>

#include "VXDLIB.H"

#define VWAPI_VERSION 0x0100

/* client register structure (Client_Reg_Struc) */
struct client {
	uint32_t edi, esi, ebp, res, ebx, edx, ecx, eax, error, eip;
	uint16_t cs, cs_hi;
	uint32_t eflags;
};
#define CF 1

#define QSIZE 256   /* power of 2 */

static struct {
	uint8_t  open, paused;
	uint16_t channels, bits, blockalign;
	uint32_t rate;
	uint32_t step, frac;     /* source frames per output frame, 16.16 */
	struct { uint32_t lin, len, id; } q[QSIZE];
	unsigned qhead, qtail;   /* queued: qhead..qtail-1, qhead is playing */
	uint32_t pos;            /* byte position in the playing buffer */
	uint32_t done[QSIZE];
	unsigned dhead, dtail;
	uint32_t played;         /* bytes */
	uint32_t vol_l, vol_r;   /* 0-10000h */
} wv;

extern struct globalvars gvars;
extern int vsb_hwfreq( void );
extern void vsb_setmaster( int percent );

static uint32_t irqsave( void );
#pragma aux irqsave = "pushfd" "pop eax" "cli" value [eax]
static void irqrestore( uint32_t );
#pragma aux irqrestore = "push eax" "popfd" parm [eax]

static void finish_head( void )
{
	wv.done[wv.dtail++ & (QSIZE - 1)] = wv.q[wv.qhead & (QSIZE - 1)].id;
	wv.qhead++;
	wv.pos = 0;
}

void vsb_pm_api( unsigned vm, struct client *c )
{
	uint32_t fl;
	(void)vm;
	c->eflags &= ~CF;
	switch ( c->eax & 0xFFFF ) {
	case 0:
		c->eax = VWAPI_VERSION;
		c->ebx = vsb_hwfreq();
		break;
	case 1:
		if ( c->ebx < 1000 || c->ebx > 96000 || ( c->ecx != 1 && c->ecx != 2 ) ||
			 ( c->edx != 8 && c->edx != 16 ) || !vsb_hwfreq() ) {
			c->eflags |= CF;
			break;
		}
		fl = irqsave();
		wv.rate = c->ebx;
		wv.channels = c->ecx;
		wv.bits = c->edx;
		wv.blockalign = wv.channels * wv.bits / 8;
		wv.step = (uint32_t)( ( (uint64_t)wv.rate << 16 ) / vsb_hwfreq() );
		wv.frac = 0;
		wv.qhead = wv.qtail = wv.dhead = wv.dtail = 0;
		wv.pos = wv.played = 0;
		wv.paused = 0;
		if ( !wv.vol_l && !wv.vol_r )
			wv.vol_l = wv.vol_r = 0x10000;
		wv.open = 1;
		irqrestore( fl );
		break;
	case 2:
		wv.open = 0;
		break;
	case 3:
		if ( wv.qtail - wv.qhead >= QSIZE || !c->ecx ) {
			c->eflags |= CF;
			break;
		}
		fl = irqsave();
		wv.q[wv.qtail & (QSIZE - 1)].lin = c->esi;
		wv.q[wv.qtail & (QSIZE - 1)].len = c->ecx - c->ecx % wv.blockalign;
		wv.q[wv.qtail & (QSIZE - 1)].id = c->edx;
		wv.qtail++;
		irqrestore( fl );
		break;
	case 4:
		fl = irqsave();
		if ( wv.dhead == wv.dtail )
			c->eflags |= CF;
		else
			c->edx = wv.done[wv.dhead++ & (QSIZE - 1)];
		irqrestore( fl );
		break;
	case 5:
		fl = irqsave();
		while ( wv.qhead != wv.qtail )
			finish_head();
		wv.played = 0;
		wv.frac = 0;
		irqrestore( fl );
		break;
	case 6:
		wv.paused = 1;
		break;
	case 7:
		wv.paused = 0;
		break;
	case 8:
		c->eax = wv.played;
		break;
	case 9:
		wv.vol_l = ( c->ebx & 0xFFFF ) + 1;
		wv.vol_r = ( c->ebx >> 16 ) + 1;
		break;
	case 10:
		vsb_setmaster( ( ( c->ebx & 0xFFFF ) + ( c->ebx >> 16 ) ) * 50 / 0xFFFF );   /* percent */
		break;
	default:
		c->eflags |= CF;
	}
}

static int sample( const uint8_t *p, int right )
{
	if ( wv.channels == 2 && right )
		p += wv.bits / 8;
	return wv.bits == 16 ? *(int16_t *)p : ( *p - 128 ) << 8;
}

static int16_t sat( int v )
{
	return v > 32767 ? 32767 : v < -32768 ? -32768 : v;
}

/* mix the queued buffers into pcm (16-bit stereo, n frames at the
 * output rate); called by the sound hardware interrupt */
void vxdwave_mix( int16_t *pcm, int n )
{
	int i;
	if ( !wv.open || wv.paused )
		return;
	for ( i = 0; i < n && wv.qhead != wv.qtail; i++ ) {
		const uint8_t *p = (const uint8_t *)wv.q[wv.qhead & (QSIZE - 1)].lin + wv.pos;
		int l = sample( p, 0 ), r = sample( p, 1 );
		pcm[2 * i]     = sat( pcm[2 * i]     + (int)( ( (int64_t)l * wv.vol_l ) >> 16 ) );
		pcm[2 * i + 1] = sat( pcm[2 * i + 1] + (int)( ( (int64_t)r * wv.vol_r ) >> 16 ) );
		for ( wv.frac += wv.step; wv.frac >= 0x10000; wv.frac -= 0x10000 ) {
			wv.pos += wv.blockalign;
			wv.played += wv.blockalign;
			if ( wv.pos >= wv.q[wv.qhead & (QSIZE - 1)].len ) {
				finish_head();
				if ( wv.qhead == wv.qtail )
					break;
			}
		}
	}
}
