
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

#include "AU.H"

#define SUP16BITUNSIGNED 1 /* support 16-bit unsigned format */

#define MIXERROUTINE 0

bool _hdpmi_InstallISR( uint8_t i, int(*ISR)(void) );
bool _hdpmi_UninstallISR( void );
bool _hdpmi_InstallInt31( uint8_t );
bool _hdpmi_UninstallInt31( void );

extern void SNDISR_Mixer( uint16_t *, uint16_t *, uint32_t, uint32_t, uint32_t );
extern void fatal_error( int );

extern int hAU;
extern struct globalvars gvars;
#if SETABSVOL
uint16_t SNDISR_SB_VOL;
#endif
#if PREMAPDMA
extern uint32_t SNDISR_MappedBase; /* linear address mapped ISA DMA region (0x000000 - 0xffffff) */
#endif

static uint32_t ISR_DMA_Addr = 0;
static uint32_t ISR_DMA_Size = 0;

/* ISR_DMA_MappedAddr is dynamically set and contains the current SB DMA address;
 * it's usually in conventional memory, but might be anywhere in first 16M address space.
 * In the latter case, the address mapping will be released by HDPMI when a protected-mode
 * program exits.That's why this variable should be cleared by the int 21h handler when
 * a DOS exit occurs.
 */
uint32_t ISR_DMA_MappedAddr = 0;

#ifdef _LOGBUFFMAX
uint32_t dwMaxBytes = 0;
#endif

#ifdef DJGPP
static inline void _disable_ints(void) { asm("mov $0x900, %%ax\n\t" "int $0x31" ::: "eax" ); }
static inline void  _enable_ints(void) { asm("mov $0x901, %%ax\n\t" "int $0x31" ::: "eax" ); }
#else
void _disable_ints(void);
void  _enable_ints(void);
#pragma aux _disable_ints = \
    "mov ax, 900h" \
    "int 31h" \
    parm[] \
    modify exact [ax]
#pragma aux _enable_ints = \
    "mov ax, 901h" \
    "int 31h" \
    parm[] \
    modify exact [ax]
#endif

static int16_t *pPCM = NULL;

#ifndef DJGPP
/* here malloc/free is superfast since it's a very simple "stack" */
#define MALLOCSTATIC 0
#else
#define MALLOCSTATIC 1
#endif

#if SLOWDOWN
#include "TIMER.H"
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
 */

static unsigned int cv_rate( PCM_CV_TYPE_S *pcmsrc, unsigned int samplenum, unsigned int channels, unsigned int samplerate, unsigned int newrate)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	const unsigned int instep = ((samplerate / newrate) << 12) | (((4096 * (samplerate % newrate) + newrate - 1 ) / newrate) & 0xFFF);
	const unsigned int inend = (samplenum / channels) << 12;
	PCM_CV_TYPE_S *pcmdst;
	unsigned long ipi;
	unsigned int inpos = 0;//(samplerate < newrate) ? instep/2 : 0;
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

	dbgprintf(("cv_rate: srcrate=%u dstrate=%u chn=%u smpl=%u step=%x end=%x\n", samplerate, newrate, channels, samplenum, instep, inend ));

	pcmdst = pcmsrc;
	total = samplenum/channels;

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

#ifdef DJGPP
int SNDISR_Interrupt( uint32_t clstk_esp, uint32_t clstk_ss )
/////////////////////////////////////////////////////////////
#else
int SNDISR_Interrupt( struct clientregs _far *clstat )
//////////////////////////////////////////////////////
#endif
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

#if !TRIGGERATONCE
    if ( VSB_TriggerIRQ )
        VIRQ_Invoke();
