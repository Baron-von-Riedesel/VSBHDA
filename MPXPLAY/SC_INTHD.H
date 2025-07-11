/*
 * Universal Interface for Intel High Definition Audio Codec
 *
 * Copyright (c) 2004 Takashi Iwai <tiwai@suse.de>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along with
 *  this program; if not, write to the Free Software Foundation, Inc., 59
 *  Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __SOUND_HDA_CODEC_H
#define __SOUND_HDA_CODEC_H

#if defined(CONFIG_PM) || defined(CONFIG_SND_HDA_POWER_SAVE)
#define SND_HDA_NEEDS_RESUME    /* resume control code is required */
#endif

/*
 * nodes
 */
#define    AC_NODE_ROOT        0x00

/*
 * function group types
 */
enum {
    AC_GRP_AUDIO_FUNCTION = 0x01,
    AC_GRP_MODEM_FUNCTION = 0x02,
};
    
/*
 * widget types
 */
enum {
    AC_WID_AUD_OUT,        /* 0 Audio Out (DAC) */
    AC_WID_AUD_IN,         /* 1 Audio In (ADC) */
    AC_WID_AUD_MIX,        /* 2 Audio Mixer */
    AC_WID_AUD_SEL,        /* 3 Audio Selector */
    AC_WID_PIN,            /* 4 Pin Complex */
    AC_WID_POWER,          /* 5 Power */
    AC_WID_VOL_KNB,        /* 6 Volume Knob */
    AC_WID_BEEP,           /* 7 Beep Generator */
    AC_WID_VENDOR = 0x0f   /* Vendor specific */
};

struct BDL_s {
    uint64_t address;
    uint32_t size;
    uint32_t flags;
};

/*
 * GET verbs (0F=12 bit verb ID)
 */
#define AC_VERB_GET_STREAM_FORMAT        0x0a00
#define AC_VERB_GET_AMP_GAIN_MUTE        0x0b00
#define AC_VERB_GET_PROC_COEF            0x0c00 /* get processing coefficient value */
#define AC_VERB_GET_COEF_INDEX           0x0d00 /* get processing coefficient index */
#define AC_VERB_PARAMETERS               0x0f00
#define AC_VERB_GET_CONNECT_SEL          0x0f01 /* get currently selected connection */
#define AC_VERB_GET_CONNECT_LIST         0x0f02
#define AC_VERB_GET_PROC_STATE           0x0f03 /* processing state */
#define AC_VERB_GET_SDI_SELECT           0x0f04
#define AC_VERB_GET_POWER_STATE          0x0f05
#define AC_VERB_GET_CONV                 0x0f06 /* converter stream, channel */
#define AC_VERB_GET_PIN_WIDGET_CONTROL   0x0f07 /* see AC_PINCTL_ */
#define AC_VERB_GET_UNSOLICITED_RESPONSE 0x0f08
#define AC_VERB_GET_PIN_SENSE            0x0f09
#define AC_VERB_GET_BEEP_CONTROL         0x0f0a
#define AC_VERB_GET_EAPD_BTLENABLE       0x0f0c
#define AC_VERB_GET_DIGI_CONVERT         0x0f0d
#define AC_VERB_GET_VOLUME_KNOB_CONTROL  0x0f0f
/* f10-f1a: GPIO */
#define AC_VERB_GET_GPIO_DATA            0x0f15
#define AC_VERB_GET_GPIO_MASK            0x0f16
#define AC_VERB_GET_GPIO_DIRECTION       0x0f17
#define AC_VERB_GET_CONFIG_DEFAULT       0x0f1c
/* f20: AFG/MFG */
#define AC_VERB_GET_SUBSYSTEM_ID         0x0f20

/*
 * SET verbs (07=12 bit verb ID)
 */
