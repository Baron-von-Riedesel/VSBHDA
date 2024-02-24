
/* main: install & uninstall, cmdline scan */

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include <go32.h>
#include <sys/exceptn.h>

#include "CONFIG.H"
#include "PLATFORM.H"
#include "PIC.H"
#include "LINEAR.H"
#include "PTRAP.H"
#include "VDMA.H"
#include "VIRQ.H"
#include "VOPL3.H"
#include "VSB.H"

#include "MPXPLAY.H"

#define BASE_DEFAULT 0x220
#define IRQ_DEFAULT 7
#define DMA_DEFAULT 1
#define TYPE_DEFAULT 5
#if TYPE_DEFAULT < 6
#define HDMA_DEFAULT 0
#else
#define HDMA_DEFAULT 5
#endif
#define VOL_DEFAULT 7

extern int SNDISR_InterruptPM();

struct audioout_info_s aui = {0};

/* for AU_setrate() - use fixed rate */
static struct audio_decoder_info_s adi = {
    NULL, /* private data */
    0, /* infobits */
    WAVEID_PCM_SLE, /* 16-bit samples */
    22050, /* freq: 22050 or 44100 */
    HW_CHANNELS, /* channels in file (not used) */
    HW_CHANNELS, /* decoded channels */
    NULL, /* output channel matrix */
    HW_BITS, /* 16 */
    HW_BITS/8, /* bytes per sample */
    0}; /* bitrate */

uint8_t bDbgInit = 1; /* 1=debug output to DOS, 0=low-level */


#if PREMAPDMA
uint32_t MAIN_MappedBase; /* linear address mapped ISA DMA region (0x000000 - 0xffffff) */
#endif
#if SETABSVOL
uint16_t MAIN_SB_VOL = 0; //initial set volume will cause interrupt missing?
#endif
bool _hdpmi_InstallISR( uint8_t i, int(*ISR)(void) );
bool _hdpmi_UninstallISR( void );
bool _hdpmi_InstallInt31( uint8_t );
bool _hdpmi_UninstallInt31( void );

static bool bISR; /* 1=ISR installed */
static bool bQemm = false;
static bool bHdpmi = false;
static int bHelp = false;

struct globalvars gvars = { BASE_DEFAULT, IRQ_DEFAULT, DMA_DEFAULT,
#if SB16
HDMA_DEFAULT,
#endif
TYPE_DEFAULT, true, true, true, VOL_DEFAULT };

