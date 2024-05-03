
/* main: install & uninstall, cmdline scan */

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#ifdef DJGPP
#include <go32.h>
#include <sys/exceptn.h>
#endif

#include "CONFIG.H"
#include "PLATFORM.H"
#include "PIC.H"
#include "LINEAR.H"
#include "PTRAP.H"
#include "VDMA.H"
#include "VIRQ.H"
#include "VOPL3.H"
#include "VSB.H"
#include "SNDISR.H"

#include "AU.H"

#define BASE_DEFAULT 0x220
#define IRQ_DEFAULT 7
#define DMA_DEFAULT 1
#define TYPE_DEFAULT 4
#if TYPE_DEFAULT < 6
#define HDMA_DEFAULT 0
#else
#define HDMA_DEFAULT 5
#endif
#define VOL_DEFAULT 7

#define VERMAJOR "1"
#define VERMINOR "4"

#ifndef DJGPP
uint32_t    __djgpp_stack_top;
uint32_t _linear_psp;
uint32_t _linear_rmstack;
uint32_t _get_linear_psp( void );
uint32_t _get_linear_rmstack( int * );
uint32_t _get_stack_top( void );

/* don't assume hdpmi is loaded - get PSP selector from OW's _psp global var! */
#pragma aux _get_linear_psp = \
	"mov ebx, [_psp]" \
	"mov ax, 6" \
	"int 31h" \
	"push cx" \
	"push dx" \
	"pop eax" \
	parm [] \
	modify exact [ebx cx dx eax]

#pragma aux _get_linear_rmstack = \
	"mov bx, 40h" \
	"mov ax, 100h" \
	"int 31h" \
	"mov [ecx], dx" \
	"movzx eax, ax" \
	"shl eax, 4" \
	parm [ecx] \
	modify exact [bx eax]

#pragma aux _get_stack_top = \
	"mov eax, esp" \
	parm [] \
	modify exact[eax];

#endif

int hAU;  /* handle audioout_info; we don't want to know mpxplay internals */
static int freq = 22050; /* default value for AU_setrate() */

uint8_t bOMode = 1; /* 1=output DOS, 2=direct, 4=debugger */

#if PREMAPDMA
uint32_t MAIN_MappedBase; /* linear address mapped ISA DMA region (0x000000 - 0xffffff) */
#endif
#if SETABSVOL
extern uint16_t SNDISR_SB_VOL = 0; //initial set volume will cause interrupt missing?
#endif

static bool bISR; /* 1=ISR installed */
static bool bQemm = false;
static bool bHdpmi = false;
static int bHelp = false;

struct globalvars gvars = { BASE_DEFAULT, IRQ_DEFAULT, DMA_DEFAULT,
#if SB16
HDMA_DEFAULT,
#endif
TYPE_DEFAULT,
#if VMPU
0, /* no default for Midi port */
#endif
true, true, true, VOL_DEFAULT, 16 };

