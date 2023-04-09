
/* sound hardware interrupt routine */

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "SBEMUCFG.H"
#include "PLATFORM.H"
#include "PIC.H"
#include "DPMIHLP.H"
#include "VDMA.H"
#include "VIRQ.H"
#include "VOPL3.H"
#include "VSB.H"

#include <MPXPLAY.H>
#include <MIX_FUNC.H>

extern mpxplay_audioout_info_s aui;
extern struct globalvars gvars;
extern uint16_t MAIN_SB_VOL;
#if PREMAPDMA
extern uint32_t MAIN_MappedBase; /* linear address mapped ISA DMA region (0x000000 - 0xffffff) */
#endif

static uint32_t ISR_DMA_Addr = 0;
static uint32_t ISR_DMA_Size = 0;
static uint32_t ISR_DMA_MappedAddr = 0;

static void SNDISR_Interrupt( void );

#define ISR_PCM_SAMPLESIZE 16384 /* sample buffer size */

static int16_t ISR_OPLPCM[ISR_PCM_SAMPLESIZE+256];
static int16_t ISR_PCM[ISR_PCM_SAMPLESIZE+256];

int SNDISR_InterruptPM( void )
//////////////////////////////
{
    if(aui.card_handler->irq_routine && aui.card_handler->irq_routine(&aui)) //check if the irq belong the sound card
    {
#if SETIF
        _enable_ints();
#endif
        SNDISR_Interrupt();
        PIC_SendEOIWithIRQ(aui.card_irq);
        return(1);
    }
    return(0);
}

