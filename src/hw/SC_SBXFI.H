
#define EMU20KX_MAX_CHANNELS     8
#define EMU20KX_MAX_BYTES        4

#define EMU20KX_PAGESIZE     4096 // ???
#define EMU20KX_MAXPAGES     1024 // ???

//#define PCI_SUBDEVICE_ID_CREATIVE_SB0760     0x0024
//#define PCI_SUBDEVICE_ID_CREATIVE_SB08801    0x0041
//#define PCI_SUBDEVICE_ID_CREATIVE_SB08802    0x0042
//#define PCI_SUBDEVICE_ID_CREATIVE_SB08803    0x0043
//#define PCI_SUBDEVICE_ID_CREATIVE_HENDRIX    0x6000

// sample formats
#define SRC_SF_U8     0x0
#define SRC_SF_S16    0x1
#define SRC_SF_S24    0x2
#define SRC_SF_S32    0x3
#define SRC_SF_F32    0x4

// EMU20K1 registers (ct20k1reg.h)
#define    PTPAHX       0x13B000 /* page table high */
#define    PTPALX       0x13B004 /* page table low */
#define    TRNCTL       0x13B404 /* used */
#define    TRNIS        0x13B408 /* used */

#define    AMOPLO       0x140000
#define    AMOPHI       0x140004
#define    PMOPLO       0x148000
#define    PMOPHI       0x148004

#define    PRING_LO_HI  0x198000

#define    SRCCTL       0x1B0000 /* */
#define    SRCCCR       0x1B0004
#define    SRCCA        0x1B0010
#define    SRCCF        0x1B0014
#define    SRCSA        0x1B0018
#define    SRCLA        0x1B001C
#define    SRCMCTL      0x1B012C /* */
#define    SRCIP        0x1B102C /* SRC interrupt pending */

#define    AC97D        0x1C5400
#define    AC97A        0x1C5404
#define    AC97CTL      0x1C5408
#define    I2SCTL       0x1C5420

#define    SPOS         0x1C5440
#define    SPOSA        0x1C5440
#define    SPOSB        0x1C5444
#define    SPOSC        0x1C5448
#define    SPOSD        0x1C544C

#define    SPISA        0x1C5450
#define    SPISB        0x1C5454
#define    SPISC        0x1C5458
#define    SPISD        0x1C545C

#define    SPFSCTL      0x1C5460

#define    SPFS0        0x1C5468
#define    SPFS1        0x1C546C
#define    SPFS2        0x1C5470
#define    SPFS3        0x1C5474
#define    SPFS4        0x1C5478
#define    SPFS5        0x1C547C

#define    SPOCTL       0x1C5480
#define    SPICTL       0x1C5484

#define    WC           0x1C6000
#define    TIMR         0x1C6004
# define   TIMR_IE      (1<<15)
# define   TIMR_IP      (1<<14)
#define    GIP          0x1C6010 /* global interrupt pending */
#define    GIE          0x1C6014 /* global interrupt enable */
#define    DIE          0x1C6018
#define    DIC          0x1C601C
#define    GPIO         0x1C6020
#define    GPIOCTL      0x1C6024 /* used */
#define    PLLCTL       0x1C6060
#define    GCTL         0x1C6070

// GIE bits
#define PLL_INT     (1 << 10) /* PLL input-clock out-of-range */
#define FI_INT      (1 << 9)  /* forced interrupt */
#define IT_INT      (1 << 8)  /* timer interrupt */
#define PCI_INT     (1 << 7)  /* PCI bus error pending */
#define URT_INT     (1 << 6)  /* UART Tx/Rx */
#define GPI_INT     (1 << 5)  /* GPI pin */
#define MIX_INT     (1 << 4)  /* mixer parameter segment FIFO channels */
#define DAI_INT     (1 << 3)  /* DAI (SR-tracker or SPDIF-receiver) */
#define TP_INT      (1 << 2)  /* transport priority queue */
#define DSP_INT     (1 << 1)  /* DSP */
#define SRC_INT     (1 << 0)  /* SRC channels */

