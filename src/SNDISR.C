
/* sound hardware interrupt routine */

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "CONFIG.H"
#include "PLATFORM.H"
#include "PIC.H"
#include "LINEAR.H"
#include "VDMA.H"
#include "VIRQ.H"
#include "VOPL3.H"
#include "VSB.H"
#include "PTRAP.H"
#include "ADPCM.H"

#ifdef _DEBUG
//#define SNDISRLOG /* enables sound interrupt logs */
#include <stdio.h>

/* optionally emit PCM data;
 * if activated, file logfile.asm (HDLFUNC!) must also be changed!
 * LOGPCM8DATA: log happens BEFORE sample rate conversion
 * LOGPCM16DATA: log happens AFTER sample rate conversion
 */
#define LOGPCM8DATA  1 /* support /LM1 - 8-bit PCM data, mono only */
#define LOGPCM16DATA 1 /* support /LM2 - 16-bit PCM data, mono only */

# if LOGPCM8DATA
#  ifdef DJGPP
static inline void writepcm8data(unsigned char x) { asm("movb %0, %%dl\n\t" "movw $0x81, %%ax\n\t" "int $0x41" ::"r" (x): "%eax", "%edx"); }
#  else
void writepcm8data(unsigned char);
#pragma aux writepcm8data = \
    "mov ax, 0081h" \
    "int 41h" \
    parm [dl] \
    modify exact [eax edx]
#  endif
# endif
# if LOGPCM16DATA
#  ifdef DJGPP
static inline void writepcm16data(short x) { asm("movw %0, %%dx\n\t" "movw $0x82, %%ax\n\t" "int $0x41" ::"r" (x): "%eax", "%edx" ); }
#  else
void writepcm16data(short);
#pragma aux writepcm16data = \
    "mov ax, 0082h" \
    "int 41h" \
    parm [dx] \
    modify exact [eax edx]
#  endif
# endif

#endif

#include "AU.H"

#if SOUNDFONT
#include "VMPU.H"
//extern tsf* tsfrenderer;
extern void* tsfrenderer;
void tsf_render_short(void *, short *, int, int);
#endif

#define SUP16BITUNSIGNED 1 /* support 16-bit unsigned format */

#define MIXERROUTINE 0

#define VOICELR 1

bool _SND_InstallISR( uint8_t, int(*ISR)(void) );
bool _SND_UninstallISR( uint8_t );

#if MUXERROUTINE==2
extern void SNDISR_Mixer( uint16_t *, uint16_t *, uint32_t, uint32_t, uint32_t );
#endif
extern void fatal_error( int );

extern struct globalvars gvars;

struct SNDISR_s {
	int16_t *pPCM;
	uint32_t DMA_linearBase; /* linear start address of current DMA buffer */
	uint32_t DMA_Base;       /* (physical) base address of DMA buffer at last remapping */
	uint32_t DMA_Size;       /* size of DMA buffer at last remapping */
	uint32_t Block_Handle;   /* handle of remapping block */
	uint32_t Block_Addr;     /* linear base of remapping block ( page aligned ) */
#if PT0V86
	uint32_t PageTab0v86;	 /* v1.8: linear address v86 pagetab 0 */
#endif
	void *hAU;
#if SETABSVOL
	uint16_t SB_VOL;
#endif
	uint8_t SndIrq;
#ifdef _LOGBUFFMAX /* log the usage of the PCM buffer? */
	uint32_t dwMaxBytes;
#endif
#ifdef _DEBUG
    int max_samples;
    int total_samples;
    int cntTotal;
    int cntDigital;
#endif
};

static struct SNDISR_s isr = {NULL,-1,0,0};

#ifndef DJGPP
/* here malloc/free is superfast since it's a very simple "stack" */
#define MALLOCSTATIC 0
#else
#define MALLOCSTATIC 1
#endif

#if SLOWDOWN

static void delay_10us(unsigned int ticks)
//////////////////////////////////////////
{
	static uint64_t oldtsc = 0;
	uint64_t newtsc;

	do {
		newtsc = rdtsc();
	} while ( (newtsc - oldtsc) < ( ticks << 18 ) );
	oldtsc = newtsc;
}
#endif

/* rate conversion.
 * src & dst are 16-bit, channels is either 1 or 2; if it's 2, nSamples is even!
 * out: new sample cnt.
 *
 * example: 16 samples, 1 channel, srcrate=11025, dstrate=44100:
 * 1. instep = (0 << 12) | ((4096 * ( 11025 % 44100 ) / 44100 + 1) & 0xfff)
 *           = (( 4096 * 11025 ) / 44100 + 1) & 0xfff
 *           = ( 45.158.400 / 44100 + 1) & 0xfff
 *           = 1025 & 0xfff -> 1025
 * 2. loops: 65536 / 1025 = 63
 *
 */

