#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include <DPMI/DBGUTIL.H>
#include <SBEMUCFG.H>
#include <PIC.H>
#include <OPL3EMU.H>
#include <VDMA.H>
#include <VIRQ.H>
#include <SBEMU.H>
#include <UNTRAPIO.H>
#include "QEMM.H"
#include "HDPMIPT.H"

#include <MPXPLAY.H>
#include <AU_MIXER/MIX_FUNC.H>

#define SB16 1
#define QEMMPICTRAPDYN 1
#define MAIN_PCM_SAMPLESIZE 16384

int dbgprintf(const char *fmt, ... );
#define dbgprintf dbgprintf

#ifdef _DEBUG
extern void TestSound(BOOL play, mpxplay_audioout_info_s *);
extern int16_t* TEST_Sample;
extern unsigned long TEST_SampleLen;
#endif

static mpxplay_audioout_info_s aui = {0};

static int16_t MAIN_OPLPCM[MAIN_PCM_SAMPLESIZE+256];
static int16_t MAIN_PCM[MAIN_PCM_SAMPLESIZE+256];

static DPMI_ISR_HANDLE MAIN_TimerIntHandlePM;
static uint32_t MAIN_DMA_Addr = 0;
static uint32_t MAIN_DMA_Size = 0;
static uint32_t MAIN_DMA_MappedAddr = 0;
static uint16_t MAIN_SB_VOL = 0; //intial set volume will cause interrupt missing?
static uint16_t MAIN_GLB_VOL = 0; //TODO: add hotkey

static void MAIN_Interrupt();
static void MAIN_InterruptPM();


static QEMM_IODT MAIN_OPL3IODT[] =
{
    0x388, &OPL3EMU_OPL3_388,
    0x389, &OPL3EMU_OPL3_389,
    0x38A, &OPL3EMU_OPL3_38A,
    0x38B, &OPL3EMU_OPL3_38B
};

static const uint8_t MAIN_ChannelPageMap[] =
{
    0x87, 0x83, 0x81, 0x82, -1, 0x8b, 0x89, 0x8a
};

static QEMM_IODT MAIN_VDMA_IODT[] =
{
    0x00, &VDMA_DMA,  /* ch 0 */
    0x01, &VDMA_DMA,
    0x02, &VDMA_DMA,  /* ch 1 */
    0x03, &VDMA_DMA,
    //0x04, &VDMA_DMA,  /* ch 2 */
    //0x05, &VDMA_DMA,
    0x06, &VDMA_DMA,  /* ch 3 */
    0x07, &VDMA_DMA,
    0x08, &VDMA_DMA,
    0x09, &VDMA_DMA,
    0x0A, &VDMA_DMA,
    0x0B, &VDMA_DMA,
    0x0C, &VDMA_DMA,
    0x0D, &VDMA_DMA,
    0x0E, &VDMA_DMA,
    0x0F, &VDMA_DMA,
    //0x81, &VDMA_DMA, /* ch 2 */
    0x82, &VDMA_DMA, /* ch 3 */
    0x83, &VDMA_DMA, /* ch 1 */
    0x87, &VDMA_DMA, /* ch 0 */
#if SB16
    0x89, &VDMA_DMA, /* ch 6 */
    0x8A, &VDMA_DMA, /* ch 7 */
    0x8B, &VDMA_DMA, /* ch 5 */
    //0x8F, &VDMA_DMA,
    //0xC0, &VDMA_DMA, /* ch 4 */
    //0xC2, &VDMA_DMA,
    0xC4, &VDMA_DMA, /* ch 5 */
    0xC6, &VDMA_DMA,
    0xC8, &VDMA_DMA, /* ch 6 */
    0xCA, &VDMA_DMA,
    0xCC, &VDMA_DMA, /* ch 7 */
    0xCE, &VDMA_DMA,
    0xD0, &VDMA_DMA,
    0xD2, &VDMA_DMA,
    0xD4, &VDMA_DMA,
    0xD6, &VDMA_DMA,
    0xD8, &VDMA_DMA,
    0xDA, &VDMA_DMA,
    0xDC, &VDMA_DMA,
    0xDE, &VDMA_DMA,
#endif
};

static QEMM_IODT MAIN_VIRQ_IODT[] =
{
    0x20, &VIRQ_IRQ,
    //0x21, &VIRQ_IRQ,
    0xA0, &VIRQ_IRQ,
    //0xA1, &VIRQ_IRQ,
};