#define AC_VERB_SET_STREAM_FORMAT        0x200 /* */
#define AC_VERB_SET_AMP_GAIN_MUTE        0x300 /* */
#define AC_VERB_SET_PROC_COEF            0x400
#define AC_VERB_SET_COEF_INDEX           0x500
#define AC_VERB_SET_CONNECT_SEL          0x701
#define AC_VERB_SET_PROC_STATE           0x703
#define AC_VERB_SET_SDI_SELECT           0x704
#define AC_VERB_SET_POWER_STATE          0x705
#define AC_VERB_SET_CHANNEL_STREAMID     0x706
#define AC_VERB_SET_PIN_WIDGET_CONTROL   0x707
#define AC_VERB_SET_UNSOLICITED_ENABLE   0x708
#define AC_VERB_SET_PIN_SENSE            0x709
#define AC_VERB_SET_BEEP_CONTROL         0x70a
#define AC_VERB_SET_EAPD_BTLENABLE       0x70c
#define AC_VERB_SET_DIGI_CONVERT_1       0x70d
#define AC_VERB_SET_DIGI_CONVERT_2       0x70e
#define AC_VERB_SET_VOLUME_KNOB_CONTROL  0x70f
#define AC_VERB_SET_GPIO_DATA            0x715
#define AC_VERB_SET_GPIO_MASK            0x716
#define AC_VERB_SET_GPIO_DIRECTION       0x717
#define AC_VERB_SET_CONFIG_DEFAULT_BYTES_0    0x71c
#define AC_VERB_SET_CONFIG_DEFAULT_BYTES_1    0x71d
#define AC_VERB_SET_CONFIG_DEFAULT_BYTES_2    0x71e
#define AC_VERB_SET_CONFIG_DEFAULT_BYTES_3    0x71f
#define AC_VERB_SET_CODEC_RESET          0x7ff

/*
 * Parameter IDs ( for verb ID 0F00 )
 */
#define AC_PAR_VENDOR_ID         0x00
#define AC_PAR_SUBSYSTEM_ID      0x01
#define AC_PAR_REV_ID            0x02
#define AC_PAR_NODE_COUNT        0x04
#define AC_PAR_FUNCTION_TYPE     0x05
#define AC_PAR_AUDIO_FG_CAP      0x08
#define AC_PAR_AUDIO_WIDGET_CAP  0x09
#define AC_PAR_PCM               0x0a
#define AC_PAR_STREAM            0x0b
#define AC_PAR_PIN_CAP           0x0c
#define AC_PAR_AMP_IN_CAP        0x0d
#define AC_PAR_CONNLIST_LEN      0x0e
#define AC_PAR_POWER_STATE       0x0f
#define AC_PAR_PROC_CAP          0x10
#define AC_PAR_GPIO_CAP          0x11
#define AC_PAR_AMP_OUT_CAP       0x12

/*
 * AC_VERB_PARAMETERS results (32bit)
 */

/* Function Group Type */
#define AC_FGT_TYPE            (0xff<<0)
#define AC_FGT_TYPE_SHIFT       0
#define AC_FGT_UNSOL_CAP       (1<<8)

/* Audio Function Group Capabilities */
#define AC_AFG_OUT_DELAY        (0xf<<0)
#define AC_AFG_IN_DELAY         (0xf<<8)
#define AC_AFG_BEEP_GEN         (1<<16)

/* Audio Widget Capabilities
 */
#define AC_WCAP_STEREO            (1<<0)    /* stereo I/O */
#define AC_WCAP_IN_AMP            (1<<1)    /* AMP-in present */
#define AC_WCAP_OUT_AMP           (1<<2)    /* AMP-out present */
#define AC_WCAP_AMP_OVRD          (1<<3)    /* AMP-parameter override */
#define AC_WCAP_FORMAT_OVRD       (1<<4)    /* format override */
#define AC_WCAP_STRIPE            (1<<5)    /* stripe */
#define AC_WCAP_PROC_WID          (1<<6)    /* Proc Widget */
#define AC_WCAP_UNSOL_CAP         (1<<7)    /* Unsol capable */
#define AC_WCAP_CONN_LIST         (1<<8)    /* connection list */
#define AC_WCAP_DIGITAL           (1<<9)    /* digital I/O */
#define AC_WCAP_POWER             (1<<10)   /* power control */
#define AC_WCAP_LR_SWAP           (1<<11)   /* L/R swap */
#define AC_WCAP_DELAY            (0xf<<16)
#define AC_WCAP_DELAY_SHIFT        16
#define AC_WCAP_TYPE             (0xf<<20)
#define AC_WCAP_TYPE_SHIFT         20

/* supported PCM rates and bits */
#define AC_SUPPCM_RATES            (0xfff << 0)
#define AC_SUPPCM_BITS_8        (1<<16)
#define AC_SUPPCM_BITS_16        (1<<17)
#define AC_SUPPCM_BITS_20        (1<<18)
#define AC_SUPPCM_BITS_24        (1<<19)
#define AC_SUPPCM_BITS_32        (1<<20)

/* supported PCM stream format */
#define AC_SUPFMT_PCM            (1<<0)
#define AC_SUPFMT_FLOAT32        (1<<1)
#define AC_SUPFMT_AC3            (1<<2)