static struct {
    const char *option;
    const char *desc;
    int *pValue;
} MAIN_Options[] = {
    "/?", "Show help", &bHelp,
    "/A", "Set IO base address [220|240, def 220]", &gvars.base,
    "/I", "Set IRQ number [5|7, def 7]", &gvars.irq,
    "/D", "Set DMA channel [0|1|3, def 1]", &gvars.dma,
#if SB16
    "/H", "Set High DMA channel [5|6|7, no def]", &gvars.hdma,
    "/T", "Set SB Type [0-6, def 5]", &gvars.type,
#else
    "/T", "Set SB Type [0-5, def 5]", &gvars.type,
#endif
    "/OPL","Set OPL3 emulation [0|1, def 1]", &gvars.opl3,
    "/PM", "Set protected-mode support [0|1, def 1]", &gvars.pm,
    "/RM", "Set real-mode support [0|1, def 1]", &gvars.rm,
    "/F", "Set frequency [22050|44100, def 22050]", &adi.freq,
    "/VOL", "Set master volume [0-9, def 7]", &gvars.vol,
    "/O", "Set output (HDA only) [0=lineout|1=speaker|2=hp, def 0]", &gvars.pin,
    "/DEV", "Set start index for device scan (HDA only) [def 0]", &gvars.device,
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
	asm( /* check if a (virtual) SB does respond */
		"mov $0x226, %%dx \n\t"
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

#if 1 //def _DEBUG
void fatal_error( int nError )
//////////////////////////////
{
	asm( /* set text mode 3 */
		"mov $3, %ax\n\t"
		"int $0x10"
	   );
	printf("VSBHDA: fatal error %u\n", nError );
	for (;;);

}
#endif

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
	if (bISR) UninstallISR();

	if( gvars.rm )
		PTRAP_Uninstall_RM_PortTraps();

	if( gvars.pm ) {
		PTRAP_Uninstall_PM_PortTraps();
	}
    return;
}

/* uninstall.
 * uninstall a protected-mode TSR is best done from real-mode.
 * for VSBHDA, it's triggered by an OUT 226h, 55h ( see vsb.c ).
 * note: no printf() possible here, since stdio files are closed.
 */
void MAIN_Uninstall( void )
///////////////////////////
{
	__dpmi_regs r = {0};
	ReleaseRes();
	AU_close( &aui );
	/* set TSR's parent PSP to current PSP;
	 * set TSR's int 22h to current PSP:0000;
	 * switch to TSR PSP
	 * run an int 21h, ah=4Ch in pm
	 * todo: adjust value at psp:2Eh
	 */
	r.x.ax = 0x5100; /* get current PSP in BX (segment) */
	__dpmi_simulate_real_mode_interrupt(0x21, &r);
#if 0
	uint32_t dwSSSP = ReadLinearD( (r.x.bx << 4) + 0x2E );
	WriteLinearD( _go32_info_block.linear_address_of_original_psp+0x2E, dwSSSP );
#endif
	WriteLinearW( _go32_info_block.linear_address_of_original_psp+0xA, 0 );
	WriteLinearW( _go32_info_block.linear_address_of_original_psp+0xC, r.x.bx );
	WriteLinearW( _go32_info_block.linear_address_of_original_psp+0x16, r.x.bx );
	r.x.bx = _go32_info_block.linear_address_of_original_psp >> 4;
	r.x.ax = 0x5000;
	__dpmi_simulate_real_mode_interrupt(0x21, &r);
	dbgprintf("MAIN_Uninstall: all cleanup done, terminating\n");
	asm( /* DOS exit, will terminate the installed VSBHDA TSR */
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
#define HELPNOTE "\nOptions /A /I /D /H /T have precedence.\n"
#else
#define MAXTYPE 5
#define HELPNOTE "\nOptions /A /I /D /T have precedence.\n"
#endif

int main(int argc, char* argv[])
////////////////////////////////
{

    //parse BLASTER env first.
    void * p;
    char* blaster = getenv("BLASTER");
    if(blaster != NULL) {
        char c;
        while(( c = toupper(*(blaster++)))) {
            if(c == 'I')
                gvars.irq = *(blaster++) - '0';
            else if(c == 'D')
                gvars.dma = *(blaster++) - '0';
            else if(c == 'A') {
                gvars.base = strtol(blaster, &blaster, 16);
                while(*blaster >= '0') blaster++;
            } else if(c =='T')
                gvars.type = *(blaster++) - '0';
#if SB16
            else if(c =='H')
                gvars.hdma = *(blaster++) - '0';
#endif
        }
    }

    /* check cmdline arguments */
    for(int i = 1; i < argc; ++i ) {
        int j;
        for( j = 0; j < OPT_COUNT; ++j ) {
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
        printf("VSBHDA: Sound Blaster emulation on AC97. Usage:\n");

        for( int i = 0; MAIN_Options[i].option; i++ )
            printf( " %-8s: %s\n", MAIN_Options[i].option, MAIN_Options[i].desc );

        printf("\nNote: the BLASTER environment variable may change the default settings; " HELPNOTE );
        printf("\nSource code used from:\n    MPXPlay (https://mpxplay.sourceforge.net/)\n    DOSBox (https://www.dosbox.com/)\n");
        return 0;
    }

    if( gvars.base != 0x220 && gvars.base != 0x240 ) {
        printf("Error: invalid IO base address: %x.\n", gvars.base );
        return 1;
    }
    if( gvars.irq != 0x5 && gvars.irq != 0x7 ) {
        printf("Error: invalid IRQ: %d.\n", gvars.irq );
        return 1;
    }
    if( gvars.dma != 0x0 && gvars.dma != 1 && gvars.dma != 3 ) {
        printf("Error: invalid DMA channel.\n");
        return 1;
    }
#if SB16
    if( gvars.hdma != 0x0 && ( gvars.hdma <= 4 || gvars.hdma > 7)) {
        printf("Error: invalid HDMA channel: %u\n", gvars.hdma );
        return 1;
    }
#endif
    if( gvars.type <= 0 || gvars.type > MAXTYPE ) {
        printf("Error: invalid SB Type: %d\n", gvars.type );
        return 1;
    }
    if( gvars.pin < 0 || gvars.pin > 2) {
        printf("Error: Invalid output: %d\n", gvars.pin );
        return 1;
    }
     if( gvars.vol < 0 || gvars.vol > 9) {
        printf("Error: Invalid volume.\n");
        return 1;
    }
     if( adi.freq != 22050 && adi.freq != 44100 ) {
        printf("Error: Invalid frequency.\n");
        return 1;
    }

    __dpmi_get_segment_base_address(_my_ds(), &DSBase);

    /* temp alloc a 64 kB buffer so it will belong to THIS client. Any dpmi memory allocation
     * while another client is active will result in problems, since that memory is released when
     * the client exits. Also, if malloc() needs a new block of dpmi memory, it will adjust
     * limit of DS - something that has to be avoided.
     */
    if (p = malloc( 0x10000 ) )
        free( p );

    __dpmi_set_segment_limit(_my_ds(), 0xFFFFFFFF);

    if ( IsInstalled() ) {
        printf("SB found - probably VSBHDA already installed.\n" );
        return(0);
    }
    if( gvars.rm ) {
        int bcd = PTRAP_GetQEMMVersion();
        //dbgprintf("QEMM version: %x.%02x\n", bcd>>8, bcd&0xFF);
        if(bcd < 0x703) {
            printf("Jemm/Qemm not installed, or version below 7.03: %x.%02x - disable real mode support.\n", bcd >> 8, bcd & 0xFF);
            gvars.rm = false;
        }
    }
    if( gvars.pm ) {
        bool hasHDPMI = PTRAP_DetectHDPMI(); //another DPMI host used other than HDPMI
        if(!hasHDPMI)
            printf("HDPMI not installed, disable protected mode support.\n");
        gvars.pm = hasHDPMI;
    }
    if( !gvars.pm && !gvars.rm ) {
        printf("Both real mode & protected mode support are disabled, exiting.\n");
        return 1;
    }
    VIRQ_Init();
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
    AU_setmixer_init(&aui);
    //MAIN_GLB_VOL = gvars.vol;
#if SETABSVOL
    MAIN_SB_VOL = gvars.vol * 256/9; /* translate 0-9 to 0-256 */
#endif
    AU_setmixer_outs(&aui, MIXER_SETMODE_ABSOLUTE, gvars.vol * 100/9 );
    //AU_setmixer_one( &aui, AU_MIXCHAN_MASTER, MIXER_SETMODE_ABSOLUTE, gvars.vol * 100/9 );
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
    printf("Sound Blaster emulation enabled at Address=%x, IRQ=%d, DMA=%d, HDMA=%d, Type=%d\n",
           gvars.base, gvars.irq, gvars.dma, gvars.hdma, gvars.type );
#else
    printf("Sound Blaster emulation enabled at Address=%x, IRQ=%u, DMA=%u, Type=%d\n", gvars.base, gvars.irq, gvars.dma, gvars.type );
#endif
    if ( gvars.vol != VOL_DEFAULT )
        printf("Volume=%u\n", gvars.vol );

    bISR = InstallISR(PIC_IRQ2VEC( aui.card_irq), &SNDISR_InterruptPM );

    PIC_UnmaskIRQ(aui.card_irq);

    bDbgInit = 0;
    AU_prestart(&aui);
    AU_start(&aui);

#if PREMAPDMA
    /* Map the full first 16M to simplify DMA mem access */
    MAIN_MappedBase = MapFirst16M();
    dbgprintf("MappedBase=%x\n", MAIN_MappedBase );
#endif

    if( bISR && ( bQemm || (!gvars.rm) ) && ( bHdpmi || (!gvars.pm) ) ) {
        __dpmi_regs r = {0};
        __dpmi_set_coprocessor_emulation( 0 );
        uint32_t psp = _go32_info_block.linear_address_of_original_psp;
        __dpmi_free_dos_memory( ReadLinearW( psp+0x2C ) );
        WriteLinearW( psp+0x2C, 0 );
        for ( int i = 0; i < 5; i++ )
            _dos_close( i );
        __djgpp_exception_toggle();
        _go32_info_block.size_of_transfer_buffer = 0; /* ensure it's not used anymore */
        asm( /* clear fs/gs before calling DOS "terminate and stay resident" */
            "push $0\n\t"
            "pop %gs\n\t"
            "push $0\n\t"
            "pop %fs"
           );
        r.x.dx= 0x10; /* only psp remains in conv. memory */
        r.x.ax = 0x3100;
        __dpmi_simulate_real_mode_interrupt(0x21, &r); //won't return on success
    }
    ReleaseRes();
    printf("Error: Failed installing TSR.\n");
    return 1;
}
