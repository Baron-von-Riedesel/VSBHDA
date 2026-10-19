
/* CA20K1 registers;
 * source: ALSA, ctxfi/ct20k1reg.h
 */
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
#define    SRCIMAP      0x1B0008
#define    SRCODDC      0x1B000C
#define    SRCCA        0x1B0010
#define    SRCCF        0x1B0014
#define    SRCSA        0x1B0018
#define    SRCLA        0x1B001C
#define    SRCDN0Z      0x1B0030
#define    SRCDN0Z0     0x1B0030
#define    SRCDN0Z1     0x1B0034
#define    SRCDN0Z2     0x1B0038
#define    SRCDN0Z3     0x1B003C
#define    SRCDN1Z      0x1B0040
#define    SRCDN1Z0     0x1B0040
#define    SRCDN1Z1     0x1B0044
#define    SRCDN1Z2     0x1B0048
#define    SRCDN1Z3     0x1B004C
#define    SRCDN1Z4     0x1B0050
#define    SRCDN1Z5     0x1B0054
#define    SRCDN1Z6     0x1B0058
#define    SRCDN1Z7     0x1B005C
#define    SRCUPZ       0x1B0060
#define    SRCUPZ0      0x1B0060
#define    SRCUPZ1      0x1B0064
#define    SRCUPZ2      0x1B0068
#define    SRCUPZ3      0x1B006C
#define    SRCUPZ4      0x1B0070
#define    SRCUPZ5      0x1B0074
#define    SRCUPZ6      0x1B0078
#define    SRCUPZ7      0x1B007C
#define    SRCMCTL      0x1B012C /* */
#define    SRCIP        0x1B102C /* SRC interrupt pending */
#define    SRCENBSTAT   0x1B202C
#define    SRCENBLO     0x1B212C
#define    SRCENBHI     0x1B222C
#define    SRCENBS      0x1B232C
#define    SRCENB       0x1B282C
#define    SRCENB07     0x1B282C
#define    SRCENBS07    0x1B302C

#define    DAOIMAP_START 0x1C5000
#define    DAOIMAP      0x1C5000
#define    DAOIMAP_END  0x1C5124
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

#define    SRTSCTL      0x1C54D0 /* used */
#define    SRTICTL      0x1C54F0 /* used */

#define    WC           0x1C6000
#define    TIMR         0x1C6004 /* timer ticks ( bits 0-13 ) */
# define   TIMR_IE      (1<<15)
# define   TIMR_IP      (1<<14)
#define    GIP          0x1C6010 /* global interrupt pending */
#define    GIE          0x1C6014 /* global interrupt enable */
#define    DIE          0x1C6018
#define    DIC          0x1C601C
#define    GPIO         0x1C6020
#define    GPIOCTL      0x1C6024 /* used */
#define    PLLCTL       0x1C6060 /* written in hw_pll_init() */
#define    GCTL         0x1C6070
#define    ID0          0x1C6080 /* used */
#define    ID1          0x1C6084
#define    ID2          0x1C6088
#define    ID3          0x1C608C

/* EMU20K1 card initialization bits */
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

