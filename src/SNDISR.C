
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
#include "CTADPCM.H"
#include "PTRAP.H"

#if DISPSTAT
#include <stdio.h>
#endif

#ifdef _DEBUG
//#define SNDISRLOG
#endif

#include "AU.H"

#if SOUNDFONT
#include "VMPU.H"
//#include "../tsf/TSF.H"
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

#if ADPCM

ADPCM_STATE ISR_adpcm_state;

static int DecodeADPCM(uint8_t *adpcm, int bytes)
/////////////////////////////////////////////////
{
    int start = 0;
    int i;
    int bits = VSB_GetBits();
    int outbytes;
    int outcount = 0;
    uint8_t* pcm;

    if( ISR_adpcm_state.useRef ) {
        ISR_adpcm_state.useRef = false;
        ISR_adpcm_state.ref = *adpcm;
        ISR_adpcm_state.step = 0;
        start = 1;
    }

    /* bits may be 2,3,4 -> outbytes = bytes * 4,3,2 */
    outbytes = bytes * ( 9 / bits );
    pcm = (uint8_t*)malloc( outbytes );
    dbgprintf(("DecodeADPCM( %X, %u ): malloc(%u)=%X, bits=%u\n", adpcm, bytes, outbytes, pcm, bits ));

    switch ( bits ) {
    case 2:
        for( i = start; i < bytes; ++i) {
            pcm[outcount++] = decode_ADPCM_2_sample((adpcm[i] >> 6) & 0x3, &ISR_adpcm_state.ref, &ISR_adpcm_state.step);
            pcm[outcount++] = decode_ADPCM_2_sample((adpcm[i] >> 4) & 0x3, &ISR_adpcm_state.ref, &ISR_adpcm_state.step);
            pcm[outcount++] = decode_ADPCM_2_sample((adpcm[i] >> 2) & 0x3, &ISR_adpcm_state.ref, &ISR_adpcm_state.step);
            pcm[outcount++] = decode_ADPCM_2_sample((adpcm[i] >> 0) & 0x3, &ISR_adpcm_state.ref, &ISR_adpcm_state.step);
        }
        break;
    case 3:
        for( i = start; i < bytes; ++i) {
            pcm[outcount++] = decode_ADPCM_3_sample((adpcm[i] >> 5) & 0x7, &ISR_adpcm_state.ref, &ISR_adpcm_state.step);
            pcm[outcount++] = decode_ADPCM_3_sample((adpcm[i] >> 2) & 0x7, &ISR_adpcm_state.ref, &ISR_adpcm_state.step);
            pcm[outcount++] = decode_ADPCM_3_sample((adpcm[i] & 0x3) << 1, &ISR_adpcm_state.ref, &ISR_adpcm_state.step);
        }
        break;
    default:
        for( i = start; i < bytes; ++i) {
            pcm[outcount++] = decode_ADPCM_4_sample(adpcm[i] >> 4,  &ISR_adpcm_state.ref, &ISR_adpcm_state.step);
            pcm[outcount++] = decode_ADPCM_4_sample(adpcm[i] & 0xf, &ISR_adpcm_state.ref, &ISR_adpcm_state.step);
        }
        break;
    }
    //assert(outcount <= outbytes);
    dbgprintf(("DecodeADPCM: outcount=%u\n", outcount ));
    memcpy( adpcm, pcm, outcount );
    free(pcm);
    return outcount;
}
#endif

/* rate conversion.
 * src & dst are 16-bit, channels is either 1 or 2; if it's 2, samplenum is even!
 * out: new sample cnt.
 * example with 16 samples, 1 channel, srcrate=4410, dstrate=44100:
 * 1. instep = (0 << 12) | (((4096 * ( 4410 % 44100 ) + 44100 - 1 ) / 44100) & 0xfff)
 *           = (( 4096 * 4410 + 44100 - 1 ) / 44100 ) & 0xfff
 *           = ( 181.074.459 / 44100 ) & 0xfff
 *           = 410 & 0xfff -> 410
 * 2. inend  = ( 16 / 1 ) << 12  -> 65536
 * 3. do {} while loop: 65536 / 410 = 159 (interpolation steps)
 *
 * problem is same as with ADPCM: the last sample(s) should be saved and used as first
 * in the next call...
 */