// EMU20K1 card initialization bits
#define GCTL_EAC    0x00000001 /* used */
#define GCTL_EAI    0x00000002 /* used; autoinit */
#define GCTL_BEP    0x00000004
#define GCTL_BES    0x00000008
#define GCTL_DSP    0x00000010
#define GCTL_DBP    0x00000020 /* used */
#define GCTL_ABP    0x00000040
#define GCTL_TBP    0x00000080 /* used */
#define GCTL_SBP    0x00000100
#define GCTL_FBP    0x00000200 /* used */
#define GCTL_XA     0x00000400
#define GCTL_ET     0x00000800 /* used */
#define GCTL_PR     0x00001000
#define GCTL_MRL    0x00002000
#define GCTL_SDE    0x00004000
#define GCTL_SDI    0x00008000
#define GCTL_SM     0x00010000
#define GCTL_SR     0x00020000
#define GCTL_SD     0x00040000
#define GCTL_SE     0x00080000
#define GCTL_AID    0x00100000 /* used; autoinit */

// SRC resource control block
#define SRCCTL_STATE 0x00000007
#define SRCCTL_BM    0x00000008
#define SRCCTL_RSR   0x00000030
#define SRCCTL_SF    0x000001C0
#define SRCCTL_WR    0x00000200
#define SRCCTL_PM    0x00000400
#define SRCCTL_ROM   0x00001800
#define SRCCTL_VO    0x00002000
#define SRCCTL_ST    0x00004000
#define SRCCTL_IE    0x00008000
#define SRCCTL_ILSZ  0x000F0000
#define SRCCTL_BP    0x00100000

// SRCCTL_STATE
#define SRC_STATE_OFF     0x0
#define SRC_STATE_INIT    0x4

#define SRCCA_CA    0x03FFFFFF

// mixer defs

#define BLANK_SLOT        4094

#define INIT_VOL    0x1c00

#define AMIXER_Y_IMMEDIATE    1

#define AMOPLO_M    0x00000003 // amoplo mode
#define AMOPLO_X    0x0003FFF0
#define AMOPLO_Y    0xFFFC0000

#define AMOPHI_SADR  0x000000FF
#define AMOPHI_SE    0x80000000

enum CT_AMIXER_CTL{
 // volume control mixers
 AMIXER_MASTER_F,
 AMIXER_MASTER_R,
 AMIXER_MASTER_C,
 AMIXER_MASTER_S,
 AMIXER_PCM_F,
 AMIXER_PCM_R,
 AMIXER_PCM_C,
 AMIXER_PCM_S,
 AMIXER_SPDIFI,
 AMIXER_LINEIN,
 AMIXER_MIC,
 AMIXER_SPDIFO,
 AMIXER_WAVE_F,
 AMIXER_WAVE_R,
 AMIXER_WAVE_C,
 AMIXER_WAVE_S,
 AMIXER_MASTER_F_C,
 AMIXER_PCM_F_C,
 AMIXER_SPDIFI_C,
 AMIXER_LINEIN_C,
 AMIXER_MIC_C,
 // this should always be the last one
 NUM_CT_AMIXERS
};

enum MIXER_PORT_T {
 MIX_WAVE_FRONT,
 MIX_WAVE_REAR,
 MIX_WAVE_CENTLFE,
 MIX_WAVE_SURROUND,
 MIX_SPDIF_OUT,
 MIX_PCMO_FRONT,
 MIX_MIC_IN,
 MIX_LINE_IN,
 MIX_SPDIF_IN,
 MIX_PCMI_FRONT,
 MIX_PCMI_REAR,
 MIX_PCMI_CENTLFE,
 MIX_PCMI_SURROUND,

 NUM_MIX_PORTS
};