static unsigned int cv_rate( PCM_CV_TYPE_S *pcmsrc, const unsigned int nSamples, const unsigned int channels, unsigned int srcrate, unsigned int dstrate)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	/* v2.0: new instep calculation seems a bit more intuitive */
	//const unsigned int instep = ((srcrate / dstrate) << 12) | (((((srcrate % dstrate) << 12 ) + dstrate - 1 ) / dstrate) & 0xFFF);
	const unsigned int instep = ((srcrate / dstrate) << 12) | ((((srcrate % dstrate) << 12 ) / dstrate + 1 ) & 0xFFF);

	const unsigned int inend = (nSamples >> (channels - 1)) << 12;
	PCM_CV_TYPE_S *pcmdst;
#ifdef _DEBUG
	unsigned int idx;
#endif
	//unsigned int inpos = (srcrate < dstrate) ? (instep >> 1) : 0;
	unsigned int inpos = 0; /* bits 0-11 are position between 2 samples, bits 12-31 are sample index */
#if MALLOCSTATIC
	static int maxsample = 0;
	static PCM_CV_TYPE_S* buff = NULL;
#else
	PCM_CV_TYPE_S* buff;
#endif

	//if(!nSamples)
	//	return 0;

#if MALLOCSTATIC
	if ( nSamples > maxsample ) {
		if ( buff )
			free( buff );
		buff = (PCM_CV_TYPE_S*)malloc( (nSamples+2) * sizeof(PCM_CV_TYPE_S) );
		maxsample = nSamples;
	}
#else
	buff = (PCM_CV_TYPE_S*)malloc( (nSamples+2) * sizeof(PCM_CV_TYPE_S));
#endif
	memcpy( buff, pcmsrc, (nSamples+2) * sizeof(PCM_CV_TYPE_S) );

	pcmdst = pcmsrc;

    /* v2.0: one additional sample is now supplied, so the last sample won't
     *       need special treatment ( variable total removed ).
     */

	for ( inpos = 0; inpos < inend; inpos += instep ) {
		unsigned int m1,m2;
#ifndef _DEBUG
		unsigned int idx;
#endif
		PCM_CV_TYPE_S *incurr,*innext;

		idx = (inpos >> 12 ) << ( channels - 1);
		m2 = inpos & 0xFFF;
		m1 = 4096 - m2;
		incurr = buff + idx;
		innext = buff + idx + channels;
		*pcmdst++ = ( *incurr * m1 + *innext * m2 ) >> 12;
		if ( channels > 1 )
			*pcmdst++ = ( *(incurr+1) * m1 + *(innext+1) * m2 ) >> 12;
	}

#ifdef SNDISRLOG
	dbgprintf(("cv_rate(smpl=%u, chn=%u) in step/end=%u/%u idx=%u new smpl=%u\n", nSamples, channels, instep, inend, idx, (pcmdst - pcmsrc) >> ( channels - 1) ));
#endif

#if !MALLOCSTATIC
	free(buff);
#endif
	/* v2.0: shift added to return "true" sample count */
	//return ( pcmdst - pcmsrc );
	return ( (pcmdst - pcmsrc) >> ( channels - 1 ) );
}

/* convert 8-bits signed/unsigned to 16-bits signed. */

static void cv_bits_8_to_16( PCM_CV_TYPE_S *pcm, unsigned int nSamples, uint8_t issigned )
//////////////////////////////////////////////////////////////////////////////////////////
{
	PCM_CV_TYPE_UC *srcu;
	PCM_CV_TYPE_SC *srcs;
	PCM_CV_TYPE_S *dst = pcm + nSamples - 1;

    if ( issigned ) {
        srcs = (PCM_CV_TYPE_SC *)pcm + nSamples - 1;
        for ( ; nSamples; nSamples-- )
            *dst-- = (PCM_CV_TYPE_S)((*srcs--) << 8);
    } else {
        srcu = (PCM_CV_TYPE_UC *)pcm + nSamples - 1;
        for ( ; nSamples; nSamples-- )
            *dst-- = (PCM_CV_TYPE_S)((*srcu-- ^ 0x80) << 8);
    }
}

/* convert mono to stereo. */

