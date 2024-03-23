
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
extern uint16_t MAIN_SB_VOL;
#endif
#if PREMAPDMA
extern uint32_t MAIN_MappedBase; /* linear address mapped ISA DMA region (0x000000 - 0xffffff) */
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

static void SNDISR_Interrupt( void );

#define ISR_PCM_SAMPLESIZE 16384 /* sample buffer size */

static int16_t *pOPLPCM = NULL;
static int16_t *pPCM = NULL;
//static int16_t ISR_OPLPCM[ISR_PCM_SAMPLESIZE+256];
//static int16_t ISR_PCM[ISR_PCM_SAMPLESIZE+256];

#define MALLOCSTATIC 1

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

static unsigned int mixer_speed_lq( PCM_CV_TYPE_S *pcmsrc, unsigned int samplenum, unsigned int channels, unsigned int samplerate, unsigned int newrate)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
	PCM_CV_TYPE_S* buff = (PCM_CV_TYPE_S*)malloc(samplenum * sizeof(PCM_CV_TYPE_S));
#endif
	memcpy( buff, pcmsrc, samplenum * sizeof(PCM_CV_TYPE_S) );

	dbgprintf(("mixer_speed_lq: srcrate=%u dstrate=%u chn=%u smpl=%u step=%x end=%x\n", samplerate, newrate, channels, samplenum, instep, inend ));

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
	PCM_CV_TYPE_C *inptr = (PCM_CV_TYPE_C *)pcm;
	PCM_CV_TYPE_C *outptr = (PCM_CV_TYPE_C *)pcm;

	dbgprintf(("cv_bits_8_to_16( pcm=%X, smpls=%u\n", pcm, samplenum ));

	inptr += samplenum;
	outptr += samplenum * 2;

	for ( ; samplenum; samplenum-- ) {
		/* copy upper bits (bytes) to the right/correct place */
		inptr--;
		outptr--;
		*outptr = *inptr ^ 0x80; /* convert unsigned to signed */
		/* fill lower bits (bytes) with zeroes */
		outptr--;
		*outptr = 0;
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

int SNDISR_InterruptPM( void )
//////////////////////////////
{
    /* check if the irq belong the sound card */
    //if(aui.card_handler->irq_routine && aui.card_handler->irq_routine(&aui)) //check if the irq belong the sound card
    if( AU_isirq( hAU ) ) {
#if SETIF
        _enable_ints();
#endif
        SNDISR_Interrupt();
        PIC_SendEOIWithIRQ( AU_getirq( hAU ) );
        return(1);
    }
    return(0);
}

static void SNDISR_Interrupt( void )
////////////////////////////////////
{
    uint32_t mastervol;
    uint32_t voicevol;
    uint32_t midivol;
    int samples;
    bool digital;
    int dma;
    int32_t DMA_Count;
    int i;

#if !TRIGGERATONCE
    if ( VSB_TriggerIRQ ) {
        VSB_TriggerIRQ = 0;
        VSB_SetIRQStatus();
        VIRQ_Invoke();
    }
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
    if( MAIN_SB_VOL != mastervol * gvars.vol / 9) {
        MAIN_SB_VOL =  mastervol * gvars.vol / 9;
        //uint8_t buffer[200];
        //asm("fsave %0": "m"(buffer)); /* needed if AU_setmixer_one() uses floats */
        AU_setmixer_one( hAU, AU_MIXCHAN_MASTER, MIXER_SETMODE_ABSOLUTE, mastervol * 100 / 256 ); /* convert to percentage 0-100 */
        //asm("frstor %0": "m"(buffer));
        //dbgprintf(("ISR: set master volume=%u\n", MAIN_SB_VOL ));
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
    //dbgprintf(("samples:%u ",samples));

    if(samples == 0) /* no free space in DMA buffer? */
        return;

    digital = VSB_Running();
    dma = VSB_GetDMA();
    DMA_Count = VDMA_GetCounter(dma);

    if( digital ) {
        int i,j;
        uint32_t DMA_Addr = VDMA_GetAddress(dma);
        int32_t DMA_Index = VDMA_GetIndex(dma);
        uint32_t SB_Bytes = VSB_GetSampleBytes();
        uint32_t SB_Pos = VSB_GetPos();
        uint32_t SB_Rate = VSB_GetSampleRate();
        uint32_t freq = AU_getfreq( hAU );
        int samplesize = max( 1, VSB_GetBits() / 8 );
        int channels = VSB_GetChannels();
        int pos = 0;
        int count;
        bool resample; //don't resample if sample rates are close
        int bytes;

        //dbgprintf(("dsp: pos=%X bytes=%d rate=%d smpsize=%u chn=%u\n", SB_Pos, SB_Bytes, SB_Rate, samplesize, channels ));
        //dbgprintf(("DMA index: %x\n", DMA_Index));
        do {
#if !PREMAPDMA
            /* check if the current DMA address is within the mapped region.
             * if no, release current mapping region.
             */
            if( ISR_DMA_MappedAddr && !(DMA_Addr >= ISR_DMA_Addr && DMA_Addr + DMA_Index + DMA_Count <= ISR_DMA_Addr + ISR_DMA_Size )) {
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
                ISR_DMA_Addr = DMA_Addr;
                ISR_DMA_Size = max( min(DMA_Index + DMA_Count, 0x4000 ), 0x20000 );
                if ( DMA_Addr < 0x100000 )
                    ISR_DMA_MappedAddr = DMA_Addr;
                else {
                    __dpmi_meminfo info;
                    info.address = ISR_DMA_Addr;
                    info.size = ISR_DMA_Size;
                    if( __dpmi_physical_address_mapping(&info) == -1 )
                        fatal_error( 2 );
                    ISR_DMA_MappedAddr = info.address;
                }
                // dbgprintf(("ISR: chn=%d DMA_Addr/Index/Count=%x/%x/%x ISR_DMA_Addr/Size/MappedAddr=%x/%x/%x\n", dma, DMA_Addr, DMA_Index, DMA_Count, ISR_DMA_Addr, ISR_DMA_Size, ISR_DMA_MappedAddr ));
            }
#endif
            count = samples - pos;
            resample = true; //don't resample if sample rates are close
            if( SB_Rate < freq )
                //count = max(channels, count / ( ( freq + SB_Rate-1) / SB_Rate ));
                count = max(1, count * SB_Rate / freq );
            else if(SB_Rate > freq )
                //count *= (SB_Rate + aui.freq_card/2)/aui.freq_card;
                count = count * SB_Rate / freq;
            else
                resample = false;
            count = min(count, max(1,(DMA_Count) / samplesize / channels)); //stereo initial 1 byte
            count = min(count, max(1,(SB_Bytes - SB_Pos) / samplesize / channels )); //stereo initial 1 byte. 1 /2channel = 0, make it 1
            if(VSB_GetBits() < 8) //ADPCM 8bit
                count = max(1, count / (9 / VSB_GetBits()));
            bytes = count * samplesize * channels;

            /* copy samples to our PCM buffer
             */
#if PREMAPDMA
            memcpy( pPCM + pos * 2, NearPtr(MAIN_MappedBase + DMA_Addr + DMA_Index, bytes));
#else
            if( ISR_DMA_MappedAddr == 0 || VSB_IsSilent() ) {//map failed?
                memset( pPCM + pos * 2, 0, bytes);
            } else
                memcpy( pPCM + pos * 2, NearPtr(ISR_DMA_MappedAddr+(DMA_Addr - ISR_DMA_Addr) + DMA_Index ), bytes);
#endif

            /* format conversion needed? */
#if ADPCM
            if( VSB_GetBits() < 8) //ADPCM  8bit
                count = DecodeADPCM((uint8_t*)(pPCM + pos * 2), bytes);
#endif
            if( samplesize != 2 )
                cv_bits_8_to_16( pPCM + pos * 2, count * channels ); /* converts unsigned 8-bit to signed 16-bit */
#if SUP16BITUNSIGNED
            else if ( !VSB_IsSigned() )
                for ( i = pos * 2, j = i + count * channels; i < j; *(pPCM+i) ^= 0x8000, i++ );
#endif
            if( resample ) /* SB_Rate != freq */
                count = mixer_speed_lq( pPCM + pos * 2, count * channels, channels, SB_Rate, freq ) / channels;
            if( channels == 1) //should be the last step
                cv_channels_1_to_2( pPCM + pos * 2, count);
            pos += count;
            //dbgprintf(("samples:%d %d %d\n", count, pos, samples));
            DMA_Index = VDMA_SetIndexCounter(dma, DMA_Index + bytes, DMA_Count - bytes);
            //int LastDMACount = DMA_Count;
            DMA_Count = VDMA_GetCounter( dma );
            SB_Pos = VSB_SetPos( SB_Pos + bytes ); /* will set mixer IRQ status! (register 0x82) */
            if(SB_Pos >= SB_Bytes)
            {
                //dbgprintf(("SNDISR_Interrupt: SB_Pos >= SB_Bytes: %u/%u, bytes/count=%u/%u, dma=%X/%u\n", SB_Pos, SB_Bytes, bytes, count, VDMA_GetAddress(dma), VDMA_GetCounter(dma) ));
                if(!VSB_GetAuto())
                    VSB_Stop();
                VSB_SetPos(0);
                VIRQ_Invoke();
                SB_Bytes = VSB_GetSampleBytes();
                SB_Pos = VSB_GetPos();
                SB_Rate = VSB_GetSampleRate();
                //incase IRQ handler re-programs DMA
                DMA_Index = VDMA_GetIndex(dma);
                DMA_Count = VDMA_GetCounter(dma);
                DMA_Addr = VDMA_GetAddress(dma);
            }
        } while(VDMA_GetAuto(dma) && (pos < samples) && VSB_Running());

        //dbgprintf(("SNDISR_Interrupt: pos/samples=%u/%u, running=%u\n", pos, samples, VSB_Running() ));
#if 1
        for( i = pos; i < samples; i++ )
            *(pPCM + i*2+1) = *(pPCM + i*2) = 0;
#else
        samples = min(samples, pos);
#endif
    }

    /* software mixer: very simple implemented - but should work quite well */

    //if( gvars.opl3 ) {
#ifndef NOFM
	if( VOPL3_IsActive() ) {
        int16_t* pcm = digital ? pOPLPCM : pPCM;
        int channels;
        VOPL3_GenSamples(pcm, samples); //will generate samples*2 if stereo
        //always use 2 channels
        channels = VOPL3_GetMode() ? 2 : 1;
        if( channels == 1 )
            cv_channels_1_to_2( pcm, samples );

        if( digital ) {
#if MIXERROUTINE==0
            for( i = 0; i < samples * 2; i++ ) {
                int a = (*(pPCM+i) * (int)voicevol / 256) + 32768;    /* convert to 0-65535 */
                int b = (*(pOPLPCM+i) * (int)midivol / 256 ) + 32768; /* convert to 0-65535 */
                int mixed = (a < 32768 || b < 32768) ? ((a*b)/32768) : ((a+b)*2 - (a*b)/32768 - 65536);
                *(pPCM+i) = (mixed > 65535 ) ? 0x7fff : mixed - 32768;
            }
#elif MIXERROUTINE==1
            /* this variant is simple, but quiets too much ... */
            for( i = 0; i < samples * 2; i++ ) *(pPCM+i) = ( *(pPCM+i) * voicevol + *(pOPLPCM+i) * midivol ) >> (8+1);
#else
            /* in assembly it's probably easier to handle signed/unsigned shifts */
            SNDISR_Mixer( pPCM, pOPLPCM, samples * 2, voicevol, midivol );
#endif
        } else
            for( i = 0; i < samples * 2; i++ ) *(pPCM+i) = ( *(pPCM+i) * midivol ) >> 8;
    } else {
#endif
        if( digital )
            for( i = 0; i < samples * 2; i++ ) *(pPCM+i) = ( *(pPCM+i) * voicevol ) >> 8;
        else
            memset( pPCM, 0, samples * sizeof(int16_t) * 2 );
#ifndef NOFM
    }
#endif
    //aui.samplenum = samples * 2;
    //aui.pcm_sample = ISR_PCM;
    AU_writedata( hAU, samples * 2, pPCM );
}

bool SNDISR_InstallISR( uint8_t intno, int(*ISR)(void) )
////////////////////////////////////////////////////////
{
    pOPLPCM = malloc( sizeof(int16_t) * (ISR_PCM_SAMPLESIZE + 256 ) );
    pPCM    = malloc( sizeof(int16_t) * (ISR_PCM_SAMPLESIZE + 256 ) );
    dbgprintf(("SNDISR_InstallISR: pOPLPCM=%X pPCM=%X\n", pOPLPCM, pPCM ));
    if ( pOPLPCM && pPCM ) {
        if ( _hdpmi_InstallISR( intno, ISR ) ) {
            if ( _hdpmi_InstallInt31( intno ) ) {
                return true;
            }
        }
    }
    return false;
}
bool SNDISR_UninstallISR( void )
////////////////////////////////
{
    /* first uninstall int 31h, then ISR! */
    _hdpmi_UninstallInt31();
    return ( _hdpmi_UninstallISR() );
}



