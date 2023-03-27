#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include <go32.h>
#include <sys/exceptn.h>

#include "SBEMUCFG.H"
#include "PIC.H"
#include "OPL3EMU.H"
#include "VDMA.H"
#include "VIRQ.H"
#include "SBEMU.H"
#include "UNTRAPIO.H"
#include "DPMI_.H"
#include "QEMM.H"
#include "HDPMIPT.H"

#include <MPXPLAY.H>
#include <AU_MIXER/MIX_FUNC.H>

#define PREMAPDMA 0
#define MAIN_PCM_SAMPLESIZE 16384

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

static BOOL enableRM;
static BOOL enablePM;
static BOOL PM_ISR;


#if PREMAPDMA
static uint32_t MAIN_MappedBase; /* linear address mapped ISA DMA region (0x000000 - 0xffffff) */
#else
static uint32_t MAIN_DMA_Addr = 0;
static uint32_t MAIN_DMA_Size = 0;
static uint32_t MAIN_DMA_MappedAddr = 0;
#endif
static uint16_t MAIN_SB_VOL = 0; //initial set volume will cause interrupt missing?
static uint16_t MAIN_GLB_VOL = 0; //TODO: add hotkey

void MAIN_Interrupt();
static int MAIN_InterruptPM();

static const uint8_t MAIN_ChannelPageMap[] = { 0x87, 0x83, 0x81, 0x82, -1, 0x8b, 0x89, 0x8a };

#define tport( port, proc ) TPORT_ ## port,
#define tportx( port, proc, table ) TPORT_ ## port,
enum TrappedPorts {
#include "PORTS.H"
#undef tport
#undef tportx
    NUM_TPORTS
};

#define tport( port, proc ) { port, 0, proc },
#define tportx( port, proc, table ) { port, 0, proc },
static QEMM_IODT MAIN_IODT[] = {
#include "PORTS.H"
#undef tport
#undef tportx
};

/* order of port ranges */
#define OPL3_IODT  0
#define IRQ_IODT   1
#define DMA_IODT   2
#define DMAPG_IODT 3
#if SB16
#define HDMA_IODT  4
#define SB_IODT    5
#define END_IODT   6
#else
#define SB_IODT    4
#define END_IODT   5
#endif

#define tport( port, proc )
#define tportx( port, proc, table ) TPORT_ ## port,
static int portranges[] = {
#include "PORTS.H"
    NUM_TPORTS
};
#undef tport
#undef tportx

static BOOL bQemm = FALSE;
static BOOL bHdpmi = FALSE;