static const struct {
    const char *option;
    const char *desc;
    int *pValue;
} GOptions[] = {
    "/?", "Show help", &bHelp,
    "/A", "Set IO base address [220|240, def 220]", &gvars.base,
    "/I", "Set IRQ number [5|7, def 7]", &gvars.irq,
    "/D", "Set DMA channel [0|1|3, def 1]", &gvars.dma,
#if SB16
    "/H", "Set High DMA channel [5|6|7, no def]", &gvars.hdma,
    "/T", "Set SB Type [0-6, def 4]", &gvars.type,
#else
    "/T", "Set SB Type [0-5, def 4]", &gvars.type,
#endif
#if VMPU
    "/P", "Set Midi port [330|300, no def]", &gvars.mpu,
#endif
    "/OPL","Set OPL3 emulation [0|1, def 1]", &gvars.opl3,
    "/PM", "Set protected-mode support [0|1, def 1]", &gvars.pm,
    "/RM", "Set real-mode support [0|1, def 1]", &gvars.rm,
    "/F",  "Set frequency [22050|44100, def 22050]", (int *)&freq,
    "/VOL", "Set master volume [0-9, def 7]", &gvars.vol,
    "/BS",  "Set PCM buffer size [in 4k pages, def 16]", &gvars.buffsize,
#if SLOWDOWN
    "/SD",  "Set slowdown factor [def 0]", &gvars.slowdown,
#endif
    "/O",  "Set output (HDA only) [0=lineout|1=speaker|2=hp, def 0]", &gvars.pin,
    "/DEV", "Set start index for device scan (HDA only) [def 0]", &gvars.device,
    NULL, NULL, 0,
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

int _is_installed( void );

static int IsInstalled( void )
//////////////////////////////
{
    return _is_installed();
}

#if 1 //def _DEBUG
void fatal_error( int nError )
//////////////////////////////
{
#ifdef DJGPP
	asm( /* set text mode 3 */
		"mov $3, %ax\n\t"
		"int $0x10"
	   );
#else
	_asm mov ax,3
    _asm int 10h
#endif
	printf("VSBHDA: fatal error %u\n", nError );
	for (;;);
}
#endif

static void ReleaseRes( void )
//////////////////////////////
{
	if (bISR) SNDISR_UninstallISR();

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
 */
void _uninstall_tsr( uint32_t linpsp );

void MAIN_Uninstall( void )
///////////////////////////
{
	ReleaseRes();
	AU_close( hAU );
	_uninstall_tsr( _my_psp() ); /* should not return */
	return;
}

#if REINITOPL
void MAIN_ReinitOPL( void )
///////////////////////////
{
	if( gvars.opl3 ) {
		uint8_t buffer[108];
		fpu_save(buffer);
		VOPL3_Reinit( AU_getfreq( hAU ) );
		fpu_restore(buffer);
	}
}
#endif

#if SB16

#define MAXTYPE 6
#if VMPU
#define HELPNOTE "\nOptions /A /I /D /H /P /T have precedence.\n"
#else
#define HELPNOTE "\nOptions /A /I /D /H /T have precedence.\n"
#endif

#else

#define MAXTYPE 5
#define HELPNOTE "\nOptions /A /I /D /T have precedence.\n"

#endif

#if VMPU
#define IsHexOption(x) (GOptions[x].pValue == &gvars.base || GOptions[x].pValue == &gvars.mpu )
#else
#define IsHexOption(x) (GOptions[x].pValue == &gvars.base )
#endif

int main(int argc, char* argv[])
////////////////////////////////
{

    //parse BLASTER env first.
    int i;
    void * p;
    int rmstksel;
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
#if VMPU
            } else if(c == 'P') {
                gvars.mpu = strtol(blaster, &blaster, 16);
                while(*blaster >= '0') blaster++;
#endif
            } else if(c == 'T')
                gvars.type = *(blaster++) - '0';
#if SB16
            else if(c == 'H')
                gvars.hdma = *(blaster++) - '0';
#endif
        }
    }
    dbgprintf(("A=%x I=%u D=%u T=%u", gvars.base, gvars.irq, gvars.dma, gvars.type ));
#if SB16
    dbgprintf((" H=%u", gvars.hdma ));
#endif
#if VMPU
    dbgprintf((" P=%x", gvars.mpu ));
#endif
    dbgprintf(("\n" ));

    /* check cmdline arguments */
    for( i = 1; i < argc; ++i ) {
        int j;
        for( j = 1; GOptions[j].option; ++j ) {
            int len = strlen( GOptions[j].option);
            if( memicmp(argv[i], GOptions[j].option, len) == 0 ) {
                if ( argv[i][len] >= '0' && argv[i][len] <= '9' ) {
                    *GOptions[j].pValue = strtol(&argv[i][len], NULL, IsHexOption(j) ? 16 : 10 );
                    break;
                } else if ( argv[i][len] == 0 && *GOptions[j].pValue == false ) {
                    *GOptions[j].pValue = true;
                    break;
                }
            }
        }
        if ( GOptions[j].option == NULL )
            bHelp = true;
    }

    /* if -? or unrecognised option was entered, display help and exit */
    if( bHelp ) {
        bHelp = false;
        printf("VSBHDA v" VERMAJOR "." VERMINOR "; Sound Blaster emulation on HDA/AC97. Usage:\n");

        for( i = 0; GOptions[i].option; i++ )
            printf( " %-8s: %s\n", GOptions[i].option, GOptions[i].desc );

        printf("\nNote: the BLASTER environment variable may change the default settings; " HELPNOTE );
        printf("\nSource code used from:\n    MPXPlay (https://mpxplay.sourceforge.net/)\n    DOSBox (https://www.dosbox.com/)\n");
        return 0;
    }

    if( gvars.base != 0x220 && gvars.base != 0x240 ) {
        printf("Error: invalid IO base address: %x\n", gvars.base );
        return 1;
    }
    if( gvars.irq != 0x5 && gvars.irq != 0x7 ) {
        printf("Error: invalid IRQ %d\n", gvars.irq );
        return 1;
    }
    if( gvars.dma != 0x0 && gvars.dma != 1 && gvars.dma != 3 ) {
        printf("Error: invalid DMA channel %d\n", gvars.dma );
        return 1;
    }