static QEMM_IODT MAIN_SB_IODT[] =
{ //MAIN_Options[OPT_ADDR].value will be added at runtime
    0x00, &OPL3EMU_OPL3_388,
    0x01, &OPL3EMU_OPL3_389,
    0x02, &OPL3EMU_OPL3_38A,
    0x03, &OPL3EMU_OPL3_38B,
    0x04, &SBEMU_SB_MixerAddr,
    0x05, &SBEMU_SB_MixerData,
    0x06, &SBEMU_SB_DSP_Reset,
    0x08, &OPL3EMU_OPL3_388,
    0x09, &OPL3EMU_OPL3_389,
    0x0A, &SBEMU_SB_DSP_Read,
    0x0C, &SBEMU_SB_DSP_Write,
    0x0E, &SBEMU_SB_DSP_ReadStatus,
    0x0F, &SBEMU_SB_DSP_ReadINT16BitACK,
};

QEMM_IOPT MAIN_VDMA_IOPT;
QEMM_IOPT MAIN_VIRQ_IOPT;
QEMM_IOPT MAIN_SB_IOPT;

QEMM_IOPT MAIN_VDMA_IOPT_PM1;
QEMM_IOPT MAIN_VDMA_IOPT_PM2;
QEMM_IOPT MAIN_VDMA_IOPT_PM3;
#if SB16
QEMM_IOPT MAIN_VHDMA_IOPT_PM1;
#endif
QEMM_IOPT MAIN_VIRQ_IOPT_PM1;
QEMM_IOPT MAIN_VIRQ_IOPT_PM2;
QEMM_IOPT MAIN_SB_IOPT_PM;

struct {
    const char* option;
    const char* desc;
    int value;
} MAIN_Options[] =
{
    "/?", "Show help", FALSE,
    "/A", "Specify IO address, valid value: 220,240", 0x220,
    "/I", "Specify IRQ number, valud value: 5,7", 7,
    "/D", "Specify DMA channel, valid value: 0,1,3", 1,
    "/T", "Specify SB Type, valid value: 0-6", 5,
#if SB16
    "/T", "Specify SB Type, valid value: 0-6", 5,
    "/H", "Specify High DMA channel, valid value: 5,6,7", -1,
#else
    "/T", "Specify SB Type, valid value: 0-5", 5,
#endif
    "/OPL", "Enable OPL3 emulation", TRUE,
    "/PM", "Support protected mode games, you can try disable it when you have compatibility issues", TRUE,
    "/RM", "Support real mode games", TRUE,
    "/O", "Select output. 0: headphone, 1: speaker. Intel HDA only", 1,
    "/VOL", "Set master volume (0-9)", 7,
#if DEBUG
    "/test", "Test sound and exit", FALSE,
#endif
    NULL, NULL, 0,
};
enum EOption
{
    OPT_Help,
    OPT_ADDR,
    OPT_IRQ,
    OPT_DMA,
    OPT_TYPE,
#if SB16
    OPT_HDMA,
#endif
    OPT_OPL,
    OPT_PM,
    OPT_RM,
    OPT_OUTPUT,
    OPT_VOL,
#if DEBUG
    OPT_TEST,
#endif
    OPT_COUNT,
};

static const char* MAIN_SBTypeString[] =
{
    "0",
    "1.0",
    "1.5",
    "2.0",
    "Pro",
    "Pro",
#if SB16
    "16",
#endif
};

static int MAIN_SB_DSPVersion[] =
{
    0,
    0x0100,
    0x0105,
    0x0202,
    0x0302,
    0x0302,
#if SB16
    0x0400,
#endif
};