#if 1
static void cv_channels_1_to_2( PCM_CV_TYPE_S *pcm_sample, unsigned int nSamples )
//////////////////////////////////////////////////////////////////////////////////
{
    PCM_CV_TYPE_S *src = pcm_sample + nSamples - 1;
    PCM_CV_TYPE_S *dst = pcm_sample + nSamples * 2 - 1;

    for( ; nSamples; nSamples-- ) {
        *dst-- = *src; *dst-- = *src--;
    }
    return;
}
#else
extern void cv_channels_1_to_2( PCM_CV_TYPE_S *pcm_sample, unsigned int nSamples );
#endif

static int SNDISR_Interrupt( void )
///////////////////////////////////
{
    uint32_t mastervol;
    uint32_t voicevol;
    uint32_t midivol;
#if VOICELR
    uint32_t mastervol2;
    uint32_t voicevol2;
#endif
    int16_t* pPCMOPL;
    uint32_t freq;
    int nSamples; /* # of samples requested by sound hardware */
    int IdxSm; /* sample index in 16bit PCM buffer */
    int i;
#if COMPAT4
    uint16_t mask;
#endif
#ifdef _DEBUG
    int loop;
#endif

    /* check if the sound hw does request an interrupt. */
    if( !AU_isirq( isr.hAU ) )
        return(0);

#if COMPAT4
    /* v1.8: /CF4 */
    if ( gvars.compatflags & CF_MASKPIT ) {
        mask = PIC_GetIRQMask();
        PIC_SetIRQMask(mask | 1);
    }
#endif
    /* since the client context is now restored when a SB IRQ is emulated,
     * it's safe to call VIRQ_Invoke here. This will happen only for
     * DSP cmds 0xF2/0xF3 (trigger IRQ).
     * Todo: check if SB emulated Irq is masked; if yes, don't trigger!
     */
    if ( VSB_GetIRQStatus() )
        VIRQ_Invoke();

#if SETIF
    _enable_ints();
#endif

    //AU_setoutbytes( isr.hAU ); //v1.9: now obsolete
    nSamples = AU_cardbuf_space( isr.hAU ) / ( sizeof(int16_t) * 2 ); //16 bit, 2 channels
    if ( !nSamples ) { /* no free space in DMA buffer? Shouldn't happen... */
        dbgprintf(("isr: ERROR - AU_cardbuf_space() returned 0 samples\n" ));
        goto isrexit;
    }
    freq = AU_getfreq( isr.hAU );
#ifdef _DEBUG
    if ( nSamples > isr.max_samples )
        isr.max_samples = nSamples;
    isr.total_samples += nSamples;
    isr.cntTotal++;
    //dbgprintf(("isr: samples:%u ",nSamples));
    loop = 0;
    for ( IdxSm = 0, isr.cntDigital++; VSB_Running() && IdxSm < nSamples; loop++ ) {
        int ocnt;
#else
    for ( IdxSm = 0; VSB_Running() && IdxSm < nSamples; ) {
#endif
        /* a loop that may run 2 (or multiple) times if a SB buffer overrun occured */
        int i,j;
        int dmachannel = VSB_GetDMA();
        int bytes; /* no of bytes to be copied from SB DMA buffer */
        int bits = VSB_GetBits();
        int channels = VSB_GetChannels();
        int samplesize = ( bits + 7 ) >> 3;
        int count = nSamples - IdxSm; /* samples to handle in this turn */
        int sbcnt;
        bool resample;
        uint32_t DMA_Base;
        uint32_t DMA_Index;
        int32_t DMA_Count;
        uint32_t SB_BuffSpace = VSB_GetBuffSpace(); /* remaining buffer size in bytes */
        uint32_t SB_Rate = VSB_GetSampleRate();
        int IsSilent = VSB_IsSilent();

        if ( !IsSilent ) {
            DMA_Base = VDMA_GetBase(dmachannel);
            DMA_Index = VDMA_GetIndex(dmachannel);
            DMA_Count = VDMA_GetCount(dmachannel);
            /* check if the current DMA buffer is within the mapped region. */
#if PT0V86
            /* v1.8: if access to v86 pagetab 0 is installed, translate upper memory address
             * to physical address; this is needed because hdpmi is a VCPI client, hence has
             * no knowledge of the current v86 mappings.
             */
            if ( DMA_Base < 0x100000 && DMA_Base >= 0xA0000 && isr.PageTab0v86 ) {
#ifdef _DEBUG
                uint32_t tmp = DMA_Base;
#endif
                DMA_Base = (*((uint32_t *)NearPtr(isr.PageTab0v86) + (DMA_Base >> 12 )) & ~0xfff) | (DMA_Base & 0xFFF);
                dbgprintf(("isr(%u), conv address %X -> phys address %X [pgtab0=%X]\n", loop, tmp, DMA_Base, isr.PageTab0v86 ));
            }
#endif
            if( !(DMA_Base >= isr.DMA_Base && (DMA_Base + DMA_Index + DMA_Count) <= (isr.DMA_Base + isr.DMA_Size) )) {
                isr.DMA_linearBase = -1;
            }
            /* if there's no mapped region, create one that covers current DMA op. */
            if( isr.DMA_linearBase == -1 ) {
                isr.DMA_Base = DMA_Base;
                isr.DMA_Size = min( max(DMA_Index + DMA_Count, 0x4000 ), 0x20000 );
                if ( DMA_Base < 0x100000 ) {
                    isr.DMA_linearBase = DMA_Base;
                } else {
                    /* size is in pages, phys. address must have bits 0-11 cleared */
                    if( __dpmi_map_physical_device(isr.Block_Handle, 0, (isr.DMA_Size + (isr.DMA_Base & 0xfff) + 4095 ) >> 12 , isr.DMA_Base & ~0xfff ) == -1 )
                        fatal_error( 2 );
                    isr.DMA_linearBase = isr.Block_Addr | (isr.DMA_Base & 0xFFF);
                }
                dbgprintf(("isr(%u), ISR_DMA address (re)mapped: isr.DMA_Base(%d)=%x, isr.DMA_Size=%x, isr.DMA_linearBase=%x\n",
                           loop, dmachannel, isr.DMA_Base, isr.DMA_Size, isr.DMA_linearBase ));
            }
        }
        /* don't resample if sample rates are close? */
        if( SB_Rate != freq ) {
            int tmpcnt = count * SB_Rate / freq;
            resample = true;
            //count = max( channels, count / ( ( freq + SB_Rate-1) / SB_Rate ));
            /* v2.0: fixed: operands for modulus op were wrong - count was ALWAYS increased,
             * even if freq was an exact multiple of SB_Rate.
             */
            //if ( SB_Rate < freq && SB_Rate % freq ) count++;
            /* in Quake, count = 0 seems to occure?  */
            //if ( SB_Rate < freq && freq % SB_Rate ) count++;
            //if ( ( SB_Rate < freq && freq % SB_Rate ) || !count ) count++;
            /* v2.0: even if freq is an exact multiple of SB_Rate, the division
             * may have given a too small value of count!
             */
            while ( count > ( tmpcnt * freq / SB_Rate ) )
                tmpcnt++;
            count = tmpcnt;
        } else
            resample = false;
#ifdef _DEBUG
        ocnt = count;
        //dbgprintf(("isr(%u): c=0x%02X ocnt=0x%02X\n", loop, count, ocnt ));
#endif
#if ADPCM
        if( bits < 8 ) { /* ADPCM? */
            sbcnt = SB_BuffSpace - adpcm_state.useRef;
            //count += count % ( 6 - bits );
            count = min( count, sbcnt * (6 - bits) );
            bytes = (count+(6 - bits)-1) / (6 - bits) + adpcm_state.useRef;
# ifdef SNDISRLOG
            dbgprintf(("isr(%u): ADPCM bits=%u bytes=%u samples=%u count=%u SB BuffSpace=%u\n", loop, bits, bytes, nSamples, count, SB_BuffSpace ));
# endif
        } else
#endif
        {
            /* samplesize and channels can be either 1 or 2 */
            sbcnt = SB_BuffSpace / (samplesize * channels);
            /* v2.0: ensure that count hasn't become < samples - that would distort sound */
            if ( SB_BuffSpace % (samplesize * channels) )
                sbcnt++;

            count = min( count, max(1, sbcnt));
            bytes = count * samplesize * channels;
        }

        /* copy samples to our PCM buffer */
        if( IsSilent ) {
            memset( isr.pPCM + IdxSm * 2, 0, bytes + 1 ); /* v2.0: one extra byte for resampling */
        } else {
            char *pDest = (char *)(isr.pPCM + IdxSm * 2);
            if ( DMA_Count < bytes ) {
                /* v2.0: DMA buffer underrun handled here now; this approach avoids
                 *       multiple format conversions if DMA buffer size is small.
                 */
                int chunk;
                int tmpbytes;
#ifdef SNDISRLOG
                dbgprintf(("isr(%u): DMA space < bytes (0x%X) samples=0x%X DMA Idx/Cnt=0x%X/0x%X\n", loop, bytes, nSamples, DMA_Index, DMA_Count ));
#endif
                if ( !VDMA_IsAuto(dmachannel) ) {
                    count = DMA_Count / (samplesize * channels );
                    bytes = DMA_Count;
                }
                for ( tmpbytes = 0; tmpbytes < bytes; tmpbytes += chunk ) {
                    chunk = min( DMA_Count, bytes - tmpbytes );
                    memcpy( pDest + tmpbytes, NearPtr(isr.DMA_linearBase + ( DMA_Base - isr.DMA_Base) + DMA_Index ), chunk );
                    DMA_Index = VDMA_SetIndexCount(dmachannel, DMA_Index + chunk, DMA_Count - chunk );
                    DMA_Count = VDMA_GetCount(dmachannel);
#ifdef SNDISRLOG
                    dbgprintf(("isr(%u): chunk=%X tmpbytes=%X DMA Idx/Cnt=0x%X/0x%X\n", loop, chunk, tmpbytes, DMA_Index, DMA_Count ));
#endif
                }
            } else {
                memcpy( pDest, NearPtr(isr.DMA_linearBase + ( DMA_Base - isr.DMA_Base) + DMA_Index ), bytes );
                DMA_Index = VDMA_SetIndexCount(dmachannel, DMA_Index + bytes, DMA_Count - bytes);
#ifdef SNDISRLOG /* v1.8: needed for debug logs only */
                DMA_Count = VDMA_GetCount( dmachannel );
#endif
            }
            /* v2.0: copy 1 more sample for cv_rate() */
            if ( resample ) {
                /* copy the next sample is the best strategy, but
                 * may be a problem if SB buffer is at its end
                 * ( especially if DSP cmd is single-cycle only );
                 * in that case, just copy the last sample!
                 * ADPCM is special, it's handled inside DecodeADPCM().
                 */
                memcpy( pDest + bytes,
                       ( bytes == SB_BuffSpace ) ?
                       pDest + bytes - samplesize * channels :
                       NearPtr(isr.DMA_linearBase + ( DMA_Base - isr.DMA_Base) + DMA_Index ),
                       samplesize * channels );
            }
        }

        /* update DSP regs */
        VSB_ReduceBuffSpace( bytes ); /* will set mixer IRQ status if space becomes <= 0 */

        /* format conversion needed? */
#if ADPCM
        if( bits < 8 )
            count = DecodeADPCM((uint8_t*)(isr.pPCM + IdxSm * 2), bytes - adpcm_state.useRef, bits );
#endif
        if( samplesize != 2 ) {
#ifdef _DEBUG
# if LOGPCM8DATA
            if ( gvars.logmode == 1 ) {
                unsigned char *tmp = (unsigned char *)isr.pPCM + IdxSm * 2;
                for ( i = 0; i < count; i++, tmp++ )
                    writepcm8data(*tmp);
            }
# endif
#endif
            cv_bits_8_to_16( isr.pPCM + IdxSm * 2, (count+1) * channels, VSB_IsSigned() ); /* converts unsigned 8-bit to signed 16-bit */
        }
#if SUP16BITUNSIGNED
        else if ( !VSB_IsSigned() )
            for ( i = IdxSm * 2, j = i + (count+1) * channels; i < j; *(isr.pPCM+i) ^= 0x8000, i++ );
#endif
        if( resample ) /* SB_Rate != freq? */
            count = cv_rate( isr.pPCM + IdxSm * 2, count * channels, channels, SB_Rate, freq );

#ifdef _DEBUG
# if LOGPCM16DATA /* log 16-bit PCM data; file logfile.asm (HDLFUNC!) must also be changed! */
        if ( gvars.logmode == 2 ) {
            short *tmp = isr.pPCM + IdxSm * 2;
            for ( i = 0; i < count; i++, tmp++ )
                writepcm16data(*tmp);
        }
# endif
#endif

        if( channels == 1) //should be the last step
            cv_channels_1_to_2( isr.pPCM + IdxSm * 2, count);

        IdxSm += count;

        if( VSB_GetIRQStatus() ) {
#ifdef SNDISRLOG
            dbgprintf(("isr(%u): s/c/b=0x%02X/0x%02X/0x%03X SB BufSpace=%u DMA Idx/Cnt=%X/%X\n", loop, nSamples, count, bytes, SB_BuffSpace, DMA_Index, DMA_Count ));
#endif
            if ( VSB_IsAuto() ) {
                VSB_ResetBuffSpace();
            } else
                VSB_Stop(); /* v1.8: does no longer reset SB position */
            VIRQ_Invoke();
        } else {
#ifdef SNDISRLOG
            dbgprintf(("isr(%u): s/c(o)/b=0x%02X/0x%02X(0x%02X)/0x%03X SB Space=0x%X DMA Idx/Cnt=%X/%X\n", loop, nSamples, count, ocnt, bytes, SB_BuffSpace, DMA_Index, DMA_Count ));
#endif
            /* v1.9: to exit the loop here (unconditionally) was incorrect -
             *       might be that DMA buffer < SB buffer!
             *       test case: Open Cubic Player.
             *       however, exit if DMA autoinit isn't active should be ok.
             * v2.0: now unconditional exit is correct - DMA underrun is handled within loop.
             */
            break;
            //if ( !VDMA_IsAuto(dmachannel) ) break;
        }
    };

    if (IdxSm) {
        /* in case there weren't enough samples copied, fill the rest with silence.
         * v1.5: it's better to reduce samples to IdxSm. If mode isn't autoinit,
         * the program may want to instantly initiate another DSP play cmd.
         * v1.8: returned to filling the rest with silence...
         * v2.0: in case there were MORE samples produced than required ( may happen
         * because of rate conversion or ADPCM ), adjust # of samples!
         */
#ifdef _DEBUG
# ifdef SNDISRLOG
        if ( IdxSm < nSamples ) dbgprintf(("isr: %u samples to add\n", nSamples - IdxSm ));
# endif
#endif
        for( i = IdxSm; i < nSamples; i++ )
            *(isr.pPCM + i*2+1) = *(isr.pPCM + i*2) = 0;

#if 1 /* TEST TEST TEST */
        /* v2.0: adjust nSamples - we don't want to loose generated sound data;
         *       the sound hardware buffers are able to handle this.
         */
        nSamples = i;
#endif

    } else if ( IdxSm = VSB_ReadDirectSamples( (uint8_t *)isr.pPCM ) ) {

        char *pDest = (char *)isr.pPCM;
        //uint32_t freq = AU_getfreq( isr.hAU );

        /* calc the src frequency by formula:
         * x / dst-freq = src-smpls / dst-smpls
         * x = src-smpl * dst-freq / dst-smpls
         */
        uint32_t SB_Rate = IdxSm * freq / nSamples;

        /* v2.0: cv_rate() now expects an extra, final sample */
        *(pDest + IdxSm) = *(pDest + IdxSm - 1);
#ifdef SNDISRLOG
        dbgprintf(("isr, direct samples: IdxSm=%d, samples=%d, rate=%u\n", IdxSm, nSamples, SB_Rate ));
#endif
        cv_bits_8_to_16( isr.pPCM, IdxSm + 1, 0 );
        IdxSm = cv_rate( isr.pPCM, IdxSm, 1, SB_Rate, freq );
        cv_channels_1_to_2( isr.pPCM, IdxSm );
        for( i = IdxSm; i < nSamples; i++ )
            *(isr.pPCM + i*2+1) = *(isr.pPCM + i*2) = 0;
    }

    /* get volumes for software mixer */

    if( gvars.type < 4) { //SB2.0 and before
        mastervol = (VSB_GetMixerReg( SB_MIXERREG_MASTERVOL) & 0xF) << 4; /* 3 bits (1-3) */
        voicevol  = (VSB_GetMixerReg( SB_MIXERREG_VOICEVOL)  & 0x7) << 5; /* 2 bits (1-2) */
        midivol   = (VSB_GetMixerReg( SB_MIXERREG_MIDIVOL)   & 0xF) << 4; /* 3 bits (1-3) */
#if VOICELR
        mastervol2 = mastervol;
        voicevol2  = voicevol;
#endif
    } else {
        /* SBPro: L&R, bits 1-3/5-7, bits 0,3=1 */
        /* SB16:  L&R, bits 0-3/4-7 */
        mastervol = VSB_GetMixerReg( SB_MIXERREG_MASTERSTEREO) & 0xF0; /* 00,10,...F0 */
        voicevol  = VSB_GetMixerReg( SB_MIXERREG_VOICESTEREO)  & 0xF0;
        midivol   = VSB_GetMixerReg( SB_MIXERREG_MIDISTEREO)   & 0xF0;
#if VOICELR
        mastervol2 = (VSB_GetMixerReg( SB_MIXERREG_MASTERSTEREO) & 0xF) << 4;
        voicevol2  = (VSB_GetMixerReg( SB_MIXERREG_VOICESTEREO) & 0xF ) << 4;
#endif
    }
#if SETABSVOL
    if( isr.SB_VOL != mastervol * gvars.vol / 9) {
        isr.SB_VOL =  mastervol * gvars.vol / 9;
        //uint8_t buffer[FPU_SRSIZE];
        //fpu_save(buffer); /* needed if AU_setmixer_one() uses floats */
        AU_setmixer_one( isr.hAU, AU_MIXCHAN_MASTER, MIXER_SETMODE_ABSOLUTE, mastervol * 100 / 256 ); /* convert to percentage 0-100 */
        //fpu_restore(buffer);
        //dbgprintf(("isr: set master volume=%u\n", SNDISR_SB_VOL ));
    }
#else
    /* min: 10*10-1=ff ; ff >> 8 = 0, max: 100*100-1=ffff ; ffff >> 8 = ff */
    voicevol = ( (voicevol | 0xF + 1) * (mastervol | 0xF + 1) - 1) >> 8;
    if ( voicevol == 0xff ) voicevol = 0x100;
    midivol  = ( (midivol  | 0xF + 1) * (mastervol | 0xF + 1) - 1) >> 8;
    if ( midivol == 0xff ) midivol = 0x100;
#endif

    /* software mixer: very simple implemented - but should work quite well */

    //if( gvars.opl3 ) {
#ifndef NOFM
    if( VOPL3_IsActive() ) {
        int channels;
        pPCMOPL = IdxSm ? isr.pPCM + nSamples * 2 : isr.pPCM;
        VOPL3_GenSamples( pPCMOPL, nSamples ); //will generate samples*2 if stereo
        //always use 2 channels
        channels = VOPL3_GetMode() ? 2 : 1;
        if( channels == 1 )
            cv_channels_1_to_2( pPCMOPL, nSamples );

        if( IdxSm ) {
# if MIXERROUTINE==0
#  if VOICELR
            voicevol2 = ( (voicevol2 | 0xF + 1) * (mastervol2 | 0xF + 1) - 1) >> 8;
            if ( voicevol2 == 0xff ) voicevol2 = 0x100;
#  endif
            for( i = 0; i < nSamples * 2; i++ ) {
                int a = (*(isr.pPCM+i) * (int)voicevol / 256) + 32768;    /* convert to 0-65535 */
                int b = (*(pPCMOPL+i) * (int)midivol / 256 ) + 32768; /* convert to 0-65535 */
                int mixed = (a < 32768 || b < 32768) ? ((a*b)/32768) : ((a+b)*2 - (a*b)/32768 - 65536);
                *(isr.pPCM+i) = (mixed > 65535 ) ? 0x7fff : mixed - 32768;
#  if VOICERL
                i++;
                a = (*(isr.pPCM+i) * (int)voicevol2 / 256) + 32768;    /* convert to 0-65535 */
                b = (*(pPCMOPL+i) * (int)midivol / 256 ) + 32768; /* convert to 0-65535 */
                mixed = (a < 32768 || b < 32768) ? ((a*b)/32768) : ((a+b)*2 - (a*b)/32768 - 65536);
                *(isr.pPCM+i) = (mixed > 65535 ) ? 0x7fff : mixed - 32768;
#  endif
            }
# elif MIXERROUTINE==1
            /* this variant is simple, but quiets too much ... */
            for( i = 0; i < nSamples * 2; i++ ) *(isr.pPCM+i) = ( *(isr.pPCM+i) * voicevol + *(pPCMOPL+i) * midivol ) >> (8+1);
# else
            /* in assembly it's probably easier to handle signed/unsigned shifts */
            SNDISR_Mixer( isr.pPCM, pPCMOPL, nSamples * 2, voicevol, midivol );
# endif
# ifdef _LOGBUFFMAX
            if ( (( pPCMOPL + nSamples * 2 ) - isr.pPCM ) * sizeof(int16_t) > isr.dwMaxBytes )
                isr.dwMaxBytes = (( pPCMOPL + nSamples * 2 ) - isr.pPCM ) * sizeof(int16_t);
# endif
        } else
            for( i = 0; i < nSamples * 2; i++, pPCMOPL++ ) *pPCMOPL = ( *pPCMOPL * midivol ) >> 8;
    } else {
#endif
        if( IdxSm ) {
# if VOICELR
            voicevol2 = ( (voicevol2 | 0xF + 1) * (mastervol2 | 0xF + 1) - 1) >> 8;
            if ( voicevol2 == 0xff ) voicevol2 = 0x100;
# endif
            for( i = 0, pPCMOPL = isr.pPCM; i < nSamples * 2; i++, pPCMOPL++ ) {
                *pPCMOPL = ( *pPCMOPL * voicevol ) >> 8;
# if VOICELR
                pPCMOPL++; i++;
                *pPCMOPL = ( *pPCMOPL * voicevol2 ) >> 8;
# endif
            }
#ifdef _LOGBUFFMAX
            if ( ( pPCMOPL - isr.pPCM ) * sizeof(int16_t) > isr.dwMaxBytes )
                isr.dwMaxBytes = (( pPCMOPL + nSamples * 2 ) - isr.pPCM ) * sizeof(int16_t);
#endif
        } else
            memset( isr.pPCM, 0, nSamples * sizeof(int16_t) * 2 );
#ifndef NOFM
    }
#endif
    //aui.samplenum = nSamples * 2;
    //aui.pcm_sample = ISR_PCM;
#if SOUNDFONT
    if (tsfrenderer) {
        unsigned char fpu_buffer[FPU_SRSIZE];
        fpu_save( fpu_buffer );
        VMPU_Process_Messages();
        //tsf_set_samplerate_output(tsfrenderer, AU_getfreq( isr.hAU ));
        tsf_render_short(tsfrenderer, isr.pPCM, nSamples, 1);
        fpu_restore( fpu_buffer );
    }
#endif
    AU_writedata( isr.hAU, isr.pPCM, nSamples * 2 );

#if SLOWDOWN
    if ( gvars.slowdown )
        delay_10us(gvars.slowdown);
#endif

isrexit:
    PIC_SendEOI( isr.SndIrq );
#if COMPAT4
    if ( gvars.compatflags & CF_MASKPIT )
        return( 2 | (mask << 8 ));
#endif
    return(1);
}

#if IRQONPORTACC
/* This function is meant to allow a sound HW IRQ if interrupts are disabled.
 * It's supposed to be called while trapped FM/MPU ports are handled.
 */
void SNDISR_IrqOnPortAcc( void )
////////////////////////////////
{
    uint16_t mask = PIC_GetIRQMask();
    PIC_SetIRQMask(mask & ~(1 << AU_getirq(isr.hAU)));
    _enable_ints();
    _disable_ints();
    PIC_SetIRQMask(mask);
    return;
}
#endif

/* init sound hw - called by main() */

bool SNDISR_Init( void *hAU, uint16_t vol )
///////////////////////////////////////////
{
#if PT0V86
#define PT0SIZE 0x1000
    uint32_t tmp;
#else
#define PT0SIZE 0
#endif
    __dpmi_meminfo info;

    /* allocate PCM buffer (def. 64k), used for format conversions */
    info.address = 0;
    info.size = ( gvars.buffsize + 1 ) * 4096;
    if (__dpmi_allocate_linear_memory( &info, 1 ) == -1 )
        return false;

    /* uncommit the page behind the buffer so a buffer overflow will cause a page fault */
    __dpmi_set_page_attr( info.handle, gvars.buffsize * 4096, 1, 0);
    isr.pPCM = NearPtr( info.address );
    dbgprintf(("SNDISR_Init: pPCM=%X\n", isr.pPCM ));

    /* allocate a 128k uncommitted region used for DMA mappings */
    info.address = 0;
    info.size = 0x20000 + 0x1000 + PT0SIZE;
    if ( __dpmi_allocate_linear_memory( &info, 0 ) == -1 )
        return false;

    isr.Block_Handle = info.handle;
    isr.Block_Addr   = info.address;

#if PT0V86
    /* v1.8: get phys. address of VCPI host's page table 0 and map it into
     * protected-mode address space. This allows to access physical addresses
     * within the v86 conventional address space (EMS page frame).
     */
    if ( tmp = PTRAP_GetPageTab0v86() ) {
        if( __dpmi_map_physical_device(isr.Block_Handle, 0x20000 + 0x1000, 1, tmp ) == 0 ) {
            __dpmi_set_page_attr(isr.Block_Handle, 0x20000 + 0x1000, 1, 3 ); /* 3 = set page to r/o */
            isr.PageTab0v86 = info.address + 0x20000 + 0x1000;
            dbgprintf(("SNDISR_Init: v86 PT0=%X mapped at %X\n", tmp, isr.PageTab0v86 ));
        }
    }
#endif
    isr.hAU = hAU;
    isr.SndIrq = AU_getirq( hAU );

#if SETABSVOL
    isr.SB_VOL = vol;
#endif
    return _SND_InstallISR( PIC_IRQ2VEC( AU_getirq( hAU ) ), &SNDISR_Interrupt );
}

bool SNDISR_Exit( void )
////////////////////////
{
#ifdef _LOGBUFFMAX
    printf("SNDISR_Exit: max PCM buffer usage=%u\n", isr.dwMaxBytes );
#endif
#ifdef _DEBUG
    printf("SNDISR_Exit: cnt total/voice=%u/%u max/avg samples=%u/%u\n", isr.cntTotal, isr.cntDigital, isr.max_samples, isr.cntTotal ? isr.total_samples / isr.cntTotal : 0 );
#endif
    return ( _SND_UninstallISR( PIC_IRQ2VEC( AU_getirq( isr.hAU ) ) ) );
}