/* Pin widget capabilies */
#define AC_PINCAP_IMP_SENSE        (1<<0)    /* impedance sense capable */
#define AC_PINCAP_TRIG_REQ         (1<<1)    /* trigger required */
#define AC_PINCAP_PRES_DETECT      (1<<2)    /* presence detect capable */
#define AC_PINCAP_HP_DRV           (1<<3)    /* headphone drive capable */
#define AC_PINCAP_OUT              (1<<4)    /* output capable */
#define AC_PINCAP_IN               (1<<5)    /* input capable */
#define AC_PINCAP_BALANCE          (1<<6)    /* balanced I/O capable */
#define AC_PINCAP_VREF             (0x37<<8)
#define AC_PINCAP_VREF_SHIFT        8
#define AC_PINCAP_EAPD             (1<<16)   /* EAPD capable */
/* Vref status (used in pin cap) */
#define AC_PINCAP_VREF_HIZ        (1<<0)    /* Hi-Z */
#define AC_PINCAP_VREF_50         (1<<1)    /* 50% */
#define AC_PINCAP_VREF_GRD        (1<<2)    /* ground */
#define AC_PINCAP_VREF_80         (1<<4)    /* 80% */
#define AC_PINCAP_VREF_100        (1<<5)    /* 100% */

/* Amplifier capabilities */
#define AC_AMPCAP_OFFSET          (0x7f<<0)  /* 0dB offset */
#define AC_AMPCAP_OFFSET_SHIFT     0
#define AC_AMPCAP_NUM_STEPS_SHIFT  8
#define AC_AMPCAP_NUM_STEPS_MASK   0x7f      /* number of steps */
#define AC_AMPCAP_STEP_SIZE       (0x7f<<16) /* step size 0-32dB, in 0.25dB  */
#define AC_AMPCAP_STEP_SIZE_SHIFT  16
#define AC_AMPCAP_MUTE            (1<<31)    /* mute capable */
#define AC_AMPCAP_MUTE_SHIFT       31

/* Connection list */
#define AC_CLIST_LENGTH           (0x7f<<0)
#define AC_CLIST_LONG             (1<<7)

/* Supported power status */
#define AC_PWRST_D0SUP            (1<<0)
#define AC_PWRST_D1SUP            (1<<1)
#define AC_PWRST_D2SUP            (1<<2)
#define AC_PWRST_D3SUP            (1<<3)

/* Power state values */
#define AC_PWRST_D0            0x00
#define AC_PWRST_D1            0x01
#define AC_PWRST_D2            0x02
#define AC_PWRST_D3            0x03

/* Processing capabilies
 * correect? docs say: 0=off, 1=on, 2=benign, 3-7f reserved, 80-ff vendor specific
 */
#define AC_PCAP_BENIGN            (1<<0)
#define AC_PCAP_NUM_COEF        (0xff<<8)

/* Volume knobs capabilities */
#define AC_KNBCAP_NUM_STEPS        (0x7f<<0)
#define AC_KNBCAP_DELTA            (1<<8)

/*
 * Control Parameters
 */

/* Amp gain/mute response format + bits 0-7 of set */
#define AC_AMP_MUTE             (1<<7)
#define AC_AMP_GAIN             (0x7f)

/* verb GET_AMP_GAIN_MUTE */
#define AC_AMP_GET_INDEX        (0xf<<0)
#define AC_AMP_GET_LEFT         (1<<13)
#define AC_AMP_GET_RIGHT        (0<<13)
#define AC_AMP_GET_OUTPUT       (1<<15)
#define AC_AMP_GET_INPUT        (0<<15)

/* verb SET_AMP_GAIN_MUTE  */
#define AC_AMP_SET_INDEX        (0xf<<8)
#define AC_AMP_SET_INDEX_SHIFT  8
#define AC_AMP_SET_RIGHT        (1<<12)
#define AC_AMP_SET_LEFT         (1<<13)
#define AC_AMP_SET_INPUT        (1<<14)
#define AC_AMP_SET_OUTPUT       (1<<15)

/* DIGITAL1 bits */
#define AC_DIG1_ENABLE          (1<<0)
#define AC_DIG1_V               (1<<1)
#define AC_DIG1_VCFG            (1<<2)
#define AC_DIG1_EMPHASIS        (1<<3)
#define AC_DIG1_COPYRIGHT       (1<<4)
#define AC_DIG1_NONAUDIO        (1<<5)
#define AC_DIG1_PROFESSIONAL    (1<<6)
#define AC_DIG1_LEVEL           (1<<7)