int main(int argc, char* argv[])
{
    if((argc == 2 && stricmp(argv[1],"/?") == 0))
    {
        printf("SBEMU: Sound Blaster emulation on AC97. Usage:\n");
        int i = 0;
        while(MAIN_Options[i].option)
        {
            printf(" %-8s: %s. Default: %x\n", MAIN_Options[i].option, MAIN_Options[i].desc, MAIN_Options[i].value);
            ++i;
        }
        printf("\nNote: SBEMU will read BLASTER environment variable and use it, "
        "\n if /A /I /D /T /H set, they will override the BLASTER values.\n");
        printf("\nSource code used from:\n    MPXPlay (https://mpxplay.sourceforge.net/)\n    DOSBox (https://www.dosbox.com/)\n");
        return 0;
    }
    //parse BLASTER env first.
    {
        char* blaster = getenv("BLASTER");
        if(blaster != NULL)
        {
            char c;
            while((c=toupper(*(blaster++))))
            {
                if(c == 'I')
                    MAIN_Options[OPT_IRQ].value = *(blaster++) - '0';
                else if(c == 'D')
                    MAIN_Options[OPT_DMA].value = *(blaster++) - '0';
                else if(c == 'A')
                    MAIN_Options[OPT_ADDR].value = strtol(blaster, &blaster, 16);
                else if(c =='T')
                    MAIN_Options[OPT_TYPE].value = *(blaster++) - '0';
#if SB16
                else if(c =='H')
                    MAIN_Options[OPT_HDMA].value = *(blaster++) - '0';
#endif
            }
        }
    }

    for(int i = 1; i < argc; ++i)
    {
        for(int j = 0; j < OPT_COUNT; ++j)
        {
            int len = strlen(MAIN_Options[j].option);
            if(memicmp(argv[i], MAIN_Options[j].option, len) == 0)
            {
                int arglen = strlen(argv[i]);
                MAIN_Options[j].value = arglen == len ? 1 : strtol(&argv[i][len], NULL, 16);
                break;
            }
        }
    }

    if(MAIN_Options[OPT_ADDR].value != 0x220 && MAIN_Options[OPT_ADDR].value != 0x240)
    {
        printf("Error: invalid IO port address: %x.\n", MAIN_Options[OPT_ADDR].value);
        return 1;
    }
    if(MAIN_Options[OPT_IRQ].value != 0x5 && MAIN_Options[OPT_IRQ].value != 0x7)
    {
        printf("Error: invalid IRQ: %d.\n", MAIN_Options[OPT_IRQ].value);
        return 1;
    }
    if(MAIN_Options[OPT_DMA].value != 0x0 && MAIN_Options[OPT_DMA].value != 0x1 && MAIN_Options[OPT_DMA].value != 0x3)
    {
        printf("Error: invalid DMA channel.\n");
        return 1;
    }
    if(MAIN_Options[OPT_TYPE].value <= 0 || MAIN_Options[OPT_TYPE].value > 6)
    {
        printf("Error: invalid SB Type.\n");
        return 1;
    }
    if(MAIN_Options[OPT_OUTPUT].value != 0 && MAIN_Options[OPT_OUTPUT].value != 1)
    {
        printf("Error: Invalid Output.\n");
        return 1;
    }
     if(MAIN_Options[OPT_VOL].value < 0 || MAIN_Options[OPT_VOL].value > 9)
    {
        printf("Error: Invalid Volume.\n");
        return 1;
    }
    //TODO: alter BLASTER env?
#ifdef _DEBUG
    if(MAIN_Options[OPT_TEST].value) //test
    {
        AU_init(&aui);
        if(!aui.card_handler)
            return 0;
        AU_ini_interrupts(&aui);
        AU_setmixer_init(&aui);
        AU_setmixer_outs(&aui, MIXER_SETMODE_ABSOLUTE, 100);
        TestSound( TRUE, &aui );
        AU_del_interrupts(&aui);
        return 0;
    }
#endif

    DPMI_Init();
    if(MAIN_Options[OPT_RM].value)
    {
        int bcd = QEMM_GetVersion();
        _LOG("QEMM version: %x.%02x\n", bcd>>8, bcd&0xFF);
        if(bcd < 0x703)
        {
            printf("QEMM not installed, or version below 7.03: %x.%02x, disable real mode support.\n", bcd>>8, bcd&0xFF);
            MAIN_Options[OPT_RM].value = FALSE;
        }
    }
    if(MAIN_Options[OPT_PM].value)
    {
        BOOL hasHDPMI = HDPMIPT_Detect(); //another DPMI host used other than HDPMI
        if(!hasHDPMI)
            printf("HDPMI not installed, disable protected mode support.\n");
        MAIN_Options[OPT_PM].value = hasHDPMI;
    }
    BOOL enablePM = MAIN_Options[OPT_PM].value;
    BOOL enableRM = MAIN_Options[OPT_RM].value;
    if(!enablePM && !enableRM)
    {
        printf("Both real mode & protected mode support are disabled, exiting.\n");
        return 1;
    }
    
    aui.card_select_config = MAIN_Options[OPT_OUTPUT].value;
    AU_init(&aui);
    if(!aui.card_handler)
        return 0;
    if(aui.card_irq == MAIN_Options[OPT_IRQ].value)
    {
        printf("Sound card IRQ conflict, abort.\n");
        return 1;
    }
    AU_ini_interrupts(&aui);
    AU_setmixer_init(&aui);
    AU_setmixer_outs(&aui, MIXER_SETMODE_ABSOLUTE, 95);
    MAIN_GLB_VOL = MAIN_Options[OPT_VOL].value;
    MAIN_SB_VOL = 256*MAIN_GLB_VOL/9;
    AU_setmixer_one(&aui, AU_MIXCHAN_MASTER, MIXER_SETMODE_ABSOLUTE, MAIN_GLB_VOL*100/9);
    //use fixed rate
    mpxplay_audio_decoder_info_s adi = {NULL, 0, 1, SBEMU_SAMPLERATE, SBEMU_CHANNELS, SBEMU_CHANNELS, NULL, SBEMU_BITS, SBEMU_BITS/8, 0};
    AU_setrate(&aui, &adi);

    if( enableRM ) {
        enableRM = QEMM_Prepare_IOPortTrap();
        if ( !enableRM )
            printf("Error: Failed setting IO port handler for QEMM.\n");
    }

    QEMM_IOPT OPL3IOPT;
    QEMM_IOPT OPL3IOPT_PM;

    if(MAIN_Options[OPT_OPL].value)
    {
        if( enableRM && !QEMM_Install_IOPortTrap(MAIN_OPL3IODT, 4, &OPL3IOPT, NULL ) )
        {
            printf("Error: Failed installing IO port trap for QEMM.\n");
            return 1;
        }
        if( enablePM && !HDPMIPT_Install_IOPortTrap(0x388, 0x38B, MAIN_OPL3IODT, 4, &OPL3IOPT_PM) )
        {
            printf("Error: Failed installing IO port trap for HDPMI.\n");
            if (!enableRM)
                return 1;
            enablePM = FALSE;
        }

        OPL3EMU_Init(aui.freq_card);
        printf("OPL3 emulation enabled at port 388h.\n");
    }

    printf("Real mode support: %s.\n", enableRM ? "enabled" : "disabled");
    printf("Protected mode support: %s.\n", enablePM ? "enabled" : "disabled");

    if(enablePM)
    {
        UntrappedIO_OUT_Handler = &HDPMIPT_UntrappedIO_Write;
        UntrappedIO_IN_Handler = &HDPMIPT_UntrappedIO_Read;
    }

#if SB16
    SBEMU_Init(MAIN_Options[OPT_IRQ].value, MAIN_Options[OPT_DMA].value, MAIN_Options[OPT_HDMA].value,
               MAIN_SB_DSPVersion[ MAIN_Options[OPT_TYPE].value ], &MAIN_Interrupt);
#else
    SBEMU_Init(MAIN_Options[OPT_IRQ].value, MAIN_Options[OPT_DMA].value, -1,
               MAIN_SB_DSPVersion[ MAIN_Options[OPT_TYPE].value ], &MAIN_Interrupt);
#endif
    VDMA_Virtualize(MAIN_Options[OPT_DMA].value, TRUE);
#if SB16
    if(MAIN_Options[OPT_TYPE].value == 6)
        VDMA_Virtualize(MAIN_Options[OPT_HDMA].value, TRUE);
#endif
    for(int i = 0; i < countof(MAIN_SB_IODT); ++i)
        MAIN_SB_IODT[i].port += MAIN_Options[OPT_ADDR].value;

    QEMM_IODT* SB_Iodt = MAIN_Options[OPT_OPL].value ? MAIN_SB_IODT : MAIN_SB_IODT+4;
    int SB_IodtCount = MAIN_Options[OPT_OPL].value ? countof(MAIN_SB_IODT) : countof(MAIN_SB_IODT)-4;
    
    printf("Sound Blaster emulation enabled at Address: %x, IRQ: %x, DMA: %x\n", MAIN_Options[OPT_ADDR].value, MAIN_Options[OPT_IRQ].value, MAIN_Options[OPT_DMA].value);

    BOOL QEMMInstalledVDMA = FALSE;
    BOOL QEMMInstalledVIRQ = FALSE;
    BOOL QEMMInstalledSB   = FALSE;
    if ( enableRM ) {
        QEMMInstalledVDMA = QEMM_Install_IOPortTrap( MAIN_VDMA_IODT, countof(MAIN_VDMA_IODT), &MAIN_VDMA_IOPT, NULL );
        QEMMInstalledVIRQ = QEMM_Install_IOPortTrap( MAIN_VIRQ_IODT, countof(MAIN_VIRQ_IODT), &MAIN_VIRQ_IOPT, NULL );
        QEMMInstalledSB   = QEMM_Install_IOPortTrap( SB_Iodt, SB_IodtCount, &MAIN_SB_IOPT, NULL );
#if QEMMPICTRAPDYN //will crash with VIRQ installed, do it temporarily. TODO: figure out why
        if ( QEMMInstalledVIRQ )
            QEMM_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT, FALSE );
#endif
        if( !QEMMInstalledVDMA || !QEMMInstalledVIRQ || !QEMMInstalledSB )
            printf("Error: Failed installing IO port trap for QEMM.\n");
    }

    BOOL HDPMIInstalledVDMA1  = FALSE;
    BOOL HDPMIInstalledVDMA2  = FALSE;