static unsigned int cv_rate( PCM_CV_TYPE_S *pcmsrc, unsigned int samplenum, unsigned int channels, unsigned int srcrate, unsigned int dstrate)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	/* todo: what algorithm for instep is best? */
	//const unsigned int instep = ((srcrate / dstrate) << 12) | (((4096 * (srcrate % dstrate) - 1) / (dstrate - 1)) & 0xFFF);
	const unsigned int instep = ((srcrate / dstrate) << 12) | (((4096 * (srcrate % dstrate) + dstrate - 1 ) / dstrate) & 0xFFF);

	const unsigned int inend = (samplenum / channels) << 12;
	PCM_CV_TYPE_S *pcmdst;
	unsigned long ipi;
	unsigned int inpos = 0;//(srcrate < dstrate) ? instep / 2 : 0;
	int total;
#if MALLOCSTATIC
	static int maxsample = 0;
	static PCM_CV_TYPE_S* buff = NULL;
#else
	PCM_CV_TYPE_S* buff;
#endif

	if(!samplenum)
		return 0;

#if MALLOCSTATIC
	if ( samplenum > maxsample ) {
		if ( buff )
			free( buff );
		buff = (PCM_CV_TYPE_S*)malloc(samplenum * sizeof(PCM_CV_TYPE_S));
		maxsample = samplenum;
	}
#else
	buff = (PCM_CV_TYPE_S*)malloc(samplenum * sizeof(PCM_CV_TYPE_S));
#endif
	memcpy( buff, pcmsrc, samplenum * sizeof(PCM_CV_TYPE_S) );

	pcmdst = pcmsrc;
	total = samplenum / channels;

	do {
		int m1,m2;
		unsigned int ipi,ch;
		PCM_CV_TYPE_S *intmp1,*intmp2;
		ipi = inpos >> 12;
		m2 = inpos & 0xFFF;
		m1 = 4096 - m2;
		ch = channels;
		ipi *= ch;
		intmp1 = buff + ipi;
		intmp2 = ipi < total - ch ? intmp1 + ch : intmp1;
		do {
			*pcmdst++= ((*intmp1++) * m1 + (*intmp2++) * m2) / 4096;// >> 12; //don't use shift, signed right shift impl defined, maybe logical shift
		} while (--ch);
		inpos += instep;
	} while ( inpos < inend );

	//dbgprintf(("cv_rate(src/dst rates=%u/%u chn=%u smpl=%u step=%x end=%x)=%u\n", srcrate, dstrate, channels, samplenum, instep, inend, pcmdst - pcmsrc ));

#if !MALLOCSTATIC
	free(buff);
#endif
	return pcmdst - pcmsrc;
}

/* convert 8 to 16 bits. It's assumed that 8 bit is unsigned, 16-bit is signed */

static void cv_bits_8_to_16( PCM_CV_TYPE_S *pcm, unsigned int samplenum, uint8_t issigned )
///////////////////////////////////////////////////////////////////////////////////////////
{
	PCM_CV_TYPE_UC *srcu;
	PCM_CV_TYPE_SC *srcs;
	PCM_CV_TYPE_S *dst = pcm + samplenum - 1;

    if ( issigned ) {
        srcs = (PCM_CV_TYPE_SC *)pcm + samplenum - 1;
        for ( ; samplenum; samplenum-- )
            *dst-- = (PCM_CV_TYPE_S)((*srcs--) << 8);
    } else {
        srcu = (PCM_CV_TYPE_UC *)pcm + samplenum - 1;
        for ( ; samplenum; samplenum-- )
            *dst-- = (PCM_CV_TYPE_S)((*srcu-- ^ 0x80) << 8);
    }
}