#endif
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

        //dbgprintf(("isr: pos=%X bytes=%d rate=%d smpsize=%u chn=%u DMAidx=%u\n", SB_Pos, SB_BuffSize, SB_Rate, samplesize, channels, DMA_Index ));
        /* a while loop that may run 2 times if a SB buffer overrun occured */
        do {
            uint32_t DMA_Base = VDMA_GetBase(dmachannel);
            uint32_t DMA_Index = VDMA_GetIndex(dmachannel);
            int32_t DMA_Count = VDMA_GetCount(dmachannel);
            uint32_t SB_BuffSize = VSB_GetSampleBufferSize();
            uint32_t SB_Pos = VSB_GetPos();
            uint32_t SB_Rate = VSB_GetSampleRate();
#if !PREMAPDMA
            /* check if the current DMA address is within the mapped region.
             * if no, release current mapping region.
             */
            if( ISR_DMA_MappedAddr && !(DMA_Base >= ISR_DMA_Addr && DMA_Base + DMA_Index + DMA_Count <= ISR_DMA_Addr + ISR_DMA_Size )) {
                if( ISR_DMA_MappedAddr >= 0x100000 ) {
                    __dpmi_meminfo info;
                    info.address = ISR_DMA_MappedAddr;
                    __dpmi_free_physical_address_mapping(&info);
                }
                ISR_DMA_MappedAddr = 0;
            }
            /* if there's no mapped region, create one that covers current DMA op
             */
            if( ISR_DMA_MappedAddr == 0) {
                ISR_DMA_Addr = DMA_Base;
                ISR_DMA_Size = max( min(DMA_Index + DMA_Count, 0x4000 ), 0x20000 );
                if ( DMA_Base < 0x100000 )
                    ISR_DMA_MappedAddr = DMA_Base;
                else {
                    __dpmi_meminfo info;
                    info.address = ISR_DMA_Addr;
                    info.size = ISR_DMA_Size;
                    if( __dpmi_physical_address_mapping(&info) == -1 )
                        fatal_error( 2 );
                    ISR_DMA_MappedAddr = info.address;
                }
                dbgprintf(("isr, ISR_DMA address (re)mapped: DMA_Base=%x, DMA_Size=%x, DMA_MappedAddr=%x\n", ISR_DMA_Addr, ISR_DMA_Size, ISR_DMA_MappedAddr ));
            }
#endif
            count = samples - IdxSm;
            resample = true; //don't resample if sample rates are close
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
#if PREMAPDMA
            memcpy( pPCM + IdxSm * 2, NearPtr( SNDISR_MappedBase + DMA_Base + DMA_Index, bytes ));
#else
            if( ISR_DMA_MappedAddr == 0 || VSB_IsSilent() ) {//map failed?
                memset( pPCM + IdxSm * 2, 0, bytes);
            } else
                memcpy( pPCM + IdxSm * 2, NearPtr(ISR_DMA_MappedAddr + ( DMA_Base - ISR_DMA_Addr) + DMA_Index ), bytes );
#endif

            /* format conversion needed? */
#if ADPCM
            if( VSB_GetBits() < 8)
                count = DecodeADPCM((uint8_t*)(pPCM + IdxSm * 2), bytes);
#endif
            if( samplesize != 2 )
                cv_bits_8_to_16( pPCM + IdxSm * 2, count * channels ); /* converts unsigned 8-bit to signed 16-bit */
#if SUP16BITUNSIGNED
            else if ( !VSB_IsSigned() )
                for ( i = IdxSm * 2, j = i + count * channels; i < j; *(pPCM+i) ^= 0x8000, i++ );
#endif
            if( resample ) /* SB_Rate != freq */
                count = cv_rate( pPCM + IdxSm * 2, count * channels, channels, SB_Rate, freq ) / channels;
            if( channels == 1) //should be the last step
                cv_channels_1_to_2( pPCM + IdxSm * 2, count);

            /* conversion done; now set new values for DMA and SB buffer;
             * in case the end of SB buffer is reached, emulate an interrupt
             * and run this loop a second time.
             */
            IdxSm += count;
            //dbgprintf(("isr: samples:%d %d %d\n", count, IdxSm, samples));
            DMA_Index = VDMA_SetIndexCount(dmachannel, DMA_Index + bytes, DMA_Count - bytes);
            DMA_Count = VDMA_GetCount( dmachannel );
            SB_Pos = VSB_SetPos( SB_Pos + bytes ); /* will set mixer IRQ status if pos beyond buffer */
            if( SB_Pos >= SB_BuffSize ) {
                dbgprintf(("isr: Pos/BuffSize=%u/%u samples/count=%u/%u bytes=%u dmaIdx/Cnt=%X/%X\n", SB_Pos, SB_BuffSize, samples, count, bytes, VDMA_GetIndex(dmachannel), VDMA_GetCount(dmachannel) ));
                if(!VSB_GetAuto())
                    VSB_Stop();
                VSB_SetPos(0); /* */
                VIRQ_Invoke();
            }
        } while(VDMA_GetAuto(dmachannel) && (IdxSm < samples) && VSB_Running());

        /* in case there weren't enough samples copied, fill the rest with silence */
        for( i = IdxSm; i < samples; i++ )
            *(pPCM + i*2+1) = *(pPCM + i*2) = 0;

	} else if ( i = VSB_GetDirectCount( &pDirect ) ) {

		int count = i;
		uint32_t freq = AU_getfreq( hAU );

		/* calc the src frequency by formula:
		 * x / dst-freq = src-smpls / dst-smpls
		 * x = src-smpl * dst-freq / dst-smpls
		 */
		uint32_t SB_Rate = count * freq / samples;

		//dbgprintf(("isr, direct samples: cnt=%d, samples=%d, rate%u\n", count, samples, SB_Rate ));
		memcpy( pPCM, pDirect, count );
		cv_bits_8_to_16( pPCM, count );
		count = cv_rate( pPCM, count, 1, SB_Rate, freq );
		cv_channels_1_to_2( pPCM, count );
		for( i = count; i < samples; i++ )
			*(pPCM + i*2+1) = *(pPCM + i*2) = 0;
		VSB_ResetDirectCount();
		digital = true;
	}

    /* software mixer: very simple implemented - but should work quite well */

    //if( gvars.opl3 ) {