#if SB16
    if( gvars.hdma != 0x0 && ( gvars.hdma <= 4 || gvars.hdma > 7)) {
        printf("Error: invalid HDMA channel: %d\n", gvars.hdma );
        return 1;
    }
#endif
    if( gvars.type <= 0 || gvars.type > MAXTYPE ) {
        printf("Error: invalid SB Type: %d\n", gvars.type );
        return 1;
    }
#if VMPU
    if( gvars.mpu && ( gvars.mpu != 0x330 && gvars.mpu != 0x300 )) {
        printf("Error: invalid Midi port address: %x\n", gvars.mpu );
        return 1;
    }
#endif
    if( gvars.pin < 0 || gvars.pin > 2) {
        printf("Error: Invalid output device %d\n", gvars.pin );
        return 1;
    }
    if( gvars.vol < 0 || gvars.vol > 9) {
        printf("Error: Invalid volume %d\n", gvars.vol );
        return 1;
    }
    if( gvars.buffsize < 1) {
        printf("Error: Invalid PCM buffer size %d\n", gvars.buffsize );
        return 1;
    }
    if( freq != 22050 && freq != 44100 ) {
        printf("Error: Invalid frequency %d\n", freq );
        return 1;
    }
#if defined(DJGPP)
    __dpmi_get_segment_base_address(_my_ds(), (unsigned long *)&DSBase);
#endif
#ifndef DJGPP
    __djgpp_stack_top = _get_stack_top();
    _linear_psp = _get_linear_psp();
    _linear_rmstack = _get_linear_rmstack(&rmstksel);
#endif

    if ( IsInstalled() ) {
        printf("SB found - probably VSBHDA already installed.\n" );
        return(0);
    }
    if( gvars.rm ) {
        int bcd = PTRAP_GetQEMMVersion();
        //dbgprintf(("QEMM version: %x.%02x\n", bcd>>8, bcd&0xFF));
        if(bcd < 0x703) {
            printf("Jemm+QPIEmu/Qemm not found [or version (%x.%02x) below 7.03]; no real mode support.\n", bcd >> 8, bcd & 0xFF);
            gvars.rm = false;
        }
    }
    if( gvars.pm ) {
        bool hasHDPMI = PTRAP_DetectHDPMI(); //another DPMI host used than HDPMI?
        if(!hasHDPMI)
            printf("HDPMI not installed, disable protected mode support.\n");
        gvars.pm = hasHDPMI;
    }
    if( !gvars.pm && !gvars.rm ) {
        printf("Both real mode & protected mode support are disabled, exiting.\n");
        return 1;
    }
    //aui.card_select_config = gvars.pin;
    //aui.card_select_devicenum = gvars.device;
    if ( (hAU = AU_init( gvars.device, gvars.pin ) ) == 0 ) {
        printf("No soundcard found!\n");
        return 1;
    }
    printf("Found sound card: %s\n", AU_getshortname( hAU ) );
    if( AU_getirq( hAU ) == gvars.irq ) {
        printf("Sound card IRQ conflict, abort.\n");
        return 1;
    }
    VIRQ_Init( AU_getirq( hAU ) );

    AU_setmixer_init( hAU );

