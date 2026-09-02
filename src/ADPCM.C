
/* Creative Labs ADPCM decoding */

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "CONFIG.H"
#include "ADPCM.H"

#if ADPCM

/* USENEXTBITS determines the "last" data byte for resampling
 * 0: the previously decoded byte is copied
 * 1: the next ADPCM encoded byte is used for decoding
 */
#define USENEXTBITS 0

struct ADPCM_STATE adpcm_state;

// functions decode_ADPCM_2|3|4() origin is DosBox: DosBox/src/hardware/sblaster.cpp!

typedef int Bits_t;

static uint8_t *decode_ADPCM_4_samples( uint8_t *dst, uint8_t *src, int count, struct ADPCM_STATE *pADPCM)
//////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    static const int8_t scaleMap[64] = {
        0,  1,  2,  3,  4,  5,  6,  7,  0,  -1,  -2,  -3,  -4,  -5,  -6,  -7,
        1,  3,  5,  7,  9, 11, 13, 15, -1,  -3,  -5,  -7,  -9, -11, -13, -15,
        2,  6, 10, 14, 18, 22, 26, 30, -2,  -6, -10, -14, -18, -22, -26, -30,
        4, 12, 20, 28, 36, 44, 52, 60, -4, -12, -20, -28, -36, -44, -52, -60
    };
    static const uint8_t adjustMap[64] = {
          0, 0, 0, 0, 0, 16, 16, 16,
          0, 0, 0, 0, 0, 16, 16, 16,
        240, 0, 0, 0, 0, 16, 16, 16,
        240, 0, 0, 0, 0, 16, 16, 16,
        240, 0, 0, 0, 0, 16, 16, 16,
        240, 0, 0, 0, 0, 16, 16, 16,
        240, 0, 0, 0, 0,  0,  0,  0,
        240, 0, 0, 0, 0,  0,  0,  0
    };
    int i;
    Bits_t samp;
    Bits_t ref;

    for ( ; count; count--, src++ ) {
        for ( i = 4; i >= 0; i -= 4 ) {

            samp = ((*src >> i) & 0xf ) + pADPCM->scale;
            if ((samp < 0) || (samp > 63)) {
                dbgprintf(("decode_ADPCM_4_samples: Bad sample %X *src=%X i=%u\n", samp, *src, i));
                if(samp < 0 ) samp =  0;
                if(samp > 63) samp = 63;
            }
            ref = pADPCM->ref + scaleMap[samp];
            pADPCM->scale = (pADPCM->scale + adjustMap[samp]) & 0xff;
            pADPCM->ref = ( ref > 0xff) ? 0xff : ( ref < 0x00 ) ? 0x00 : (uint8_t)ref;
            *dst++ = pADPCM->ref;
        }
    }
#if USENEXTBITS
    samp = ((*src >> 4 ) & 0xf ) + pADPCM->scale;
    *dst = pADPCM->ref + scaleMap[samp];
#endif

    return dst;
}

static uint8_t *decode_ADPCM_3_samples( uint8_t *dst, uint8_t *src, int count, struct ADPCM_STATE *pADPCM)
//////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    static const int8_t scaleMap[40] = {
        0,  1,  2,  3,  0,  -1,  -2,  -3,
        1,  3,  5,  7, -1,  -3,  -5,  -7,
        2,  6, 10, 14, -2,  -6, -10, -14,
        4, 12, 20, 28, -4, -12, -20, -28,
        5, 15, 25, 35, -5, -15, -25, -35
    };
    static const uint8_t adjustMap[40] = {
          0, 0, 0, 8,   0, 0, 0, 8,
        248, 0, 0, 8, 248, 0, 0, 8,
        248, 0, 0, 8, 248, 0, 0, 8,
        248, 0, 0, 8, 248, 0, 0, 8,
        248, 0, 0, 0, 248, 0, 0, 0
    };
    int i;
    Bits_t samp;
    Bits_t ref;

    for ( ; count; count--, src++ ) {
        uint32_t tsample = *src << 1; /* we need a 9-bit source here! */
        for ( i = 6; i >= 0; i -= 3 ) {

            samp = ((tsample >> i ) & 0x7 ) + pADPCM->scale;
            if ((samp < 0) || (samp > 39)) {
                dbgprintf(("decode_ADPCM_3_samples: Bad sample %X *src=%X i=%u\n", samp, *src, i));
                if(samp < 0 ) samp =  0;
                if(samp > 39) samp = 39;
            }

            ref = pADPCM->ref + scaleMap[samp];
            pADPCM->scale = (pADPCM->scale + adjustMap[samp]) & 0xff;
            pADPCM->ref = ( ref > 0xff) ? 0xff : ( ref < 0x00 ) ? 0x00 : (uint8_t)ref;
            *dst++ = pADPCM->ref;
        }
    }