/* Verb Pin widget control (F07) - 8bit */
#define AC_PINCTL_VREFEN        (0x7<<0)
#define AC_PINCTL_VREF_HIZ       0    /* Hi-Z */
#define AC_PINCTL_VREF_50        1    /* 50% */
#define AC_PINCTL_VREF_GRD       2    /* ground */
#define AC_PINCTL_VREF_80        4    /* 80% */
#define AC_PINCTL_VREF_100       5    /* 100% */
#define AC_PINCTL_IN_ENABLE     (1<<5)
#define AC_PINCTL_OUT_ENABLE    (1<<6)
#define AC_PINCTL_HP_ENABLE     (1<<7) /* low impedance amplifier enable/disable */

#define AC_PINCTL_EAPD_ENABLE   (1<<1) /* for digital widgets */

/* Unsolicited response - 8bit */
#define AC_USRSP_EN            (1<<7)

/* configuration default - 32bit */
#define AC_DEFCFG_SEQUENCE        (0xf<<0)
#define AC_DEFCFG_DEF_ASSOC       (0xf<<4)
#define AC_DEFCFG_ASSOC_SHIFT      4
#define AC_DEFCFG_MISC            (0xf<<8)
#define AC_DEFCFG_MISC_SHIFT       8
#define AC_DEFCFG_COLOR           (0xf<<12)
#define AC_DEFCFG_COLOR_SHIFT      12
#define AC_DEFCFG_CONN_TYPE       (0xf<<16)
#define AC_DEFCFG_CONN_TYPE_SHIFT  16
#define AC_DEFCFG_DEVICE          (0xf<<20)
#define AC_DEFCFG_DEVICE_SHIFT     20
#define AC_DEFCFG_LOCATION        (0x3f<<24)
#define AC_DEFCFG_LOCATION_SHIFT   24
#define AC_DEFCFG_PORT_CONN       (0x3<<30)
#define AC_DEFCFG_PORT_CONN_SHIFT  30

/* device device types (0x0-0xf) */
enum {
    AC_JACK_LINE_OUT,
    AC_JACK_SPEAKER,
    AC_JACK_HP_OUT,
    AC_JACK_CD,
    AC_JACK_SPDIF_OUT,
    AC_JACK_DIG_OTHER_OUT,
    AC_JACK_MODEM_LINE_SIDE,
    AC_JACK_MODEM_HAND_SIDE,
    AC_JACK_LINE_IN,
    AC_JACK_AUX,
    AC_JACK_MIC_IN,
    AC_JACK_TELEPHONY,
    AC_JACK_SPDIF_IN,
    AC_JACK_DIG_OTHER_IN,
    AC_JACK_OTHER = 0xf,
};

/* jack connection types (0x0-0xf) */
enum {
    AC_JACK_CONN_UNKNOWN,
    AC_JACK_CONN_1_8,
    AC_JACK_CONN_1_4,
    AC_JACK_CONN_ATAPI,
    AC_JACK_CONN_RCA,
    AC_JACK_CONN_OPTICAL,
    AC_JACK_CONN_OTHER_DIGITAL,
    AC_JACK_CONN_OTHER_ANALOG,
    AC_JACK_CONN_DIN,
    AC_JACK_CONN_XLR,
    AC_JACK_CONN_RJ11,
    AC_JACK_CONN_COMB,
    AC_JACK_CONN_OTHER = 0xf,
};

/* jack colors (0x0-0xf) */
enum {
    AC_JACK_COLOR_UNKNOWN,
    AC_JACK_COLOR_BLACK,
    AC_JACK_COLOR_GREY,
    AC_JACK_COLOR_BLUE,
    AC_JACK_COLOR_GREEN,
    AC_JACK_COLOR_RED,
    AC_JACK_COLOR_ORANGE,
    AC_JACK_COLOR_YELLOW,
    AC_JACK_COLOR_PURPLE,
    AC_JACK_COLOR_PINK,
    AC_JACK_COLOR_WHITE = 0xe,
    AC_JACK_COLOR_OTHER,
};