#if SB16
    BOOL HDPMIInstalledVHDMA1 = FALSE;
#endif
    BOOL HDPMIInstalledVIRQ1  = FALSE;
    BOOL HDPMIInstalledVIRQ2  = FALSE;
    BOOL HDPMIInstalledSB     = FALSE;
    if ( enablePM ) {
        HDPMIInstalledVDMA1  = HDPMIPT_Install_IOPortTrap( 0x0,  0xF, MAIN_VDMA_IODT, 14, &MAIN_VDMA_IOPT_PM1);
#if SB16
        HDPMIInstalledVDMA2  = HDPMIPT_Install_IOPortTrap( 0x81, 0x8B, MAIN_VDMA_IODT+14,  6, &MAIN_VDMA_IOPT_PM2);
        if ( MAIN_Options[OPT_TYPE].value > 5 )
            HDPMIInstalledVHDMA1 = HDPMIPT_Install_IOPortTrap(0xC4, 0xDE, MAIN_VDMA_IODT+20, 14, &MAIN_VHDMA_IOPT_PM1);
#else
        HDPMIInstalledVDMA2  = HDPMIPT_Install_IOPortTrap(0x81, 0x87, MAIN_VDMA_IODT+14,  3, &MAIN_VDMA_IOPT_PM2);
#endif
        HDPMIInstalledVIRQ1  = HDPMIPT_Install_IOPortTrap(0x20, 0x20, MAIN_VIRQ_IODT,     1, &MAIN_VIRQ_IOPT_PM1);
        if ( MAIN_Options[OPT_IRQ].value > 7 )
            HDPMIInstalledVIRQ2  = HDPMIPT_Install_IOPortTrap(0xA0, 0xA0, MAIN_VIRQ_IODT+1,   1, &MAIN_VIRQ_IOPT_PM2);
        HDPMIInstalledSB     = HDPMIPT_Install_IOPortTrap(MAIN_Options[OPT_ADDR].value, MAIN_Options[OPT_ADDR].value+0x0F, SB_Iodt, SB_IodtCount, &MAIN_SB_IOPT_PM);
        HDPMI_PrintPorts(); /* for debugging */
        if( !HDPMIInstalledVDMA1 || !HDPMIInstalledVDMA2
#if SB16
           || ( MAIN_Options[OPT_TYPE].value > 5 && !HDPMIInstalledVHDMA1 )
#endif
           || !HDPMIInstalledVIRQ1
           || ( MAIN_Options[OPT_IRQ].value > 7 && !HDPMIInstalledVIRQ2 )
           || !HDPMIInstalledSB )
            printf("Error: Failed installing IO port trap for HDPMI.\n");
    }

    BOOL PM_ISR = HDPMIPT_InstallISR(PIC_IRQ2VEC( aui.card_irq), MAIN_InterruptPM, &MAIN_TimerIntHandlePM );

    PIC_UnmaskIRQ(aui.card_irq);

    AU_prestart(&aui);
    AU_start(&aui);

    BOOL TSR = TRUE;
    if(!PM_ISR
       || ( enableRM && ( !QEMMInstalledVDMA || !QEMMInstalledVIRQ || !QEMMInstalledSB ) )
       || ( enablePM && ( !HDPMIInstalledVDMA1 || !HDPMIInstalledVDMA2
#if SB16
       || ( MAIN_Options[OPT_TYPE].value > 5 && !HDPMIInstalledVHDMA1 )
#endif
       || !HDPMIInstalledVIRQ1
       || ( MAIN_Options[OPT_IRQ].value > 7 && !HDPMIInstalledVIRQ2 )
       || !HDPMIInstalledSB ) )
       || !(TSR=DPMI_TSR()) )
    {
        if (PM_ISR) HDPMIPT_UninstallISR( &MAIN_TimerIntHandlePM );

        if( enableRM ) {
            if( MAIN_Options[OPT_OPL].value ) QEMM_Uninstall_IOPortTrap( &OPL3IOPT, TRUE );

            if ( QEMMInstalledVDMA ) QEMM_Uninstall_IOPortTrap( &MAIN_VDMA_IOPT, TRUE );
#if !QEMMPICTRAPDYN
            if ( QEMMInstalledVIRQ ) QEMM_Uninstall_IOPortTrap( &MAIN_VIRQ_IOPT, TRUE );
#endif
            if ( QEMMInstalledSB )   QEMM_Uninstall_IOPortTrap( &MAIN_SB_IOPT, TRUE );
        }

        if( enablePM ) {
            if( MAIN_Options[OPT_OPL].value ) HDPMIPT_Uninstall_IOPortTrap( &OPL3IOPT_PM );
            if ( HDPMIInstalledVDMA1 )  HDPMIPT_Uninstall_IOPortTrap( &MAIN_VDMA_IOPT_PM1 );
            if ( HDPMIInstalledVDMA2 )  HDPMIPT_Uninstall_IOPortTrap( &MAIN_VDMA_IOPT_PM2 );
#if SB16
            if ( HDPMIInstalledVHDMA1 ) HDPMIPT_Uninstall_IOPortTrap( &MAIN_VHDMA_IOPT_PM1 );
#endif
            if ( HDPMIInstalledVIRQ1 )  HDPMIPT_Uninstall_IOPortTrap( &MAIN_VIRQ_IOPT_PM1 );
            if ( HDPMIInstalledVIRQ2 )  HDPMIPT_Uninstall_IOPortTrap( &MAIN_VIRQ_IOPT_PM2 );
            if ( HDPMIInstalledSB )     HDPMIPT_Uninstall_IOPortTrap( &MAIN_SB_IOPT_PM );
        }
        if(!TSR) printf("Error: Failed installing TSR.\n");
    }
    return 1;
}

