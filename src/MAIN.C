
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include <go32.h>
#include <sys/exceptn.h>

#include "SBEMUCFG.H"
#include "PLATFORM.H"
#include "PIC.H"
#include "DPMIHLP.H"
#include "PTRAP.H"
#include "VDMA.H"
#include "VIRQ.H"
#include "VOPL3.H"
#include "VSB.H"

#include <MPXPLAY.H>
#include <MIX_FUNC.H>

#define PREMAPDMA 0 /* 1=map full 16MB ISA address range */
#define SUPPSAFE 0 /* 1=support /SAFE cmdline option ( hardly needed ) */

#define MAIN_PCM_SAMPLESIZE 16384 /* sample buffer size */

static mpxplay_audioout_info_s aui = {0};

/* for AU_setrate() - use fixed rate */
static mpxplay_audio_decoder_info_s adi = {
	NULL, /* private data */
	0, /* infobits */
	MPXPLAY_WAVEID_PCM_SLE, /* 16-bit samples */
	SBEMU_SAMPLERATE, /* 22050 or 44100 */
	SBEMU_CHANNELS, /* channels in file (not used) */
	SBEMU_CHANNELS, /* decoded channels */
	NULL, /* output channel matrix */
	SBEMU_BITS, /* 16 */
	SBEMU_BITS/8, /* bytes per sample */
	0}; /* bitrate */

static int16_t MAIN_OPLPCM[MAIN_PCM_SAMPLESIZE+256];
static int16_t MAIN_PCM[MAIN_PCM_SAMPLESIZE+256];

uint8_t bDbgInit = 1; /* 1=debug output to DOS, 0=low-level */


#if PREMAPDMA
static uint32_t MAIN_MappedBase; /* linear address mapped ISA DMA region (0x000000 - 0xffffff) */
#else
static uint32_t MAIN_DMA_Addr = 0;
static uint32_t MAIN_DMA_Size = 0;
static uint32_t MAIN_DMA_MappedAddr = 0;
#endif
static uint16_t MAIN_SB_VOL = 0; //initial set volume will cause interrupt missing?
static uint16_t MAIN_GLB_VOL = 0;

void MAIN_Interrupt();
static int MAIN_InterruptPM();

bool _hdpmi_InstallISR( uint8_t i, int(*ISR)(void) );
bool _hdpmi_UninstallISR( void );
bool _hdpmi_InstallInt31( uint8_t );
bool _hdpmi_UninstallInt31( void );

static bool PM_ISR; /* 1=pm ISR installed */
static bool bQemm = false;
static bool bHdpmi = false;
static int bHelp = false;

#if SB16
struct globalvars gvars = { 0x220, 7, 1, 0, 5, true, true, true, 7 };
#else
struct globalvars gvars = { 0x220, 7, 1, 5, true, true, true, 7 };
#endif