/* Jack location (0x0-0x3f) */
/* common case */
enum {
    AC_JACK_LOC_NONE,
    AC_JACK_LOC_REAR,
    AC_JACK_LOC_FRONT,
    AC_JACK_LOC_LEFT,
    AC_JACK_LOC_RIGHT,
    AC_JACK_LOC_TOP,
    AC_JACK_LOC_BOTTOM,
};
/* bits 4-5 */
enum {
    AC_JACK_LOC_EXTERNAL = 0x00,
    AC_JACK_LOC_INTERNAL = 0x10,
    AC_JACK_LOC_SEPARATE = 0x20,
    AC_JACK_LOC_OTHER    = 0x30,
};
enum {
    /* external on primary chasis */
    AC_JACK_LOC_REAR_PANEL = 0x07,
    AC_JACK_LOC_DRIVE_BAY,
    /* internal */
    AC_JACK_LOC_RISER = 0x17,
    AC_JACK_LOC_HDMI,
    AC_JACK_LOC_ATAPI,
    /* others */
    AC_JACK_LOC_MOBILE_IN = 0x37,
    AC_JACK_LOC_MOBILE_OUT,
};

/* Port connectivity (0-3), bits 30-31 */
enum {
    AC_JACK_PORT_COMPLEX,
    AC_JACK_PORT_NONE,
    AC_JACK_PORT_FIXED,
    AC_JACK_PORT_BOTH,
};

enum {
    HDA_INPUT, HDA_OUTPUT
};

/* max. connections to a widget */
#define HDA_MAX_CONNECTIONS    32

/* max. codec address */
#define HDA_MAX_CODEC_ADDRESS    0x0f

//-----------------------------------------------------------------
// from hda_local.h

//enum { HDA_FRONT, HDA_REAR, HDA_CLFE, HDA_SIDE }; /* index for dac_nidx */
//enum { HDA_DIG_NONE, HDA_DIG_EXCLUSIVE, HDA_DIG_ANALOG_DUP }; /* dig_out_used */

//-----------------------------------------------------------------
// from hda_int.c

enum {
 AZX_DRIVER_ICH,
 AZX_DRIVER_PCH,
 AZX_DRIVER_SCH,
 AZX_DRIVER_SKL,
 AZX_DRIVER_HDMI,
 AZX_DRIVER_ATI,
 AZX_DRIVER_ATIHDMI,
 AZX_DRIVER_ATIHDMI_NS,
 AZX_DRIVER_VIA,
 AZX_DRIVER_SIS,
 AZX_DRIVER_ULI,
 AZX_DRIVER_NVIDIA,
 AZX_DRIVER_TERA,
 AZX_DRIVER_CTX,
 AZX_DRIVER_CTHDA,
 AZX_DRIVER_CMEDIA,
 AZX_DRIVER_ZHAOXIN,
 AZX_DRIVER_GENERIC,
 AZX_NUM_DRIVERS
};

struct HDASTREAM_s {
    union {
        uint32_t dwCtl;
        struct {
            uint16_t wCtl;
            uint8_t bCtl2316;
            uint8_t bSts;
        };
    };
    uint32_t dwLinkPos;
    uint32_t dwBufLen;
    uint16_t wLastIdx;
    uint16_t wFIFOmark;
    uint16_t wFIFOsize;
    uint16_t wFormat;
    uint32_t dwRsvd;
    uint32_t dwBDLLow;
    uint32_t dwBDLHigh;
};

struct HDAREGS_s {
    uint16_t gcap;
    uint8_t  vminor;
    uint8_t  vmajor;
    uint16_t opayload;
    uint16_t ipayload;
    uint32_t gctl;
    uint16_t wakeen;
    uint16_t statests;
    uint32_t gsts;
    uint32_t rsvd1[3];
    uint32_t intctl;
    uint32_t intsts;
    uint32_t rsvd2[2];
    uint32_t walclk;
    uint32_t rsvd3[1];
    uint32_t ssync;
    uint32_t rsvd4[1];
    uint32_t corblbase;
    uint32_t corbubase;
    uint16_t corbwp;
    uint16_t corbrp;
    uint8_t  corbctl;
    uint8_t  corbsts;
    uint8_t  corbsize;
    uint8_t  rsvd5[1];
    uint32_t rirblbase;
    uint32_t rirbubase;
    uint16_t rirbwp;
    uint16_t rirbric;
    uint8_t  rirbctl;
    uint8_t  rirbsts;
    uint8_t  rirbsize;
    uint8_t  rsvd6[1];
    uint32_t ic;
    uint32_t ir;
    uint16_t ics;
    uint16_t rsvd7[1];
    uint32_t rsvd8[1];
    uint32_t dplbase;
    uint32_t dpubase;
    uint32_t rsvd9[2];
    struct HDASTREAM_s stream[8];
};

