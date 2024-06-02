
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

#if DISPSTAT
#include <stdio.h>
#endif

#include "AU.H"

#define SUP16BITUNSIGNED 1 /* support 16-bit unsigned format */

#define MIXERROUTINE 0

bool _SND_InstallISR( uint8_t, int(*ISR)(void) );
bool _SND_UninstallISR( uint8_t );

extern void SNDISR_Mixer( uint16_t *, uint16_t *, uint32_t, uint32_t, uint32_t );
extern void fatal_error( int );

extern int hAU;
extern struct globalvars gvars;
#if SETABSVOL
uint16_t SNDISR_SB_VOL;
#endif

struct SNDISR_s {
	int16_t *pPCM;
	uint32_t DMA_linearBase; /* linear start address of current DMA buffer */
	uint32_t DMA_Base;       /* (physical) base address of DMA buffer at last remapping */
	uint32_t DMA_Size;       /* size of DMA buffer at last remapping */
	uint32_t Block_Handle;   /* handle of remapping block */
	uint32_t Block_Addr;     /* linear base of remapping block ( page aligned ) */
#ifdef _LOGBUFFMAX /* log the usage of the PCM buffer? */
	uint32_t dwMaxBytes;
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
 * src & dst are 16-bit
 * out: new sample cnt
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
	unsigned int inpos = 0;//(srcrate < dstrate) ? instep/2 : 0;
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

	do{
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
		do{
			*pcmdst++= ((*intmp1++) * m1 + (*intmp2++) * m2) / 4096;// >> 12; //don't use shift, signed right shift impl defined, maybe logical shift
		}while (--ch);
		inpos += instep;
	}while( inpos < inend );

	//dbgprintf(("cv_rate(src/dst rates=%u/%u chn=%u smpl=%u step=%x end=%x)=%u\n", srcrate, dstrate, channels, samplenum, instep, inend, pcmdst - pcmsrc ));

#if !MALLOCSTATIC
	free(buff);
#endif
	return pcmdst - pcmsrc;
}

/* convert 8 to 16 bits. It's assumed that 8 bit is unsigned, 16-bit is signed */