/* convert mono to stereo. */

static void cv_channels_1_to_2( PCM_CV_TYPE_S *pcm_sample, unsigned int samplenum )
///////////////////////////////////////////////////////////////////////////////////
{
    PCM_CV_TYPE_S *src = pcm_sample + samplenum - 1;
    PCM_CV_TYPE_S *dst = pcm_sample + samplenum * 2 - 1;

    for( ; samplenum; samplenum-- ) {
        *dst-- = *src; *dst-- = *src--;
    }
    return;
}

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
    uint8_t* pDirect;
    uint32_t freq;
    int samples;
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
    if (gvars.compatflags & 4 ) {
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

    AU_setoutbytes( isr.hAU ); //aui.card_outbytes = aui.card_dmasize;
    samples = AU_cardbuf_space( isr.hAU ) / ( sizeof(int16_t) * 2 ); //16 bit, 2 channels
    if ( !samples ) { /* no free space in DMA buffer? Shouldn't happen... */
        PIC_SendEOI( isr.SndIrq );
        return(1);
    }
    freq = AU_getfreq( isr.hAU );
#ifdef _DEBUG
    if (samples > isr.max_samples)
        isr.max_samples = samples;
    isr.total_samples += samples;
    isr.cntTotal++;
    //dbgprintf(("isr: samples:%u ",samples));
    loop = 0;
    for ( IdxSm = 0, isr.cntDigital++; VSB_Running() && IdxSm < samples; loop++ ) {
        int ocnt;
#else
    for ( IdxSm = 0; VSB_Running() && IdxSm < samples; ) {
#endif
        /* a loop that may run 2 (or multiple) times if a SB buffer overrun occured */
        int i,j;
        int dmachannel = VSB_GetDMA();
        int samplesize = max( 1, VSB_GetBits() / 8 );
        int count = samples - IdxSm; /* samples to handle in this turn */
        bool resample;
        int bytes;
        int channels = VSB_GetChannels();
        uint32_t DMA_Base;
        uint32_t DMA_Index;
        int32_t DMA_Count;
        uint32_t SB_BuffSize = VSB_GetSampleBufferSize(); /* buffer size in bytes */
        uint32_t SB_Pos = VSB_GetPos();
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
            resample = true;
            //count = max( channels, count / ( ( freq + SB_Rate-1) / SB_Rate ));
            count = count * SB_Rate / freq;
            if ( SB_Rate < freq && SB_Rate % freq ) count++;
        } else
            resample = false;
#ifdef _DEBUG
        ocnt = count;
#endif
        /* ensure count won't exceed DMA buffer size;
         * v1.8: check DMA only if not silent;
         */
        if (!IsSilent) {
            /* adjust count if sample size is < 8 (ADPCM) */
            if( VSB_GetBits() < 8 )
                count = count / (9 / VSB_GetBits());
            if ( DMA_Count < (samplesize * channels )) {
                dbgprintf(("isr(%u): DMA_Count=0x%X, samplesize=%u, channels=%u\n", loop, DMA_Count, samplesize, channels ));
                /* v1.8: if dma autoinit then restart dma; else exit & stop digital sound. */
                if ( !VDMA_GetAuto(dmachannel) || DMA_Index == 0 ) {
                    VSB_Stop();
                    break;
                }
                DMA_Index = VDMA_SetIndexCount(dmachannel, DMA_Index, 0 );
                DMA_Count = VDMA_GetCount(dmachannel );
            }
            /* v1.8: removed max() */
            //count = min( count, max(1, DMA_Count / (samplesize * channels) ) );
            count = min( count, DMA_Count / (samplesize * channels) );
        }
        count = min( count, max(1,(SB_BuffSize - SB_Pos) / (samplesize * channels) ) );

        bytes = count * samplesize * channels;

        /* copy samples to our PCM buffer */
        if( IsSilent ) {
            memset( isr.pPCM + IdxSm * 2, 0, bytes);
        } else {
            memcpy( isr.pPCM + IdxSm * 2, NearPtr(isr.DMA_linearBase + ( DMA_Base - isr.DMA_Base) + DMA_Index ), bytes );
            DMA_Index = VDMA_SetIndexCount(dmachannel, DMA_Index + bytes, DMA_Count - bytes);
            //DMA_Count = VDMA_GetCount( dmachannel ); /* v1.8: not needed */
        }

        /* update DSP regs */
        SB_Pos = VSB_SetPos( SB_Pos + bytes ); /* will set mixer IRQ status if pos beyond buffer */

        /* format conversion needed? */
#if ADPCM
        if( VSB_GetBits() < 8)
            count = DecodeADPCM((uint8_t*)(isr.pPCM + IdxSm * 2), bytes);
#endif
        if( samplesize != 2 )
            cv_bits_8_to_16( isr.pPCM + IdxSm * 2, count * channels, VSB_IsSigned() ); /* converts unsigned 8-bit to signed 16-bit */
#if SUP16BITUNSIGNED
        else if ( !VSB_IsSigned() )
            for ( i = IdxSm * 2, j = i + count * channels; i < j; *(isr.pPCM+i) ^= 0x8000, i++ );
#endif
        if( resample ) /* SB_Rate != freq */
            count = cv_rate( isr.pPCM + IdxSm * 2, count * channels, channels, SB_Rate, freq ) / channels;
        if( channels == 1) //should be the last step
            cv_channels_1_to_2( isr.pPCM + IdxSm * 2, count);

        IdxSm += count;

        if( VSB_GetIRQStatus() ) {
#ifdef SNDISRLOG
            dbgprintf(("isr(%u): SB Pos/Size=0x%X/0x%X s/c/b=0x%X/0x%X/0x%X DMA Idx/Cnt=%X/%X\n", loop, SB_Pos, SB_BuffSize, samples, count, bytes, DMA_Index, DMA_Count ));
#endif
            if ( VSB_GetAuto() )
                VSB_SetPos(0);
            else
                VSB_Stop(); /* v1.8: does no longer reset SB position */
            VIRQ_Invoke();
        } else {
#ifdef SNDISRLOG
            dbgprintf(("isr(%u): s/c/b=0x%X/0x%X/0x%X ocnt=0x%X SB Pos=0x%X ESP=0x%X\n", loop, samples, count, bytes, ocnt, SB_Pos, _my_esp() ));
#endif
            break;
        }
    };

    if (IdxSm) {
        /* in case there weren't enough samples copied, fill the rest with silence.
         * v1.5: it's better to reduce samples to IdxSm. If mode isn't autoinit,
         * the program may want to instantly initiate another DSP play cmd.
         * v1.8: returned to filling the rest with silence...
         */
#if 1
        for( i = IdxSm; i < samples; i++ )
            *(isr.pPCM + i*2+1) = *(isr.pPCM + i*2) = 0;
#else
        samples = IdxSm;
#endif
    } else if ( IdxSm = VSB_GetDirectCount( &pDirect ) ) {

        //uint32_t freq = AU_getfreq( isr.hAU );

        /* calc the src frequency by formula:
         * x / dst-freq = src-smpls / dst-smpls
         * x = src-smpl * dst-freq / dst-smpls
         */
        uint32_t SB_Rate = IdxSm * freq / samples;

        memcpy( isr.pPCM, pDirect, IdxSm );
        VSB_ResetDirectCount();
        //dbgprintf(("isr, direct samples: IdxSm=%d, samples=%d, rate=%u\n", IdxSm, samples, SB_Rate ));
        cv_bits_8_to_16( isr.pPCM, IdxSm, 0 );
        IdxSm = cv_rate( isr.pPCM, IdxSm, 1, SB_Rate, freq );
        cv_channels_1_to_2( isr.pPCM, IdxSm );
        for( i = IdxSm; i < samples; i++ )
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
        pPCMOPL = IdxSm ? isr.pPCM + samples * 2 : isr.pPCM;
        VOPL3_GenSamples( pPCMOPL, samples ); //will generate samples*2 if stereo
        //always use 2 channels
        channels = VOPL3_GetMode() ? 2 : 1;
        if( channels == 1 )
            cv_channels_1_to_2( pPCMOPL, samples );

        if( IdxSm ) {
# if MIXERROUTINE==0
#  if VOICELR
            voicevol2 = ( (voicevol2 | 0xF + 1) * (mastervol2 | 0xF + 1) - 1) >> 8;
            if ( voicevol2 == 0xff ) voicevol2 = 0x100;
#  endif
            for( i = 0; i < samples * 2; i++ ) {
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
            for( i = 0; i < samples * 2; i++ ) *(isr.pPCM+i) = ( *(isr.pPCM+i) * voicevol + *(pPCMOPL+i) * midivol ) >> (8+1);
# else
            /* in assembly it's probably easier to handle signed/unsigned shifts */
            SNDISR_Mixer( isr.pPCM, pPCMOPL, samples * 2, voicevol, midivol );
# endif
# ifdef _LOGBUFFMAX
            if ( (( pPCMOPL + samples * 2 ) - isr.pPCM ) * sizeof(int16_t) > isr.dwMaxBytes )
                isr.dwMaxBytes = (( pPCMOPL + samples * 2 ) - isr.pPCM ) * sizeof(int16_t);
# endif
        } else
            for( i = 0; i < samples * 2; i++, pPCMOPL++ ) *pPCMOPL = ( *pPCMOPL * midivol ) >> 8;
    } else {
#endif
        if( IdxSm ) {
# if VOICELR
            voicevol2 = ( (voicevol2 | 0xF + 1) * (mastervol2 | 0xF + 1) - 1) >> 8;
            if ( voicevol2 == 0xff ) voicevol2 = 0x100;
# endif
            for( i = 0, pPCMOPL = isr.pPCM; i < samples * 2; i++, pPCMOPL++ ) {
                *pPCMOPL = ( *pPCMOPL * voicevol ) >> 8;
# if VOICELR
                pPCMOPL++; i++;
                *pPCMOPL = ( *pPCMOPL * voicevol2 ) >> 8;
# endif
            }
#ifdef _LOGBUFFMAX
            if ( ( pPCMOPL - isr.pPCM ) * sizeof(int16_t) > isr.dwMaxBytes )
                isr.dwMaxBytes = (( pPCMOPL + samples * 2 ) - isr.pPCM ) * sizeof(int16_t);
#endif
        } else
            memset( isr.pPCM, 0, samples * sizeof(int16_t) * 2 );
#ifndef NOFM
    }
#endif
    //aui.samplenum = samples * 2;
    //aui.pcm_sample = ISR_PCM;
#if SOUNDFONT
    if (tsfrenderer) {
        unsigned char fpu_buffer[FPU_SRSIZE];
        fpu_save( fpu_buffer );
        VMPU_Process_Messages();
        //tsf_set_samplerate_output(tsfrenderer, AU_getfreq( isr.hAU ));
        tsf_render_short(tsfrenderer, isr.pPCM, samples, 1);
        fpu_restore( fpu_buffer );
    }
#endif
    AU_writedata( isr.hAU, samples * 2, isr.pPCM );

#if SLOWDOWN
    if ( gvars.slowdown )
        delay_10us(gvars.slowdown);
#endif
    PIC_SendEOI( isr.SndIrq );
#if COMPAT4
    if ( gvars.compatflags & 4 )
        return( 2 | (mask << 8 ));
#endif

    return(1);
}

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