/*CORB & RIRB */
#define HDA_CORB_MAXSIZE 1024
#define HDA_CORB_ALIGN    128
#define HDA_RIRB_MAXSIZE 2048
#define HDA_RIRB_ALIGN    128

#define HDA_DPLBASE_ENABLE     0x1  /* Enable position buffer */

/* PCI space */
#define HDA_PCIREG_TCSEL   0x44

//#define MAX_AZX_DEV        16
//#define BDL_SIZE           8192   /* must conform to HDA_CORB/RIRB_ALIGN -> bits 0-6 cleared! */
#define BDL_SIZE             (sizeof(struct BDL_s) * 512) /* must be 128byte aligned */
//#define AZX_MAX_FRAG       (BDL_SIZE / (MAX_AZX_DEV * 16))

#define STATESTS_INT_MASK    0x07
#define AZX_MAX_CODECS       4

/* GCTL unsolicited response enable bit */
#define HDA_GCTL_UREN        (1<<8)

/* GCTL reset bit */
#define HDA_GCTL_RESET       (1<<0)

/* SD_CTL bits */
#define SD_CTL_STREAM_RESET    0x01    /* stream reset bit; 0=stream ready */
#define SD_CTL_DMA_START       0x02    /* 1=DMA engine runs */
#define SD_CTL_STREAM_TAG_MASK    (0xf << 20) /* bits 20-23 stream# */
#define SD_CTL_STREAM_TAG_SHIFT    20

/* SD_CTL and SD_STS */
#define SD_INT_COMPLETE        0x04    /* int on completion enable/status */
#define SD_INT_FIFO_ERR        0x08    /* FIFO error interrupt enable/status */
#define SD_INT_DESC_ERR        0x10    /* descriptor error interrupt enable/status */
#define SD_INT_MASK            (SD_INT_DESC_ERR | SD_INT_FIFO_ERR | SD_INT_COMPLETE)

#define RIRB_INT_MASK        0x05

/* INTCTL and INTSTS */
/* bits 0-29 are for streams, starting with input, output, bidirect */
//#define HDA_INT_ALL_STREAM   0xff       /* all stream interrupts */
#define HDA_INT_CTRL_EN      0x40000000 /* controller interrupt enable bit */
#define HDA_INT_GLOBAL_EN    0x80000000 /* global interrupt enable bit */

/* Defines for ATI HD Audio support in SB450 south bridge */
#define ATI_SB450_HDAUDIO_MISC_CNTR2_ADDR   0x42
#define ATI_SB450_HDAUDIO_ENABLE_SNOOP      0x02

/* Defines for Nvidia HDA support */
#define NVIDIA_HDA_TRANSREG_ADDR      0x4e
#define NVIDIA_HDA_ENABLE_COHBITS     0x0f
#define NVIDIA_HDA_ISTRM_COH          0x4d
#define NVIDIA_HDA_OSTRM_COH          0x4c
#define NVIDIA_HDA_ENABLE_COHBIT      0x01

/* Defines for Intel SCH HDA snoop control */
#define INTEL_SCH_HDA_DEVC            0x78
#define INTEL_SCH_HDA_DEVC_NOSNOOP    (0x1<<11)

#define INTEL_HDA_HDBARL           0x10
#define INTEL_HDA_HDBARU           0x14
#define INTEL_HDA_HDCTL            0x40


#define MAX_PCM_VOLS    2

/* amp values */
#define AMP_IN_MUTE(idx)    (0x7080 | ((idx)<<8))
#define AMP_IN_UNMUTE(idx)    (0x7000 | ((idx)<<8))
#define AMP_OUT_MUTE    0xb080
#define AMP_OUT_UNMUTE    0xb000
#define AMP_OUT_ZERO    0xb000
/* pinctl values */
#define PIN_IN        0x20
#define PIN_VREF80    0x24
#define PIN_VREF50    0x21
#define PIN_OUT       0x40
#define PIN_HP        0xc0
#define PIN_HP_AMP    0x80

#define defcfg_type(node) (((node)->def_cfg & AC_DEFCFG_DEVICE) >> \
               AC_DEFCFG_DEVICE_SHIFT)
#define defcfg_location(node) (((node)->def_cfg & AC_DEFCFG_LOCATION) >> \
                   AC_DEFCFG_LOCATION_SHIFT)
#define defcfg_port_conn(node) (((node)->def_cfg & AC_DEFCFG_PORT_CONN) >> \
                AC_DEFCFG_PORT_CONN_SHIFT)

#endif /* __SOUND_HDA_CODEC_H */