#if USENEXTBITS
    samp = ((*src >> 5 ) & 0x7 ) + pADPCM->scale;
    *dst = pADPCM->ref + scaleMap[samp];
#endif
    return dst;
}

static uint8_t *decode_ADPCM_2_samples( uint8_t *dst, uint8_t *src, int count, struct ADPCM_STATE *pADPCM)
//////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    static const int8_t scaleMap[24] = {
        0,   1,   0,  -1,   1,   3,  -1,  -3,
        2,   6,  -2,  -6,   4,  12,  -4, -12,
        8,  24,  -8, -24,  16,  48, -16, -48
    };
    static const uint8_t adjustMap[24] = {
        0,   4,   0,   4, 252,   4, 252,   4,
      252,   4, 252,   4, 252,   4, 252,   4,
      252,   4, 252,   4, 252,   0, 252,   0
    };
    int i;
    Bits_t samp;
    Bits_t ref;

    for ( ; count; count--, src++ ) {
        for ( i = 6; i >= 0; i -= 2 ) {

            samp = ((*src >> i ) & 0x3 ) + pADPCM->scale;
            if ((samp < 0) || (samp > 23)) {
                dbgprintf(("decode_ADPCM_2_samples: Bad sample %X *src=%X i=%u\n", samp, *src, i));
                if(samp < 0 ) samp =  0;
                if(samp > 23) samp = 23;
            }

            ref = pADPCM->ref + scaleMap[samp];
            pADPCM->scale = (pADPCM->scale + adjustMap[samp]) & 0xff;
            pADPCM->ref = ( ref > 0xff) ? 0xff : ( ref < 0x00 ) ? 0x00 : (uint8_t)ref;
            *dst++ = pADPCM->ref;
        }
    }
#if USENEXTBITS
    samp = ((*src >> 6 ) & 0x3 ) + pADPCM->scale;
    *dst = pADPCM->ref + scaleMap[samp];
#endif
    return dst;
}

/* argument 'count' does NOT include the ref byte!
 * v2.0: changed memory allocation and handling of ref byte.
 */

int DecodeADPCM( uint8_t *pcm, int count, int bits )
////////////////////////////////////////////////////
{
    int start = adpcm_state.useRef;
    uint8_t *dst = pcm;
    uint8_t *src;

    /* bits may be 2,3,4 -> new count = bytes * 4,3,2 */
    src = pcm + ((count * ( 6 - bits ) + 15) & ~0x3);

    if( adpcm_state.useRef ) {
        adpcm_state.useRef = 0;
        adpcm_state.ref = *pcm;
        adpcm_state.scale = 0;
        src++;
    }

    memcpy( src, pcm + start, count + USENEXTBITS );

    switch ( bits ) {
    case 2:  dst = decode_ADPCM_2_samples( dst, src, count, &adpcm_state ); break;
    case 3:  dst = decode_ADPCM_3_samples( dst, src, count, &adpcm_state ); break;
    default: dst = decode_ADPCM_4_samples( dst, src, count, &adpcm_state ); break;
    }
#if !USENEXTBITS
    /* v2.0: cv_rate() now expects one byte more to be present; for ADPCM, this extra byte
     * can only be set NOW, after decoding!
     */
    *dst = *(dst-1);
#endif
    dbgprintf(("DecodeADPCM( %X, cnt=%u, bits=%u ): useRef=%u, new count=%u\n", pcm, count, bits, start, dst - pcm ));
# ifdef _DEBUG
#  if 0 /* src bytes (ADPCM encoded data) to be displayed? */
    dbgprintf(("ADPCMbytes: "));
    if ( start )
        dbgprintf(("ref=%02X ", adpcm_state.ref));
    for ( ; count; count--, src++ )
        dbgprintf(("%02X%c", *src, (count == 1) ? '\n' : ' '));
#  endif
#  if 0 /* dst bytes (8-bit unsigned mono PCM) to be displayed? */
    dbgprintf(("PCMbytes: "));
    for ( src = pcm; src < dst; src++ )
        dbgprintf(("%02X%c", *src, (src == dst - 1) ? '\n' : ' '));
#  endif
# endif
    return dst - pcm;
}
#endif