static void SNDISR_Interrupt( void )
////////////////////////////////////
{
    int32_t vol;
    int32_t voicevol;
    int32_t midivol;

#if !TRIGGERATONCE
    if ( VSB_TriggerIRQ ) {
        VSB_TriggerIRQ = 0;
        VIRQ_Invoke();
    }
#endif
    if( gvars.type < 4) //SB2.0 and before
    {
        vol = (VSB_GetMixerReg( SB_MIXERREG_MASTERVOL) >> 1) * 256 / 7;
        voicevol = (VSB_GetMixerReg( SB_MIXERREG_VOICEVOL) >> 1) * 256 / 3;
        midivol = (VSB_GetMixerReg( SB_MIXERREG_MIDIVOL) >> 1) * 256 / 7;
    }
    else if( gvars.type == 6) //SB16
    {
        vol = (VSB_GetMixerReg( SB_MIXERREG_MASTERSTEREO) >> 4) * 256 / 15; //4:4
        voicevol = (VSB_GetMixerReg( SB_MIXERREG_VOICESTEREO) >> 4) * 256 / 15; //4:4
        midivol = (VSB_GetMixerReg( SB_MIXERREG_MIDISTEREO) >> 4) * 256 / 15; //4:4
        //dbgprintf("vol: %d, voicevol: %d, midivol: %d\n", vol, voicevol, midivol);
    }
    else //SBPro
    {
        vol = (VSB_GetMixerReg( SB_MIXERREG_MASTERSTEREO) >> 5) * 256 / 7; //3:1:3:1 stereo usually the same for both channel for games?;
        voicevol = (VSB_GetMixerReg( SB_MIXERREG_VOICESTEREO) >> 5) * 256 / 7; //3:1:3:1
        midivol = (VSB_GetMixerReg( SB_MIXERREG_MIDISTEREO) >> 5) * 256 / 7;
    }
    if(MAIN_SB_VOL != vol * gvars.vol / 9)
    {
        //dbgprintf("set sb volume:%d %d\n", MAIN_SB_VOL, vol * gvars.vol/9);
        MAIN_SB_VOL =  vol * gvars.vol / 9;
        //uint8_t buffer[200];
        //asm("fsave %0": "m"(buffer)); /* needed if AU_setmixer_one() uses floats */
        AU_setmixer_one( &aui, AU_MIXCHAN_MASTER, MIXER_SETMODE_ABSOLUTE, vol * 100 / 256 );
        //asm("frstor %0": "m"(buffer));
    }

    aui.card_outbytes = aui.card_dmasize;
    int samples = AU_cardbuf_space(&aui) / sizeof(int16_t) / 2; //16 bit, 2 channels
    //dbgprintf("samples:%u ",samples);

    if(samples == 0) /* no free space in DMA buffer? */
        return;

    bool digital = VSB_Running();
    int dma = VSB_GetDMA();
    int32_t DMA_Count = VDMA_GetCounter(dma);

    if( digital ) {
        uint32_t DMA_Addr = VDMA_GetAddress(dma);
        int32_t DMA_Index = VDMA_GetIndex(dma);
        uint32_t SB_Bytes = VSB_GetSampleBytes();
        uint32_t SB_Pos = VSB_GetPos();
        uint32_t SB_Rate = VSB_GetSampleRate();
        int samplesize = max(1,VSB_GetBits()/8);
        int channels = VSB_GetChannels();
        //dbgprintf("dsp: pos=%X bytes=%d rate=%d smpsize=%u chn=%u\n", SB_Pos, SB_Bytes, SB_Rate, samplesize, channels );
        //dbgprintf("DMA index: %x\n", DMA_Index);
        int pos = 0;
        do {
#if !PREMAPDMA
            /* check if the current DMA address is within the mapped region.
             * if no, release current mapping region.
             */
            if( ISR_DMA_MappedAddr != 0
             && !(DMA_Addr >= ISR_DMA_Addr && DMA_Addr + DMA_Index + DMA_Count <= ISR_DMA_Addr + ISR_DMA_Size ))
            {
                if(ISR_DMA_MappedAddr > 1024*1024)
                    DPMI_UnmapMemory( ISR_DMA_MappedAddr );
                ISR_DMA_MappedAddr = 0;
            }
            /* if there's no mapped region, create one that covers current DMA op
             */
            if( ISR_DMA_MappedAddr == 0) {
                ISR_DMA_Addr = DMA_Addr & ~0xFFF;
                ISR_DMA_Size = align( max( DMA_Addr - ISR_DMA_Addr + DMA_Index + DMA_Count, 64*1024*2 ), 4096);
                ISR_DMA_MappedAddr = (DMA_Addr + DMA_Index + DMA_Count <= 1024*1024) ? ( DMA_Addr & ~0xFFF) : DPMI_MapMemory( ISR_DMA_Addr, ISR_DMA_Size );
                // dbgprintf("ISR: chn=%d DMA_Addr/Index/Count=%x/%x/%x ISR_DMA_Addr/Size/MappedAddr=%x/%x/%x\n", dma, DMA_Addr, DMA_Index, DMA_Count, ISR_DMA_Addr, ISR_DMA_Size, ISR_DMA_MappedAddr );
            }
#endif
            int count = samples - pos;
            bool resample = true; //don't resample if sample rates are close
            if(SB_Rate < aui.freq_card)
                //count = max(channels, count/((aui.freq_card+SB_Rate-1)/SB_Rate));
                count = max(1, count * SB_Rate / aui.freq_card );
            else if(SB_Rate > aui.freq_card)
                //count *= (SB_Rate + aui.freq_card/2)/aui.freq_card;
                count = count * SB_Rate / aui.freq_card;
            else
                resample = false;
            count = min(count, max(1,(DMA_Count) / samplesize / channels)); //stereo initial 1 byte
            count = min(count, max(1,(SB_Bytes - SB_Pos) / samplesize / channels )); //stereo initial 1 byte. 1 /2channel = 0, make it 1
            if(VSB_GetBits() < 8) //ADPCM 8bit
                count = max(1, count / (9 / VSB_GetBits()));
            int bytes = count * samplesize * channels;

            /* copy samples to our PCM buffer
             */
#if PREMAPDMA
            DPMI_CopyLinear(DPMI_PTR2L(ISR_PCM + pos * 2), MAIN_MappedBase + DMA_Addr + DMA_Index, bytes);
#else
            if( ISR_DMA_MappedAddr == 0) {//map failed?
                memset(ISR_PCM + pos * 2, 0, bytes);
            } else
                DPMI_CopyLinear(DPMI_PTR2L(ISR_PCM + pos * 2), ISR_DMA_MappedAddr+(DMA_Addr - ISR_DMA_Addr)+DMA_Index, bytes);
#endif

            /* format conversion needed? */
#if ADPCM
            if(VSB_GetBits()<8) //ADPCM  8bit
                count = VSB_DecodeADPCM((uint8_t*)(ISR_PCM + pos * 2), bytes);
#endif
            if( samplesize != 2 )
                cv_bits_n_to_m( ISR_PCM + pos * 2, count * channels, samplesize, 2);
            if( resample ) /* SB_Rate != aui.freq_card*/
                count = mixer_speed_lq( ISR_PCM + pos * 2, count * channels, channels, SB_Rate, aui.freq_card)/channels;
            if( channels == 1) //should be the last step
                cv_channels_1_to_n( ISR_PCM + pos * 2, count, 2, 2);
            pos += count;
            //dbgprintf("samples:%d %d %d\n", count, pos, samples);
            DMA_Index = VDMA_SetIndexCounter(dma, DMA_Index+bytes, DMA_Count-bytes);
            //int LastDMACount = DMA_Count;
            DMA_Count = VDMA_GetCounter( dma );
            SB_Pos = VSB_SetPos( SB_Pos + bytes );
            if(SB_Pos >= SB_Bytes)
            {
                dbgprintf("SNDISR_Interrupt: SB_Pos >= SB_Bytes: %u/%u, bytes/count=%u/%u, dma=%X/%u\n", SB_Pos, SB_Bytes, bytes, count, VDMA_GetAddress(dma), VDMA_GetCounter(dma) );
                if(!VSB_GetAuto())
                    VSB_Stop();
                SB_Pos = VSB_SetPos(0);
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

        dbgprintf("SNDISR_Interrupt: pos/samples=%u/%u, running=%u\n", pos, samples, VSB_Running() );
        //for(int i = pos; i < samples; ++i)
        //    ISR_PCM[i*2+1] = ISR_PCM[i*2] = 0;
        samples = min(samples, pos);
    }
    else if( gvars.opl3 )
        memset( ISR_PCM, 0, samples * sizeof(int16_t) * 2 ); //output muted samples.

    if( gvars.opl3 ) {
        int16_t* pcm = digital ? ISR_OPLPCM : ISR_PCM;
        VOPL3_GenSamples(pcm, samples); //will generate samples*2 if stereo
        //always use 2 channels
        int channels = VOPL3_GetMode() ? 2 : 1;
        if(channels == 1)
            cv_channels_1_to_n(pcm, samples, 2, SBEMU_BITS/8);

        if( digital ) {
            for(int i = 0; i < samples*2; ++i) {
                int a = (int)(ISR_PCM[i] * voicevol / 256) + 32768;
                int b = (int)(ISR_OPLPCM[i] * midivol / 256) + 32768;
                int mixed = (a < 32768 || b < 32768) ? ( a * b / 32768) : ((a+b) * 2 - a*b/32768 - 65536);
                if(mixed == 65536) mixed = 65535;
                ISR_PCM[i] = mixed - 32768;
            }
        } else
            for(int i = 0; i < samples*2; ++i)
                ISR_PCM[i] = ISR_PCM[i] * midivol / 256;
    } else if( digital )
        for( int i = 0; i < samples*2; ++i)
            ISR_PCM[i] = ISR_PCM[i] * voicevol / 256;
    samples *= 2; //to stereo

    aui.samplenum = samples;
    aui.pcm_sample = ISR_PCM;

    AU_writedata(&aui);
}