static struct {
    const char* option;
    const char* desc;
    int *pValue;
} MAIN_Options[] =
{
    "/?", "Show help", &bHelp,
    "/A", "Specify IO address, valid value: 220,240", &gvars.base,
    "/I", "Specify IRQ number, valid value: 5,7", &gvars.irq,
    "/D", "Specify DMA channel, valid value: 0,1,3", &gvars.dma,
#if SB16
    "/H", "Specify High DMA channel, valid value: 5,6,7", &gvars.hdma,
    "/T", "Specify SB Type, valid value: 0-6", &gvars.type,
#else
    "/T", "Specify SB Type, valid value: 0-5", &gvars.type,
#endif
    "/OPL", "Enable OPL3 emulation", &gvars.opl3,
    "/PM", "Support protected mode games", &gvars.pm,
    "/RM", "Support real mode games", &gvars.rm,
#if SUPPSAFE
	"/SAFE", "Safe mode - may be needed by some protected-mode programs", &gvars.safe,
#endif
	"/VOL", "Set master volume (0-9)", &gvars.vol,
    "/O", "Select output (HDA only); 0=lineout, 1=speaker, 2=headphone", &gvars.pin,
    "/DEV", "Set device index (HDA only); in case there exist multiple devices", &gvars.device,
    NULL, NULL, 0,
};
enum EOption
{
    OPT_Help,
    OPT_ADDR,
    OPT_IRQ,
    OPT_DMA,
#if SB16
    OPT_HDMA,
#endif
    OPT_TYPE,
    OPT_OPL,
    OPT_PM,
    OPT_RM,
#if SUPPSAFE
	OPT_SAFE,
#endif
	OPT_VOL,
    OPT_OUTPUT,
    OPT_DEVIDX,
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

#if PREMAPDMA
static uint32_t MapFirst16M( void )
///////////////////////////////////
{
    /* ensure the mapping doesn't cover linear address 4M.
     * to make this work, allocated uncommitted memory block of 63 MB.
     */
    uint16_t tmp;
    uint32_t result;
    __dpmi_meminfo info;

    /* ensure the mapping returns linear address beyond 64M*/
    info.address = 0;
    info.size = 0x3F00000;
    tmp = __dpmi_allocate_linear_memory( &info, 0 );
    result = DPMI_MapMemory( 0, 0x1000000 ); /* map the whole region that may be used by ISA DMA */
    if ( tmp != -1 )
        __dpmi_free_memory( info.handle );
    return( result );
}
#endif

static int IsInstalled( void )
//////////////////////////////
{
    uint8_t bSB;
	asm("mov $0x226, %%dx \n\t"
		"mov $1, %%al \n\t"
		"out %%al, %%dx \n\t"
		"in %%dx, %%al \n\t"
		"xor %%al, %%al \n\t"
		"out %%al, %%dx \n\t"
		"mov $0x22A, %%dx \n\t"
		"in %%dx, %%al \n\t"
		"mov %%al, %0 \n\t"
		: "=m"(bSB)
		:: "edx", "eax"
	   );
    return( bSB == 0xAA );
}

static bool InstallISR( uint8_t interrupt, int(*ISR)(void) )
////////////////////////////////////////////////////////////
{
    if ( _hdpmi_InstallISR( interrupt, ISR ) ) {
        return ( _hdpmi_InstallInt31( interrupt ) );
    }
    return false;
}

static bool UninstallISR( void )
////////////////////////////////
{
    /* first uninstall int 31h, then ISR! */
    _hdpmi_UninstallInt31();
    return ( _hdpmi_UninstallISR() );
}


static void ReleaseRes( void )
//////////////////////////////
{
	if (PM_ISR) UninstallISR();

	if( gvars.rm )
		PTRAP_Uninstall_RM_PortTraps();

	if( gvars.pm ) {
		PTRAP_Uninstall_PM_PortTraps();
	}
    return;
}

/* uninstall SBEMU.
 * uninstall a protected-mode TSR is best done from real-mode.
 * for SBEMU, it's triggered by an OUT 226h, 55h ( see sbemu.c ).
 * note: no printf() possible here, since stdio files are closed.
 */
void MAIN_Uninstall( void )
///////////////////////////
{
	DPMI_REG r = {0};
	ReleaseRes();
	AU_close( &aui );
	/* set TSR's parent PSP to current PSP;
	 * set TSR's int 22h to current PSP:0000;
	 * switch to TSR PSP
	 * run an int 21h, ah=4Ch in pm
	 * todo: adjust value at psp:2Eh
	 */
	r.w.ax = 0x5100; /* get current PSP in BX (segment) */
	DPMI_CallRealModeINT(0x21, &r);
#if 0
	uint32_t dwSSSP = DPMI_LoadD( (r.w.bx << 4) + 0x2E );
	DPMI_StoreD( _go32_info_block.linear_address_of_original_psp+0x2E, dwSSSP );
#endif
	DPMI_StoreW( _go32_info_block.linear_address_of_original_psp+0xA, 0 );
	DPMI_StoreW( _go32_info_block.linear_address_of_original_psp+0xC, r.w.bx );
	DPMI_StoreW( _go32_info_block.linear_address_of_original_psp+0x16, r.w.bx );
	r.w.bx = _go32_info_block.linear_address_of_original_psp >> 4;
	r.w.ax = 0x5000;
	DPMI_CallRealModeINT(0x21, &r);
	dbgprintf("MAIN_Uninstall: all cleanup done, terminating\n");
	asm(
		"mov $0x4C00, %ax \n\t"
		"int $0x21" /* not supposed to return! */
	);
	return;
}

#if REINITOPL
void MAIN_ReinitOPL( void )
///////////////////////////
{
	if( gvars.opl3 ) {
		uint8_t buffer[200];
		asm("fsave %0"::"m"(buffer));
		VOPL3_Reinit(aui.freq_card);
		asm("frstor %0"::"m"(buffer));
	}
}
#endif

#if SB16
#define MAXTYPE 6
#define HELPNOTE "\n if /A /I /D /H /T set, they will internally override the BLASTER values.\n"
#else
#define MAXTYPE 5
#define HELPNOTE "\n if /A /I /D /T set, they will internally override the BLASTER values.\n"
#endif

int main(int argc, char* argv[])
////////////////////////////////
{

    //parse BLASTER env first.
    char* blaster = getenv("BLASTER");
    if(blaster != NULL) {
        char c;
        while((c=toupper(*(blaster++)))) {
            if(c == 'I')
                *MAIN_Options[OPT_IRQ].pValue = *(blaster++) - '0';
            else if(c == 'D')
                *MAIN_Options[OPT_DMA].pValue = *(blaster++) - '0';
            else if(c == 'A')
                *MAIN_Options[OPT_ADDR].pValue = strtol(blaster, &blaster, 16);
            else if(c =='T')
                *MAIN_Options[OPT_TYPE].pValue = *(blaster++) - '0';
#if SB16
            else if(c =='H')
                *MAIN_Options[OPT_HDMA].pValue = *(blaster++) - '0';
#endif
        }
    }

    /* check cmdline arguments */
    for(int i = 1; i < argc; ++i) {
        int j;
        for( j = 0; j < OPT_COUNT; ++j) {
            int len = strlen(MAIN_Options[j].option);
            if( memicmp(argv[i], MAIN_Options[j].option, len) == 0 ) {
                if ( argv[i][len] >= '0' && argv[i][len] <= '9' ) {
                    *MAIN_Options[j].pValue = strtol(&argv[i][len], NULL, (j == OPT_ADDR) ? 16 : 10 );
                    break;
                } else if ( argv[i][len] == 0 && *MAIN_Options[j].pValue == false ) {
                    *MAIN_Options[j].pValue = true;
                    break;
                }
            }
        }
        if ( j == OPT_COUNT )
            bHelp = true;
    }

    /* if -? or unrecognised option was entered, display help and exit */
    if( bHelp ) {
        bHelp = false;
        printf("SBEMU: Sound Blaster emulation on AC97. Usage:\n");
        int i = 0;
        while(MAIN_Options[i].option) {
            printf(" %-8s: %s. Default: %x\n", MAIN_Options[i].option, MAIN_Options[i].desc, *MAIN_Options[i].pValue);
            ++i;
        }
        printf("\nNote: SBEMU will read BLASTER environment variable and use it, " HELPNOTE );
        printf("\nSource code used from:\n    MPXPlay (https://mpxplay.sourceforge.net/)\n    DOSBox (https://www.dosbox.com/)\n");
        return 0;
    }

    if( gvars.base != 0x220 && gvars.base != 0x240 )
    {
        printf("Error: invalid IO port address: %x.\n", gvars.base );
        return 1;
    }
    if( gvars.irq != 0x5 && gvars.irq != 0x7 )
    {
        printf("Error: invalid IRQ: %d.\n", gvars.irq );
        return 1;
    }
    if( gvars.dma != 0x0 && gvars.dma != 1 && gvars.dma != 3 )
    {
        printf("Error: invalid DMA channel.\n");
        return 1;
    }
#if SB16
    if( gvars.hdma != 0x0 && ( gvars.hdma <= 4 || gvars.hdma > 7))
    {
        printf("Error: invalid HDMA channel: %u\n", gvars.hdma );
        return 1;
    }
#endif
    if( gvars.type <= 0 || gvars.type > MAXTYPE )
    {
        printf("Error: invalid SB Type: %d\n", gvars.type );
        return 1;
    }
    if( gvars.pin < 0 || gvars.pin > 2)
    {
        printf("Error: Invalid Output: %d\n", gvars.pin );
        return 1;
    }
     if( gvars.vol < 0 || gvars.vol > 9)
    {
        printf("Error: Invalid Volume.\n");
        return 1;
    }
    //TODO: alter BLASTER env?

    DPMI_Init();

    if ( IsInstalled() ) {
        printf("SB found - probably SBEMU already installed.\n" );
        return(0);
    }

    if( gvars.rm )
    {
        int bcd = PTRAP_GetQEMMVersion();
        //dbgprintf("QEMM version: %x.%02x\n", bcd>>8, bcd&0xFF);
        if(bcd < 0x703)
        {
            printf("QEMM not installed, or version below 7.03: %x.%02x, disable real mode support.\n", bcd>>8, bcd&0xFF);
            gvars.rm = false;
        }
    }
    if( gvars.pm )
    {
        bool hasHDPMI = PTRAP_DetectHDPMI(); //another DPMI host used other than HDPMI
        if(!hasHDPMI)
            printf("HDPMI not installed, disable protected mode support.\n");
        gvars.pm = hasHDPMI;
    }
    if( !gvars.pm && !gvars.rm )
    {
        printf("Both real mode & protected mode support are disabled, exiting.\n");
        return 1;
    }
#if SUPPSAFE
    if(MAIN_Options[OPT_SAFE].value)
        VIRQ_SafeCall();
#else
    VIRQ_Init();
#endif
    aui.card_select_config = gvars.pin;
    aui.card_select_devicenum = gvars.device;
    if ( !AU_init( &aui ) ) {
        printf("No soundcard found!\n");
        return 1;
    }
    printf("Found sound card: %s\n", aui.card_handler->shortname);
    if( aui.card_irq == gvars.irq ) {
        printf("Sound card IRQ conflict, abort.\n");
        return 1;
    }
    AU_ini_interrupts(&aui);
    AU_setmixer_init(&aui);
    AU_setmixer_outs(&aui, MIXER_SETMODE_ABSOLUTE, 95);
    MAIN_GLB_VOL = gvars.vol;
    MAIN_SB_VOL = 256 * MAIN_GLB_VOL / 9;
    AU_setmixer_one(&aui, AU_MIXCHAN_MASTER, MIXER_SETMODE_ABSOLUTE, MAIN_GLB_VOL * 100/9 );
    AU_setrate(&aui, &adi);

    if( gvars.rm ) {
        gvars.rm = PTRAP_Prepare_RM_PortTrap();
        if ( !gvars.rm ) {
            printf("Error: Failed setting IO port handler for real-mode.\n");
            return 1;
        }
    }

    printf("Real mode support: %s.\n", gvars.rm ? "enabled" : "disabled");
    printf("Protected mode support: %s.\n", gvars.pm ? "enabled" : "disabled");

    if( gvars.pm ) {
        UntrappedIO_OUT_Handler = &PTRAP_UntrappedIO_OUT;
        UntrappedIO_IN_Handler = &PTRAP_UntrappedIO_IN;
    }

#if SB16
    if ( gvars.type < 6 )
        gvars.hdma = 0;
#endif

	PTRAP_Prepare( gvars.opl3, gvars.base, gvars.dma, gvars.hdma );


#if SB16
    VSB_Init( gvars.irq, gvars.dma, gvars.hdma, gvars.type );
#else
    VSB_Init( gvars.irq, gvars.dma, 0, gvars.type );
#endif
    VDMA_Virtualize( gvars.dma, true );
#if SB16
    if( gvars.hdma > 0 )
        VDMA_Virtualize( gvars.hdma, true );
#endif

    if ( gvars.rm ) {
        if ((bQemm = PTRAP_Install_RM_PortTraps()) == 0 )
            printf("Error: Failed installing IO port trap for real-mode.\n");
    }
    if ( gvars.pm ) {
        if(( bHdpmi = PTRAP_Install_PM_PortTraps()) == 0 )
            printf("Error: Failed installing IO port trap for protected-mode.\n");
#ifdef _DEBUG
        //PTRAP_PrintPorts(); /* for debugging */
#endif
    }

    if( gvars.opl3 ) {
        VOPL3_Init(aui.freq_card);
        printf("OPL3 emulation enabled at port 388h (%u Hz).\n", aui.freq_card );
    }

#if SB16
    printf("Sound Blaster emulation enabled at Address=%x, IRQ=%d, DMA=%d, HDMA=%d, TYPE=%d\n",
           gvars.base, gvars.irq, gvars.dma, gvars.hdma, gvars.type );
#else
    printf("Sound Blaster emulation enabled at Address=%x, IRQ=%u, DMA=%u\n", gvars.base, gvars.irq, gvars.dma );
#endif

    PM_ISR = InstallISR(PIC_IRQ2VEC( aui.card_irq), &MAIN_InterruptPM );

    PIC_UnmaskIRQ(aui.card_irq);

    bDbgInit = 0;
    AU_prestart(&aui);
    AU_start(&aui);

#if PREMAPDMA
    /* Map the full first 16M to simplify DMA mem access */
    MAIN_MappedBase = MapFirst16M();
    dbgprintf("MappedBase=%x\n", MAIN_MappedBase );
#endif
    /* temp alloc a 64 kB buffer so it will belong to THIS client. Any dpmi memory allocation
     * while another client is active will result in problems, since that memory is released when
     * the client exits.
     */
    void * p;
    if (p = malloc( 0x10000 ) )
        free( p );

    if( PM_ISR && ( bQemm || (!gvars.rm) ) && ( bHdpmi || (!gvars.pm) ) ) {
        DPMI_REG r = {0};
        uint32_t psp = _go32_info_block.linear_address_of_original_psp;
        __dpmi_free_dos_memory( DPMI_LoadW( psp+0x2C ) );
        DPMI_StoreW( psp+0x2C, 0 );
        for ( int i = 0; i < 5; i++ )
            _dos_close( i );
        __djgpp_exception_toggle();
        asm("push $0\n\t" "pop %gs\n\t" "push $0\n\t" "pop %fs"); /* clear fs/gs */
        r.w.dx= 0x10; /* only psp */
        r.w.ax = 0x3100;
        DPMI_CallRealModeINT(0x21, &r); //won't return on success
    }
    ReleaseRes();
    printf("Error: Failed installing TSR.\n");
    return 1;
}

static int MAIN_InterruptPM( void )
///////////////////////////////////
{
    if(aui.card_handler->irq_routine && aui.card_handler->irq_routine(&aui)) //check if the irq belong the sound card
    {
#if SETIF
        _enable_ints();
#endif
        MAIN_Interrupt();
        PIC_SendEOIWithIRQ(aui.card_irq);
        return(1);
    }
    return(0);
}

void MAIN_Interrupt()
/////////////////////
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
    if(MAIN_SB_VOL != vol * MAIN_GLB_VOL / 9)
    {
        //dbgprintf("set sb volume:%d %d\n", MAIN_SB_VOL, vol*MAIN_GLB_VOL/9);
        MAIN_SB_VOL =  vol * MAIN_GLB_VOL / 9;
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
            if( MAIN_DMA_MappedAddr != 0
             && !(DMA_Addr >= MAIN_DMA_Addr && DMA_Addr + DMA_Index + DMA_Count <= MAIN_DMA_Addr + MAIN_DMA_Size ))
            {
                if(MAIN_DMA_MappedAddr > 1024*1024)
                    DPMI_UnmapMemory( MAIN_DMA_MappedAddr );
                MAIN_DMA_MappedAddr = 0;
            }
            /* if there's no mapped region, create one that covers current DMA op
             */
            if(MAIN_DMA_MappedAddr == 0) {
                MAIN_DMA_Addr = DMA_Addr & ~0xFFF;
                MAIN_DMA_Size = align( max( DMA_Addr - MAIN_DMA_Addr + DMA_Index + DMA_Count, 64*1024*2 ), 4096);
                MAIN_DMA_MappedAddr = (DMA_Addr + DMA_Index + DMA_Count <= 1024*1024) ? ( DMA_Addr & ~0xFFF) : DPMI_MapMemory( MAIN_DMA_Addr, MAIN_DMA_Size );
                // dbgprintf("ISR: chn=%d DMA_Addr/Index/Count=%x/%x/%x MAIN_DMA_Addr/Size/MappedAddr=%x/%x/%x\n", dma, DMA_Addr, DMA_Index, DMA_Count, MAIN_DMA_Addr, MAIN_DMA_Size, MAIN_DMA_MappedAddr );
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
            DPMI_CopyLinear(DPMI_PTR2L(MAIN_PCM + pos * 2), MAIN_MappedBase + DMA_Addr + DMA_Index, bytes);
#else
            if( MAIN_DMA_MappedAddr == 0) {//map failed?
                memset(MAIN_PCM + pos * 2, 0, bytes);
            } else
                DPMI_CopyLinear(DPMI_PTR2L(MAIN_PCM + pos * 2), MAIN_DMA_MappedAddr+(DMA_Addr-MAIN_DMA_Addr)+DMA_Index, bytes);
#endif

            /* format conversion needed? */
#if ADPCM
            if(VSB_GetBits()<8) //ADPCM  8bit
                count = VSB_DecodeADPCM((uint8_t*)(MAIN_PCM + pos * 2), bytes);
#endif
            if( samplesize != 2 )
                cv_bits_n_to_m( MAIN_PCM + pos * 2, count * channels, samplesize, 2);
            if( resample ) /* SB_Rate != aui.freq_card*/
                count = mixer_speed_lq( MAIN_PCM + pos * 2, count * channels, channels, SB_Rate, aui.freq_card)/channels;
            if( channels == 1) //should be the last step
                cv_channels_1_to_n( MAIN_PCM + pos * 2, count, 2, 2);
            pos += count;
            //dbgprintf("samples:%d %d %d\n", count, pos, samples);
            DMA_Index = VDMA_SetIndexCounter(dma, DMA_Index+bytes, DMA_Count-bytes);
            //int LastDMACount = DMA_Count;
            DMA_Count = VDMA_GetCounter( dma );
            SB_Pos = VSB_SetPos( SB_Pos + bytes );
            if(SB_Pos >= SB_Bytes)
            {
                dbgprintf("MAIN_Interrupt: SB_Pos >= SB_Bytes: %u/%u, bytes/count=%u/%u, dma=%X/%u\n", SB_Pos, SB_Bytes, bytes, count, VDMA_GetAddress(dma), VDMA_GetCounter(dma) );
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

        dbgprintf("MAIN_Interrupt: pos/samples=%u/%u, running=%u\n", pos, samples, VSB_Running() );
        //for(int i = pos; i < samples; ++i)
        //    MAIN_PCM[i*2+1] = MAIN_PCM[i*2] = 0;
        samples = min(samples, pos);
    }
    else if( gvars.opl3 )
        memset( MAIN_PCM, 0, samples * sizeof(int16_t) * 2 ); //output muted samples.

    if( gvars.opl3 ) {
        int16_t* pcm = digital ? MAIN_OPLPCM : MAIN_PCM;
        VOPL3_GenSamples(pcm, samples); //will generate samples*2 if stereo
        //always use 2 channels
        int channels = VOPL3_GetMode() ? 2 : 1;
        if(channels == 1)
            cv_channels_1_to_n(pcm, samples, 2, SBEMU_BITS/8);

        if( digital ) {
            for(int i = 0; i < samples*2; ++i) {
                int a = (int)(MAIN_PCM[i] * voicevol / 256) + 32768;
                int b = (int)(MAIN_OPLPCM[i] * midivol / 256) + 32768;
                int mixed = (a < 32768 || b < 32768) ? ( a * b / 32768) : ((a+b) * 2 - a*b/32768 - 65536);
                if(mixed == 65536) mixed = 65535;
                MAIN_PCM[i] = mixed - 32768;
            }
        } else
            for(int i = 0; i < samples*2; ++i)
                MAIN_PCM[i] = MAIN_PCM[i] * midivol / 256;
    } else if( digital )
        for( int i = 0; i < samples*2; ++i)
            MAIN_PCM[i] = MAIN_PCM[i] * voicevol / 256;
    samples *= 2; //to stereo

    aui.samplenum = samples;
    aui.pcm_sample = MAIN_PCM;

    AU_writedata(&aui);
}