struct {
    const char* option;
    const char* desc;
    int value;
} MAIN_Options[] =
{
    "/?", "Show help", FALSE,
    "/A", "Specify IO address, valid value: 220,240", 0x220,
    "/I", "Specify IRQ number, valid value: 5,7", 7,
    "/D", "Specify DMA channel, valid value: 0,1,3", 1,
#if SB16
    "/H", "Specify High DMA channel, valid value: 5,6,7", 0,
    "/T", "Specify SB Type, valid value: 0-6", 5,
#else
    "/T", "Specify SB Type, valid value: 0-5", 5,
#endif
    "/OPL", "Enable OPL3 emulation", TRUE,
    "/PM", "Support protected mode games", TRUE,
    "/RM", "Support real mode games", TRUE,
    "/VOL", "Set master volume (0-9)", 7,
    "/O", "Select output (HDA only); 0=lineout, 1=speaker, 2=headphone", 0,
    "/DEV", "Set device index (HDA only); in case there exist multiple devices", 0,
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

static void ReleaseRes( void )
//////////////////////////////
{
	if (PM_ISR) HDPMIPT_UninstallISR();

	if( enableRM )
		QEMM_Uninstall_PortTraps( MAIN_IODT, portranges[END_IODT] );

	if( enablePM ) {
		HDPMIPT_Uninstall_PortTraps( MAIN_IODT, portranges[END_IODT] );
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
	 */
	r.w.ax = 0x5100;
	DPMI_CallRealModeINT(0x21, &r);
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

static void IODT_DelEntries( int start, int end, int entries )
//////////////////////////////////////////////////////////////
{
    int i;
	for ( i = start; i < end - entries; i++ ) {
		MAIN_IODT[i].port = MAIN_IODT[i+entries].port;
		MAIN_IODT[i].handler = MAIN_IODT[i+entries].handler;
	}
	for ( i = 0; i < countof(portranges); i++ ) {
		if ( portranges[i] > start )
			portranges[i] -= entries;
	}
}

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
    //dbgprintf("main argc=%u\n argv[1]=%s\n", argc, argv[1] ? argv[1] : "NULL" );
	if( argc >= 2 && (*argv[1] == '/' || *argv[1] == '-') && ( *(argv[1]+1) == '?' || *(argv[1]+1) == 'h' ) ) {
		printf("SBEMU: Sound Blaster emulation on AC97. Usage:\n");
		int i = 0;
		while(MAIN_Options[i].option) {
			printf(" %-8s: %s. Default: %x\n", MAIN_Options[i].option, MAIN_Options[i].desc, MAIN_Options[i].value);
			++i;
		}
		printf("\nNote: SBEMU will read BLASTER environment variable and use it, " HELPNOTE );
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
                if ( argv[i][len] >= '0' && argv[i][len] <= '9' ) {
                    MAIN_Options[j].value = strtol(&argv[i][len], NULL, (j == OPT_ADDR) ? 16 : 10 );
                    break;
                }
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
    if(MAIN_Options[OPT_DMA].value != 0x0 && MAIN_Options[OPT_DMA].value != 1 && MAIN_Options[OPT_DMA].value != 3)
    {
        printf("Error: invalid DMA channel.\n");
        return 1;
    }
#if SB16
    if(MAIN_Options[OPT_HDMA].value != 0x0 && ( MAIN_Options[OPT_HDMA].value <= 4 || MAIN_Options[OPT_HDMA].value > 7))
    {
        printf("Error: invalid HDMA channel: %u\n", MAIN_Options[OPT_HDMA].value );
        return 1;
    }
#endif
    if(MAIN_Options[OPT_TYPE].value <= 0 || MAIN_Options[OPT_TYPE].value > MAXTYPE )
    {
        printf("Error: invalid SB Type: %d\n", MAIN_Options[OPT_TYPE].value );
        return 1;
    }
    if(MAIN_Options[OPT_OUTPUT].value < 0 || MAIN_Options[OPT_OUTPUT].value > 2)
    {
		printf("Error: Invalid Output: %d\n", MAIN_Options[OPT_OUTPUT].value );
        return 1;
    }
     if(MAIN_Options[OPT_VOL].value < 0 || MAIN_Options[OPT_VOL].value > 9)
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

    if(MAIN_Options[OPT_RM].value)
    {
        int bcd = QEMM_GetVersion();
        //dbgprintf("QEMM version: %x.%02x\n", bcd>>8, bcd&0xFF);
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
    enablePM = MAIN_Options[OPT_PM].value;
    enableRM = MAIN_Options[OPT_RM].value;
    if(!enablePM && !enableRM)
    {
        printf("Both real mode & protected mode support are disabled, exiting.\n");
        return 1;
    }
    
    aui.card_select_config = MAIN_Options[OPT_OUTPUT].value;
    aui.card_select_devicenum = MAIN_Options[OPT_DEVIDX].value;
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
    AU_setrate(&aui, &adi);

    if( enableRM ) {
        enableRM = QEMM_Prepare_IOPortTrap();
        if ( !enableRM ) {
            printf("Error: Failed setting IO port handler for QEMM.\n");
            return 1;
        }
    }

    printf("Real mode support: %s.\n", enableRM ? "enabled" : "disabled");
    printf("Protected mode support: %s.\n", enablePM ? "enabled" : "disabled");

    if(enablePM) {
        UntrappedIO_OUT_Handler = &HDPMIPT_UntrappedIO_OUT;
        UntrappedIO_IN_Handler = &HDPMIPT_UntrappedIO_IN;
    }
    /* adjust IODT port table */
	MAIN_IODT[portranges[DMA_IODT]].port   = MAIN_Options[OPT_DMA].value * 2;
	MAIN_IODT[portranges[DMA_IODT]+1].port = MAIN_Options[OPT_DMA].value * 2 + 1;
	MAIN_IODT[portranges[DMAPG_IODT]].port = MAIN_ChannelPageMap[ MAIN_Options[OPT_DMA].value];
#if SB16
	if ( MAIN_Options[OPT_HDMA].value ) {
		MAIN_IODT[portranges[HDMA_IODT]].port    = MAIN_Options[OPT_HDMA].value * 4 + (0xC0-0x10);
		MAIN_IODT[portranges[HDMA_IODT]+1].port  = MAIN_Options[OPT_HDMA].value * 4 + 2 + (0xC0-0x10);
		MAIN_IODT[portranges[DMAPG_IODT]+1].port = MAIN_ChannelPageMap[ MAIN_Options[OPT_HDMA].value];
	}
#endif
	if ( MAIN_Options[OPT_ADDR].value != 0x220 )
		for( int i = portranges[SB_IODT]; i < portranges[SB_IODT+1]; i++ )
			MAIN_IODT[i].port += MAIN_Options[OPT_ADDR].value - 0x220;

	/* if no OPL3 emulation, skip ports 0x388-0x38b and 0x220-0x223 */
	if ( !MAIN_Options[OPT_OPL].value ) {
		//asm("int3");
		IODT_DelEntries( portranges[OPL3_IODT], portranges[END_IODT], 4 );
		IODT_DelEntries( portranges[SB_IODT], portranges[END_IODT], 4 );
	}

#if SB16
	/* if no SB16 emulation, skip all HDMA ports */
	if ( MAIN_Options[OPT_TYPE].value < 6 || MAIN_Options[OPT_HDMA].value == 0 ) {
		IODT_DelEntries( portranges[DMAPG_IODT]+1, portranges[END_IODT], 1 );
		IODT_DelEntries( portranges[HDMA_IODT], portranges[END_IODT], portranges[HDMA_IODT+1] - portranges[HDMA_IODT] );
	}
	SBEMU_Init( MAIN_Options[OPT_IRQ].value, MAIN_Options[OPT_DMA].value, MAIN_Options[OPT_HDMA].value,
			   MAIN_SB_DSPVersion[ MAIN_Options[OPT_TYPE].value ] );
#else
	SBEMU_Init( MAIN_Options[OPT_IRQ].value, MAIN_Options[OPT_DMA].value, 0,
			   MAIN_SB_DSPVersion[ MAIN_Options[OPT_TYPE].value ] );
#endif
    VDMA_Virtualize( MAIN_Options[OPT_DMA].value, TRUE );
#if SB16
	if( MAIN_Options[OPT_HDMA].value > 0 )
		VDMA_Virtualize( MAIN_Options[OPT_HDMA].value, TRUE );
#endif

	if ( enableRM ) {
		if ((bQemm = QEMM_Install_PortTraps( MAIN_IODT, portranges, countof(portranges)-1 )) == 0 )
			printf("Error: Failed installing IO port trap for Qemm/JemmEx.\n");
	}
    if ( enablePM ) {
		if(( bHdpmi = HDPMIPT_Install_PortTraps( MAIN_IODT, portranges, countof(portranges)-1 )) == 0 )
			printf("Error: Failed installing IO port trap for HDPMI.\n");
#ifdef _DEBUG
        HDPMIPT_PrintPorts( MAIN_IODT, portranges[END_IODT] ); /* for debugging */
#endif
    }

	if( MAIN_Options[OPT_OPL].value ) {
		OPL3EMU_Init(aui.freq_card);
		printf("OPL3 emulation enabled at port 388h.\n");
	}

#if SB16
    printf("Sound Blaster emulation enabled at Address=%x, IRQ=%d, DMA=%d, HDMA=%d, TYPE=%d\n",
           MAIN_Options[OPT_ADDR].value, MAIN_Options[OPT_IRQ].value, MAIN_Options[OPT_DMA].value, MAIN_Options[OPT_HDMA].value, MAIN_Options[OPT_TYPE].value );
#else
    printf("Sound Blaster emulation enabled at Address=%x, IRQ=%u, DMA=%u\n", MAIN_Options[OPT_ADDR].value, MAIN_Options[OPT_IRQ].value, MAIN_Options[OPT_DMA].value);
#endif


    PM_ISR = HDPMIPT_InstallISR(PIC_IRQ2VEC( aui.card_irq), &MAIN_InterruptPM );

    PIC_UnmaskIRQ(aui.card_irq);

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

	if(PM_ISR && ( bQemm || (!enableRM) ) && ( bHdpmi || (!enablePM) ) ) {
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
        asm("sti");
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

#if 0
    if ( SBEMU_TriggerIRQ ) {
        SBEMU_TriggerIRQ = 0;
        VIRQ_Invoke( SBEMU_GetIRQ() );
    }
#endif
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
        //dbgprintf("vol: %d, voicevol: %d, midivol: %d\n", vol, voicevol, midivol);
    }
    else //SBPro
    {
        vol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_MASTERSTEREO)>>5)*256/7; //3:1:3:1 stereo usually the same for both channel for games?;
        voicevol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_VOICESTEREO)>>5)*256/7; //3:1:3:1
        midivol = (SBEMU_GetMixerReg(SBEMU_MIXERREG_MIDISTEREO)>>5)*256/7;
    }
    if(MAIN_SB_VOL != vol*MAIN_GLB_VOL/9)
    {
        //dbgprintf("set sb volume:%d %d\n", MAIN_SB_VOL, vol*MAIN_GLB_VOL/9);
        MAIN_SB_VOL = vol*MAIN_GLB_VOL/9;
        //asm("sub $200, %esp \n\tfsave (%esp)"); /* needed if AU_setmixer_one() uses floats */
        AU_setmixer_one(&aui, AU_MIXCHAN_MASTER, MIXER_SETMODE_ABSOLUTE, vol*100/256);
        //asm("frstor (%esp) \n\tadd $200, %esp \n\t");
    }

    aui.card_outbytes = aui.card_dmasize;
    int samples = AU_cardbuf_space(&aui) / sizeof(int16_t) / 2; //16 bit, 2 channels
    //dbgprintf("samples:%u ",samples);

    if(samples == 0)
        return;

    BOOL digital = SBEMU_HasStarted();
    int dma = SBEMU_GetDMA();
    int32_t DMA_Count = VDMA_GetCounter(dma);

    if(digital)//&& DMA_Count != 0x10000) //-1(0xFFFF)+1=0
    {
        uint32_t DMA_Addr = VDMA_GetAddress(dma);
        int32_t DMA_Index = VDMA_GetIndex(dma);
        uint32_t SB_Bytes = SBEMU_GetSampleBytes();
        uint32_t SB_Pos = SBEMU_GetPos();
        uint32_t SB_Rate = SBEMU_GetSampleRate();
        int samplesize = max(1,SBEMU_GetBits()/8);
        int channels = SBEMU_GetChannels();
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
            if(SBEMU_GetBits()<8) //ADPCM 8bit
                count = max(1, count / (9 / SBEMU_GetBits()));
            int bytes = count * samplesize * channels;

            /* copy samples to our PCM buffer
             */
#if PREMAPDMA
            DPMI_CopyLinear(DPMI_PTR2L(MAIN_PCM+pos*2), MAIN_MappedBase + DMA_Addr + DMA_Index, bytes);
#else
            if( MAIN_DMA_MappedAddr == 0) {//map failed?
                memset(MAIN_PCM+pos*2, 0, bytes);
            } else
                DPMI_CopyLinear(DPMI_PTR2L(MAIN_PCM+pos*2), MAIN_DMA_MappedAddr+(DMA_Addr-MAIN_DMA_Addr)+DMA_Index, bytes);
#endif

            /* format conversion needed? */
#if ADPCM
            if(SBEMU_GetBits()<8) //ADPCM  8bit
                count = SBEMU_DecodeADPCM((uint8_t*)(MAIN_PCM+pos*2), bytes);
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
            SB_Pos = SBEMU_SetPos( SB_Pos + bytes );
            if(SB_Pos >= SB_Bytes)
            {
                if(!SBEMU_GetAuto())
                    SBEMU_Stop();
                SB_Pos = SBEMU_SetPos(0);
                VIRQ_Invoke( SBEMU_GetIRQ() );
                SB_Bytes = SBEMU_GetSampleBytes();
                SB_Pos = SBEMU_GetPos();
                SB_Rate = SBEMU_GetSampleRate();
                //incase IRQ handler re-programs DMA
                DMA_Index = VDMA_GetIndex(dma);
                DMA_Count = VDMA_GetCounter(dma);
                DMA_Addr = VDMA_GetAddress(dma);
            }
        } while(VDMA_GetAuto(dma) && (pos < samples) && SBEMU_HasStarted());

        //dbgprintf("digital end %d %d\n", samples, pos);
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

    //dbgprintf("MAIN INT END\n");
}