#if SETABSVOL
    SNDISR_SB_VOL = gvars.vol * 256/9; /* translate 0-9 to 0-256 */
#endif
    AU_setmixer_outs( hAU, MIXER_SETMODE_ABSOLUTE, gvars.vol * 100/9 );
    //AU_setmixer_one( hAU, AU_MIXCHAN_MASTER, MIXER_SETMODE_ABSOLUTE, gvars.vol * 100/9 );

    AU_setrate( hAU, freq, HW_CHANNELS, HW_BITS );

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

#ifdef NOFM
    gvars.opl3 = 0;
#endif
#if SB16
	PTRAP_Prepare( gvars.opl3, gvars.base, gvars.dma, gvars.hdma );
#else
	PTRAP_Prepare( gvars.opl3, gvars.base, gvars.dma, 0 );
#endif
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
#ifndef NOFM
    if( gvars.opl3 ) {
        VOPL3_Init( AU_getfreq( hAU ) );
        printf("OPL3 emulation enabled at port 388h (%u Hz).\n", AU_getfreq( hAU ) );
    }
#endif
    printf("Sound Blaster emulation enabled at Address=%x, IRQ=%d, DMA=%d, ", gvars.base, gvars.irq, gvars.dma );
#if SB16
    if (gvars.hdma) printf("HDMA=%d, ", gvars.hdma );
#endif
#if VMPU
    if (gvars.mpu)  printf("P=%X, ", gvars.mpu );
#endif
    printf("Type=%d\n", gvars.type );
    if ( gvars.vol != VOL_DEFAULT )
        printf("Volume=%u\n", gvars.vol );

    /* temp alloc a 64 kB chunk of memory. This will ensure that mallocs done while sound is playing won't
     * need another DPMI memory allocation. A dpmi memory allocation while another client is active will
     * result in problems, since that memory is released when that client exits.
     * This temp alloc is best done before SNDISR_InstallISR() is called.
     */
    /* open cube player may crash on exit with 64 kb (page fault); that's quite strange, since
     * any amount that exceeeds the size of the PCM buffer (64kB) is virtually wasted. Must be
     * a strange side effect...
     */
    if (p = malloc( 0x10000 ) )
        free( p );

    bISR = SNDISR_InstallISR(PIC_IRQ2VEC( AU_getirq( hAU ) ), &SNDISR_Interrupt );

    PIC_UnmaskIRQ( AU_getirq( hAU ) );

    AU_prestart( hAU );
    AU_start( hAU );
    if (bOMode & 1 ) bOMode = 2; /* switch to low-level i/o */

#if PREMAPDMA
    /* Map the full first 16M to simplify DMA mem access */
    MAIN_MappedBase = MapFirst16M();
    dbgprintf(("MappedBase=%x\n", MAIN_MappedBase ));
#endif

    if( bISR && ( bQemm || (!gvars.rm) ) && ( bHdpmi || (!gvars.pm) ) ) {
        uint32_t psp;
        __dpmi_regs r = {0};
        __dpmi_set_coprocessor_emulation( 0 );
        psp = _my_psp();
        __dpmi_free_dos_memory( ReadLinearW( psp+0x2C ) );
        WriteLinearW( psp+0x2C, 0 );
        for ( i = 0; i < 5; i++ )
            _dos_close( i );
#ifdef DJGPP
        __djgpp_exception_toggle();
        _go32_info_block.size_of_transfer_buffer = 0; /* ensure it's not used anymore */
        asm( /* clear fs/gs before calling DOS "terminate and stay resident" */
            "push $0\n\t"
            "pop %gs\n\t"
            "push $0\n\t"
            "pop %fs"
           );
#else
        __dpmi_free_dos_memory( rmstksel ); /* free _linear_rmstack */
#endif
        r.x.dx= 0x10; /* only psp remains in conv. memory */
        r.x.ax = 0x3100;
        __dpmi_simulate_real_mode_interrupt(0x21, &r); //won't return on success
    }
    dbgprintf(("main: bISR=%u, bQemm=%u, bHdpmi=%u\n", bISR, bQemm, bHdpmi ));
    ReleaseRes();
    printf("Error: Failed installing TSR.\n");
    return 1;
}