static void MAIN_InterruptPM()
{
    if(aui.card_handler->irq_routine && aui.card_handler->irq_routine(&aui)) //check if the irq belong the sound card
    {
        MAIN_Interrupt();
        PIC_SendEOIWithIRQ(aui.card_irq);
    }
    else
    {
        //DPMI_CallOldISR(&MAIN_TimerIntHandlePM);
        _hdpmi_CallOldISR( &MAIN_TimerIntHandlePM );
        //PIC_UnmaskIRQ(aui.card_irq);
    }
}

static void MAIN_Interrupt()
{
#if 0
    aui.card_outbytes = aui.card_dmasize;
    int space = AU_cardbuf_space(&aui)+2048;
    //_LOG("int space: %d\n", space);
    int samples = space / sizeof(int16_t) / 2 * 2;
    //int samples = 22050/18*2;
    _LOG("samples: %d %d\n", 22050/18*2, space/4*2);
    static int cur = 0;
    aui.samplenum = min(samples, TEST_SampleLen-cur);
    aui.pcm_sample = TEST_Sample + cur;
    //_LOG("cur: %d %d\n",cur,aui.samplenum);
    cur += aui.samplenum;
    cur -= AU_writedata(&aui);
#else

    int32_t vol;
    int32_t voicevol;
    int32_t midivol;
    if(MAIN_Options[OPT_TYPE].value < 4) //SB2.0 and before
    {
        vol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_MASTERVOL) >> 1)*256/7;
        voicevol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_VOICEVOL) >> 1)*256/3;
        midivol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_MIDIVOL) >> 1)*256/7;
    }
    else if(MAIN_Options[OPT_TYPE].value == 6) //SB16
    {
        vol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_MASTERSTEREO)>>4)*256/15; //4:4
        voicevol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_VOICESTEREO)>>4)*256/15; //4:4
        midivol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_MIDISTEREO)>>4)*256/15; //4:4
        //_LOG("vol: %d, voicevol: %d, midivol: %d\n", vol, voicevol, midivol);        
    }
    else //SBPro
    {
        vol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_MASTERSTEREO)>>5)*256/7; //3:1:3:1 stereo usually the same for both channel for games?;
        voicevol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_VOICESTEREO)>>5)*256/7; //3:1:3:1
        midivol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_MIDISTEREO)>>5)*256/7;
    }
    if(MAIN_SB_VOL != vol*MAIN_GLB_VOL/9)
    {
        _LOG("set sb volume:%d %d\n", MAIN_SB_VOL, vol*MAIN_GLB_VOL/9);
        MAIN_SB_VOL = vol*MAIN_GLB_VOL/9;
        AU_setmixer_one(&aui, AU_MIXCHAN_MASTER, MIXER_SETMODE_ABSOLUTE, vol*100/256);
    }

    aui.card_outbytes = aui.card_dmasize;
    int samples = AU_cardbuf_space(&aui) / sizeof(int16_t) / 2; //16 bit, 2 channels
    //_LOG("samples:%d\n",samples);

    if(samples == 0)
        return;

    BOOL digital = SBEMU_HasStarted();
    int dma = SBEMU_GetDMA();
    int32_t DMA_Count = VDMA_GetCounter(dma); //count in bytes (8bit dma)

    if(digital)//&& DMA_Count != 0x10000) //-1(0xFFFF)+1=0
    {
        uint32_t DMA_Addr = VDMA_GetAddress(dma);
        int32_t DMA_Index = VDMA_GetIndex(dma);
        uint32_t SB_Bytes = SBEMU_GetSampleBytes();
        uint32_t SB_Pos = SBEMU_GetPos();
        uint32_t SB_Rate = SBEMU_GetSampleRate();
        int samplesize = SBEMU_GetBits()/8;
        int channels = SBEMU_GetChannels();
        //_LOG("sample rate: %d %d\n", SB_Rate, aui.freq_card);
        //_LOG("DMA index: %x\n", DMA_Index);
        //_LOG("digital start\n");
        int pos = 0;
#if QEMMPICTRAPDYN
        BOOL bTrapPic = FALSE;
#endif
        do {
            /* check if the current DMA address is within the mapped region.
             * if no, release current mapping region.
             */
            if(MAIN_DMA_MappedAddr != 0
             && !(DMA_Addr >= MAIN_DMA_Addr && DMA_Addr+DMA_Index+DMA_Count <= MAIN_DMA_Addr+MAIN_DMA_Size))
            {
                if(MAIN_DMA_MappedAddr > 1024*1024)
                    DPMI_UnmappMemory(MAIN_DMA_MappedAddr);
                MAIN_DMA_MappedAddr = 0;
            }
            /* if there's no mapped region, create one that covers current DMA op
             */
            if(MAIN_DMA_MappedAddr == 0)
            {
                MAIN_DMA_Addr = DMA_Addr&~0xFFF;
                MAIN_DMA_Size = align(max(DMA_Addr-MAIN_DMA_Addr+DMA_Index+DMA_Count, 64*1024*2), 4096);
                MAIN_DMA_MappedAddr = (DMA_Addr+DMA_Index+DMA_Count <= 1024*1024) ? (DMA_Addr&~0xFFF) : DPMI_MapMemory(MAIN_DMA_Addr, MAIN_DMA_Size);
            }
            //_LOG("DMA_ADDR:%x, %x, %x\n",DMA_Addr, MAIN_DMA_Addr, MAIN_DMA_MappedAddr);

            int count = samples - pos;
            BOOL resample = TRUE; //don't resample if sample rates are close
            if(SB_Rate < aui.freq_card)
                //count = max(channels, count/((aui.freq_card+SB_Rate-1)/SB_Rate));
                count = max(1, count * SB_Rate / aui.freq_card );
            else if(SB_Rate > aui.freq_card)
                //count *= (SB_Rate + aui.freq_card/2)/aui.freq_card;
                count = count * SB_Rate / aui.freq_card;
            else
            	resample = FALSE;
            count = min(count, max(1,(DMA_Count) / samplesize / channels)); //stereo initial 1 byte
            count = min(count, max(1,(SB_Bytes - SB_Pos) / samplesize / channels )); //stereo initial 1 byte. 1 /2channel = 0, make it 1
            _LOG("samples:%d %d %d, %d %d, %d %d\n", samples, pos+count, count, DMA_Count, DMA_Index, SB_Bytes, SB_Pos);
            int bytes = count * samplesize * channels;

            /* copy samples to our PCM buffer
             */
            if(MAIN_DMA_MappedAddr == 0) //map failed?
                memset(MAIN_PCM+pos*2, 0, bytes);
            else
                DPMI_CopyLinear(DPMI_PTR2L(MAIN_PCM+pos*2), MAIN_DMA_MappedAddr+(DMA_Addr-MAIN_DMA_Addr)+DMA_Index, bytes);

            /* format conversion needed? */
            if( samplesize != 2 )
                cv_bits_n_to_m(MAIN_PCM+pos*2, count*channels, samplesize, 2);
            if( resample ) /*SB_Rate != aui.freq_card*/
                count = mixer_speed_lq(MAIN_PCM+pos*2, count*channels, channels, SB_Rate, aui.freq_card)/channels;
            if( channels == 1) //should be the last step
                cv_channels_1_to_n(MAIN_PCM+pos*2, count, 2, 2);
            pos += count;
            //_LOG("samples:%d %d %d\n", count, pos, samples);
            DMA_Index = VDMA_SetIndexCounter(dma, DMA_Index+bytes, DMA_Count-bytes);
            //int LastDMACount = DMA_Count;
            DMA_Count = VDMA_GetCounter(dma);
            SB_Pos = SBEMU_SetPos(SB_Pos+bytes);
            //_LOG("SB bytes: %d %d\n", SB_Pos, SB_Bytes);
            if(SB_Pos >= SB_Bytes)
            {
                //_LOG("INT:%d,%d,%d,%d\n",MAIN_SBBytes,SBEMU_GetSampleBytes(),MAIN_DMAIndex,DMA_Count);
                //_LOG("SBEMU: Auto: %d\n",SBEMU_GetAuto());
                if(!SBEMU_GetAuto())
                    SBEMU_Stop();
                SB_Pos = SBEMU_SetPos(0);
#if QEMMPICTRAPDYN
                if(MAIN_Options[OPT_RM].value && !bTrapPic ) {
                    QEMM_Install_IOPortTrap(MAIN_VIRQ_IODT, countof(MAIN_VIRQ_IODT), &MAIN_VIRQ_IOPT, (QEMM_IODT_LINK *)(MAIN_VIRQ_IOPT.memory) );
                    bTrapPic = TRUE;
                }
#endif
                VIRQ_Invoke(SBEMU_GetIRQ());

                SB_Bytes = SBEMU_GetSampleBytes();
                SB_Pos = SBEMU_GetPos();
                SB_Rate = SBEMU_GetSampleRate();
                //if(LastDMACount <= 32) //detection routine?
                    //break; fix crash in virtualbox.
                //    pos = 0;
                //incase IRQ handler re-programs DMA
                DMA_Index = VDMA_GetIndex(dma);
                DMA_Count = VDMA_GetCounter(dma);
                DMA_Addr = VDMA_GetAddress(dma);
            }
        } while(VDMA_GetAuto(dma) && (pos < samples) && SBEMU_HasStarted());
#if QEMMPICTRAPDYN
        if( bTrapPic )
            QEMM_Uninstall_IOPortTrap(&MAIN_VIRQ_IOPT, FALSE );
#endif

        //_LOG("digital end %d %d\n", samples, pos);
        //for(int i = pos; i < samples; ++i)
        //    MAIN_PCM[i*2+1] = MAIN_PCM[i*2] = 0;
        samples = min(samples, pos);
    }
    else if(!MAIN_Options[OPT_OPL].value)
        memset(MAIN_PCM, 0, samples*sizeof(int16_t)*2); //output muted samples.

    if(MAIN_Options[OPT_OPL].value)
    {
        int16_t* pcm = digital ? MAIN_OPLPCM : MAIN_PCM;
        OPL3EMU_GenSamples(pcm, samples); //will generate samples*2 if stereo
        //always use 2 channels
        int channels = OPL3EMU_GetMode() ? 2 : 1;
        if(channels == 1)
            cv_channels_1_to_n(pcm, samples, 2, SBEMU_BITS/8);

        if(digital)
        {
            for(int i = 0; i < samples*2; ++i)
            {
                int a = (int)(MAIN_PCM[i]*voicevol/256) + 32768;
                int b = (int)(MAIN_OPLPCM[i]*midivol/256) + 32768;
                int mixed = (a < 32768 || b < 32768) ? (a*b/32768) : ((a+b)*2 - a*b/32768 - 65536);
                if(mixed == 65536) mixed = 65535;
                MAIN_PCM[i] = mixed - 32768;
            }
        }
        else for(int i = 0; i < samples*2; ++i)
            MAIN_PCM[i] = MAIN_PCM[i]*midivol/256;
    }
    else if(digital)
        for(int i = 0; i < samples*2; ++i)
            MAIN_PCM[i] = MAIN_PCM[i]*voicevol/256;
    samples *= 2; //to stereo

    aui.samplenum = samples;
    aui.pcm_sample = MAIN_PCM;

    AU_writedata(&aui);

    _LOG("MAIN INT END\n");
#endif
}