#ifndef NOFM
    if( VOPL3_IsActive() ) {
        int channels;
        pPCMOPL = digital ? pPCM + samples * 2 : pPCM;
        VOPL3_GenSamples( pPCMOPL, samples ); //will generate samples*2 if stereo
        //always use 2 channels
        channels = VOPL3_GetMode() ? 2 : 1;
        if( channels == 1 )
            cv_channels_1_to_2( pPCMOPL, samples );

        if( digital ) {
#if MIXERROUTINE==0
            for( i = 0; i < samples * 2; i++ ) {
                int a = (*(pPCM+i) * (int)voicevol / 256) + 32768;    /* convert to 0-65535 */
                int b = (*(pPCMOPL+i) * (int)midivol / 256 ) + 32768; /* convert to 0-65535 */
                int mixed = (a < 32768 || b < 32768) ? ((a*b)/32768) : ((a+b)*2 - (a*b)/32768 - 65536);
                *(pPCM+i) = (mixed > 65535 ) ? 0x7fff : mixed - 32768;
            }
#elif MIXERROUTINE==1
            /* this variant is simple, but quiets too much ... */
            for( i = 0; i < samples * 2; i++ ) *(pPCM+i) = ( *(pPCM+i) * voicevol + *(pPCMOPL+i) * midivol ) >> (8+1);
#else
            /* in assembly it's probably easier to handle signed/unsigned shifts */
            SNDISR_Mixer( pPCM, pPCMOPL, samples * 2, voicevol, midivol );
#endif
#ifdef _LOGBUFFMAX
            if ( (( pPCMOPL + samples * 2 ) - pPCM ) * sizeof(int16_t) > dwMaxBytes )
                dwMaxBytes = (( pPCMOPL + samples * 2 ) - pPCM ) * sizeof(int16_t);
#endif
        } else
            for( i = 0; i < samples * 2; i++, pPCMOPL++ ) *pPCMOPL = ( *pPCMOPL * midivol ) >> 8;
    } else {
#endif
        if( digital ) {
            for( i = 0, pPCMOPL = pPCM; i < samples * 2; i++, pPCMOPL++ ) *pPCMOPL = ( *pPCMOPL * voicevol ) >> 8;
            //dbgprintf(("+"));
#ifdef _LOGBUFFMAX
            if ( (( pPCMOPL + samples * 2 ) - pPCM ) * sizeof(int16_t) > dwMaxBytes )
                dwMaxBytes = (( pPCMOPL + samples * 2 ) - pPCM ) * sizeof(int16_t);
#endif
        } else
            memset( pPCM, 0, samples * sizeof(int16_t) * 2 );
#ifndef NOFM
    }
#endif
    //aui.samplenum = samples * 2;
    //aui.pcm_sample = ISR_PCM;
    AU_writedata( hAU, samples * 2, pPCM );

#if SLOWDOWN
    if ( gvars.slowdown)
        delay_10us(gvars.slowdown);
#endif

    PIC_SendEOI( AU_getirq( hAU ) );
    return(1);
}

bool SNDISR_InstallISR( uint8_t intno, int(*ISR)(void) )
////////////////////////////////////////////////////////
{
    __dpmi_meminfo info;
    info.address = 0;
    info.size = ( gvars.buffsize + 1 ) * 4096;
    if (__dpmi_allocate_linear_memory( &info, 1 ) == -1 )
        return false;

    /* uncommit the page behind the buffer so a buffer overflow will cause a page fault */
    __dpmi_set_page_attr( info.handle, gvars.buffsize * 4096, 1, 0);
    pPCM = NearPtr( info.address );
    dbgprintf(("SNDISR_InstallISR: pPCM=%X\n", pPCM ));
    if ( _hdpmi_InstallISR( intno, ISR ) ) {
        if ( _hdpmi_InstallInt31( intno ) ) {
            return true;
        }
    }
    return false;
}
bool SNDISR_UninstallISR( void )
////////////////////////////////
{
    /* first uninstall int 31h, then ISR! */
#ifdef _LOGBUFFMAX
    printf("max PCM buffer usage: %u\n", dwMaxBytes );
#endif
    _hdpmi_UninstallInt31();
    return ( _hdpmi_UninstallISR() );
}