static void cv_bits_8_to_16( PCM_CV_TYPE_S *pcm, unsigned int samplenum )
/////////////////////////////////////////////////////////////////////////
{
	PCM_CV_TYPE_UC *src = (PCM_CV_TYPE_UC *)pcm + samplenum - 1;
	PCM_CV_TYPE_S *dst = pcm + samplenum - 1;

	for ( ; samplenum; samplenum-- ) {
		*dst-- = (PCM_CV_TYPE_S)((*src-- ^ 0x80) << 8);
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
    int16_t* pPCMOPL;
    uint8_t* pDirect;
    int samples;
    bool digital;
    int i;

    /* check if the sound hw does request an interrupt. */
    if( !AU_isirq( hAU ) )
        return(0);

#if SETIF
    _enable_ints();
#endif

    /* since the client context is now restored when a SB IRQ is emulated,
     * it's safe to call VIRQ_Invoke here.
     */
    if ( VSB_GetIRQStatus() )
        VIRQ_Invoke();

    if( gvars.type < 4) { //SB2.0 and before
        mastervol = (VSB_GetMixerReg( SB_MIXERREG_MASTERVOL) & 0xF) << 4; /* 3 bits (1-3) */
        voicevol  = (VSB_GetMixerReg( SB_MIXERREG_VOICEVOL)  & 0x7) << 5; /* 2 bits (1-2) */
        midivol   = (VSB_GetMixerReg( SB_MIXERREG_MIDIVOL)   & 0xF) << 4; /* 3 bits (1-3) */
    } else {
        /* SBPro: L&R, bits 1-3/5-7, bits 0,3=1 */
        /* SB16:  L&R, bits 0-3/4-7 */
        mastervol = VSB_GetMixerReg( SB_MIXERREG_MASTERSTEREO) & 0xF0; /* 00,10,...F0 */
        voicevol  = VSB_GetMixerReg( SB_MIXERREG_VOICESTEREO)  & 0xF0;
        midivol   = VSB_GetMixerReg( SB_MIXERREG_MIDISTEREO)   & 0xF0;
    }
#if SETABSVOL
    if( SNDISR_SB_VOL != mastervol * gvars.vol / 9) {
        SNDISR_SB_VOL =  mastervol * gvars.vol / 9;
        //uint8_t buffer[200];
        //fpu_save(buffer); /* needed if AU_setmixer_one() uses floats */
        AU_setmixer_one( hAU, AU_MIXCHAN_MASTER, MIXER_SETMODE_ABSOLUTE, mastervol * 100 / 256 ); /* convert to percentage 0-100 */
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
    AU_setoutbytes( hAU ); //aui.card_outbytes = aui.card_dmasize;
    samples = AU_cardbuf_space( hAU ) / sizeof(int16_t) / 2; //16 bit, 2 channels
    //dbgprintf(("isr: samples:%u ",samples));

    if(samples == 0) { /* no free space in DMA buffer? */
        PIC_SendEOI( AU_getirq( hAU ) );
        return(1);
    }

    digital = VSB_Running();

    if( digital ) {
        int i,j;
        int dmachannel = VSB_GetDMA();
        uint32_t freq = AU_getfreq( hAU );
        int samplesize = max( 1, VSB_GetBits() / 8 );
        int channels = VSB_GetChannels();
        int IdxSm = 0; /* sample index in 16bit PCM buffer */
        int count; /* samples to handle in this turn */
        bool resample; //don't resample if sample rates are close
        int bytes;

        /* a while loop that may run 2 times if a SB buffer overrun occured */
        do {
            uint32_t DMA_Base = VDMA_GetBase(dmachannel);
            uint32_t DMA_Index = VDMA_GetIndex(dmachannel);
            int32_t DMA_Count = VDMA_GetCount(dmachannel);
            uint32_t SB_BuffSize = VSB_GetSampleBufferSize(); /* buffer size in bytes */
            uint32_t SB_Pos = VSB_GetPos();
            uint32_t SB_Rate = VSB_GetSampleRate();
            int IsSilent = VSB_IsSilent();
            if ( IsSilent ) {
                DMA_Count = SB_BuffSize;
            } else {
                /* check if the current DMA buffer is within the mapped region. */
                if( !(DMA_Base >= isr.DMA_Base && (DMA_Base + DMA_Index + DMA_Count) <= (isr.DMA_Base + isr.DMA_Size) )) {
                    isr.DMA_linearBase = -1;
                }
                /* if there's no mapped region, create one that covers current DMA op. */
                if( isr.DMA_linearBase == -1 ) {
                    isr.DMA_Base = DMA_Base;
                    isr.DMA_Size = min( max(DMA_Index + DMA_Count, 0x4000 ), 0x20000 );
                    if ( DMA_Base < 0x100000 )
                        isr.DMA_linearBase = DMA_Base;
                    else {
                        /* size is in pages, phys. address must have bits 0-11 cleared */
                        if( __dpmi_map_physical_device(isr.Block_Handle, 0, (isr.DMA_Size + (isr.DMA_Base & 0xfff) + 4095 ) >> 12 , isr.DMA_Base & ~0xfff ) == -1 )
                            fatal_error( 2 );
                        isr.DMA_linearBase = isr.Block_Addr | (isr.DMA_Base & 0xFFF);
                    }
                    dbgprintf(("isr, ISR_DMA address (re)mapped: isr.DMA_Base=%x, isr.DMA_Size=%x, isr.DMA_linearBase=%x\n", isr.DMA_Base, isr.DMA_Size, isr.DMA_linearBase ));
                }
            }
            count = samples - IdxSm;
            /* don't resample if sample rates are close.
             */
            resample = true;
            if( SB_Rate < freq )
                //count = max( channels, count / ( ( freq + SB_Rate-1) / SB_Rate ));
                count = max( 1, count * SB_Rate / freq );
            else if( SB_Rate > freq )
                //count *= (SB_Rate + aui.freq_card/2)/aui.freq_card;
                count = count * SB_Rate / freq;
            else
                resample = false;
            /* ensure count won't exceed DMA buffer def */
            count = min( count, max(1,(DMA_Count) / samplesize / channels));
            /* ensure count won't exceed SB buffer def */
            count = min( count, max(1,(SB_BuffSize - SB_Pos) / samplesize / channels ));
            /* adjust count if sample size is < 8 (ADPCM) */
            if( VSB_GetBits() < 8 )
                count = max(1, count / (9 / VSB_GetBits()));
            bytes = count * samplesize * channels;

            /* copy samples to our PCM buffer */
            if( isr.DMA_linearBase == -1 || IsSilent ) {//map failed?
                memset( isr.pPCM + IdxSm * 2, 0, bytes);
            } else
                memcpy( isr.pPCM + IdxSm * 2, NearPtr(isr.DMA_linearBase + ( DMA_Base - isr.DMA_Base) + DMA_Index ), bytes );

            /* format conversion needed? */
#if ADPCM
            if( VSB_GetBits() < 8)
                count = DecodeADPCM((uint8_t*)(isr.pPCM + IdxSm * 2), bytes);
#endif
            if( samplesize != 2 )
                cv_bits_8_to_16( isr.pPCM + IdxSm * 2, count * channels ); /* converts unsigned 8-bit to signed 16-bit */
#if SUP16BITUNSIGNED
            else if ( !VSB_IsSigned() )
                for ( i = IdxSm * 2, j = i + count * channels; i < j; *(isr.pPCM+i) ^= 0x8000, i++ );
#endif
            if( resample ) /* SB_Rate != freq */
                count = cv_rate( isr.pPCM + IdxSm * 2, count * channels, channels, SB_Rate, freq ) / channels;
            if( channels == 1) //should be the last step
                cv_channels_1_to_2( isr.pPCM + IdxSm * 2, count);

            /* conversion done; now set new values for DMA and SB buffer;
             * in case the end of SB buffer is reached, emulate an interrupt
             * and run this loop a second time.
             */
            IdxSm += count;
            //dbgprintf(("isr: samples:%d %d %d\n", count, IdxSm, samples));
            if ( !IsSilent ) {
                DMA_Index = VDMA_SetIndexCount(dmachannel, DMA_Index + bytes, DMA_Count - bytes);
                DMA_Count = VDMA_GetCount( dmachannel );
            }
            SB_Pos = VSB_SetPos( SB_Pos + bytes ); /* will set mixer IRQ status if pos beyond buffer */
            if( VSB_GetIRQStatus() ) {
                dbgprintf(("isr: Pos/BuffSize=0x%X/0x%X samples/count=%u/%u bytes=%u dmaIdx/Cnt=%X/%X\n", SB_Pos, SB_BuffSize, samples, count, bytes, DMA_Index, DMA_Count ));
                if(!VSB_GetAuto())
                    VSB_Stop();
                VSB_SetPos(0); /* */
                VIRQ_Invoke();
            }
#ifdef _DEBUG
            else dbgprintf(("isr: Pos/BuffSize=0x%X/0x%X bytes=%u silent=%u\n", SB_Pos, SB_BuffSize, bytes, IsSilent ));
#endif
        } while(VDMA_GetAuto(dmachannel) && (IdxSm < samples) && VSB_Running());

        /* in case there weren't enough samples copied, fill the rest with silence.
         * v1.5: it's better to reduce samples to IdxSm. If mode isn't autoinit,
         * the program may want to instantly initiate another DSP play cmd.
         */
#if 0
        for( i = IdxSm; i < samples; i++ )
            *(isr.pPCM + i*2+1) = *(isr.pPCM + i*2) = 0;
#else
        samples = IdxSm;
#endif

    } else if ( i = VSB_GetDirectCount( &pDirect ) ) {

        int count = i;
        uint32_t freq = AU_getfreq( hAU );

        /* calc the src frequency by formula:
         * x / dst-freq = src-smpls / dst-smpls
         * x = src-smpl * dst-freq / dst-smpls
         */
        uint32_t SB_Rate = count * freq / samples;

        //dbgprintf(("isr, direct samples: cnt=%d, samples=%d, rate%u\n", count, samples, SB_Rate ));
        memcpy( isr.pPCM, pDirect, count );
        cv_bits_8_to_16( isr.pPCM, count );
        count = cv_rate( isr.pPCM, count, 1, SB_Rate, freq );
        cv_channels_1_to_2( isr.pPCM, count );
        for( i = count; i < samples; i++ )
            *(isr.pPCM + i*2+1) = *(isr.pPCM + i*2) = 0;
        VSB_ResetDirectCount();
        digital = true;
    }

    /* software mixer: very simple implemented - but should work quite well */

    //if( gvars.opl3 ) {
#ifndef NOFM
    if( VOPL3_IsActive() ) {
        int channels;
        pPCMOPL = digital ? isr.pPCM + samples * 2 : isr.pPCM;
        VOPL3_GenSamples( pPCMOPL, samples ); //will generate samples*2 if stereo
        //always use 2 channels
        channels = VOPL3_GetMode() ? 2 : 1;
        if( channels == 1 )
            cv_channels_1_to_2( pPCMOPL, samples );

        if( digital ) {
#if MIXERROUTINE==0
            for( i = 0; i < samples * 2; i++ ) {
                int a = (*(isr.pPCM+i) * (int)voicevol / 256) + 32768;    /* convert to 0-65535 */
                int b = (*(pPCMOPL+i) * (int)midivol / 256 ) + 32768; /* convert to 0-65535 */
                int mixed = (a < 32768 || b < 32768) ? ((a*b)/32768) : ((a+b)*2 - (a*b)/32768 - 65536);
                *(isr.pPCM+i) = (mixed > 65535 ) ? 0x7fff : mixed - 32768;
            }
#elif MIXERROUTINE==1
            /* this variant is simple, but quiets too much ... */
            for( i = 0; i < samples * 2; i++ ) *(isr.pPCM+i) = ( *(isr.pPCM+i) * voicevol + *(pPCMOPL+i) * midivol ) >> (8+1);
#else
            /* in assembly it's probably easier to handle signed/unsigned shifts */
            SNDISR_Mixer( isr.pPCM, pPCMOPL, samples * 2, voicevol, midivol );
#endif
#ifdef _LOGBUFFMAX
            if ( (( pPCMOPL + samples * 2 ) - isr.pPCM ) * sizeof(int16_t) > isr.dwMaxBytes )
                isr.dwMaxBytes = (( pPCMOPL + samples * 2 ) - isr.pPCM ) * sizeof(int16_t);
#endif
        } else
            for( i = 0; i < samples * 2; i++, pPCMOPL++ ) *pPCMOPL = ( *pPCMOPL * midivol ) >> 8;
    } else {
#endif
        if( digital ) {
            for( i = 0, pPCMOPL = isr.pPCM; i < samples * 2; i++, pPCMOPL++ ) *pPCMOPL = ( *pPCMOPL * voicevol ) >> 8;
            //dbgprintf(("+"));
#ifdef _LOGBUFFMAX
            if ( (( pPCMOPL + samples * 2 ) - isr.pPCM ) * sizeof(int16_t) > isr.dwMaxBytes )
                isr.dwMaxBytes = (( pPCMOPL + samples * 2 ) - isr.pPCM ) * sizeof(int16_t);
#endif
        } else
            memset( isr.pPCM, 0, samples * sizeof(int16_t) * 2 );
#ifndef NOFM
    }
#endif
    //aui.samplenum = samples * 2;
    //aui.pcm_sample = ISR_PCM;
    AU_writedata( hAU, samples * 2, isr.pPCM );

#if DISPSTAT
    if ( VSB_GetDispStat() ) printf("SNDISR_Interrupt: samples=%u, digital=%u\n", samples, digital );
#endif

    PIC_SendEOI( AU_getirq( hAU ) );

#if SLOWDOWN
    if ( gvars.slowdown )
        delay_10us(gvars.slowdown);
#endif

    return(1);
}

bool SNDISR_Init( uint8_t intno )
/////////////////////////////////
{
    __dpmi_meminfo info;
    info.address = 0;
    info.size = ( gvars.buffsize + 1 ) * 4096;
    if (__dpmi_allocate_linear_memory( &info, 1 ) == -1 )
        return false;

    /* uncommit the page behind the buffer so a buffer overflow will cause a page fault */
    __dpmi_set_page_attr( info.handle, gvars.buffsize * 4096, 1, 0);
    isr.pPCM = NearPtr( info.address );
    dbgprintf(("SNDISR_InstallISR: pPCM=%X\n", isr.pPCM ));

    info.address = 0;
    info.size = 0x20000 + 0x1000;
    if ( __dpmi_allocate_linear_memory( &info, 0 ) == -1 )
        return false;

    isr.Block_Handle = info.handle;
    isr.Block_Addr   = info.address;

    return _SND_InstallISR( intno, &SNDISR_Interrupt );
}

bool SNDISR_Exit( intno )
/////////////////////////
{
#ifdef _LOGBUFFMAX
    printf("max PCM buffer usage: %u\n", isr.dwMaxBytes );
#endif
    return ( _SND_UninstallISR( intno ) );
}



