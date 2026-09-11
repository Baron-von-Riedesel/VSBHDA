/*
 *  Copyright (c) 2004 James Courtier-Dutton <James@superbug.demon.co.uk>
 *  Driver CA0106 chips. e.g. Sound Blaster Audigy LS and Live 24bit
 *  Version: 0.0.20
 *
 *  This code was initally based on code from ALSA's emu10k1x.c which is:
 *  Copyright (c) by Francisco Moraes <fmoraes@nc.rr.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

/************************************************************************************************/
/* PCI function 0 registers, address = <val> + PCIBASE0                                         */
/************************************************************************************************/

#define PTR            0x00        /* Indexed register set pointer register */
                                   /* NOTE: The CHANNELNUM and ADDRESS words can */
                                   /* be modified independently of each other. */
                                   /* CNL[1:0], ADDR[27:16] */

#define DATA            0x04        /* Indexed register set data register */
                                    /* DATA[31:0] */

#define IPR                  0x08          /* Global interrupt pending register */
                                           /* Clear pending interrupts by writing a 1 to */
                                           /* the relevant bits and zero to the other bits */
#define IPR_MIDI_RX_B        0x00020000    /* MIDI UART-B Receive buffer non-empty */
#define IPR_MIDI_TX_B        0x00010000    /* MIDI UART-B Transmit buffer empty */
#define IPR_SPDIF_IN_USER    0x00004000    /* SPDIF input user data has 16 more bits */
#define IPR_SPDIF_OUT_USER   0x00002000    /* SPDIF output user data needs 16 more bits */
#define IPR_SPDIF_OUT_FRAME  0x00001000    /* SPDIF frame about to start */
#define IPR_SPI              0x00000800    /* SPI transaction completed */
#define IPR_I2C_EEPROM       0x00000400    /* I2C EEPROM transaction completed */
#define IPR_I2C_DAC          0x00000200    /* I2C DAC transaction completed */
#define IPR_AI               0x00000100    /* Audio pending register changed. See PTR reg 0x76 */
#define IPR_GPI              0x00000080    /* General Purpose input changed */
#define IPR_SRC_LOCKED       0x00000040    /* SRC lock status changed */
#define IPR_SPDIF_STATUS     0x00000020    /* SPDIF status changed */
#define IPR_TIMER2           0x00000010    /* 192000Hz Timer */
#define IPR_TIMER1           0x00000008    /* 44100Hz Timer */
#define IPR_MIDI_RX_A        0x00000004    /* MIDI UART-A Receive buffer non-empty */
#define IPR_MIDI_TX_A        0x00000002    /* MIDI UART-A Transmit buffer empty */
#define IPR_PCI              0x00000001    /* PCI Bus error */

#define INTE                  0x0c          /* Interrupt enable register */
#define INTE_MIDI_RX_B        0x00020000    /* MIDI UART-B Receive buffer non-empty */
#define INTE_MIDI_TX_B        0x00010000    /* MIDI UART-B Transmit buffer empty */
#define INTE_SPDIF_IN_USER    0x00004000    /* SPDIF input user data has 16 more bits */
#define INTE_SPDIF_OUT_USER   0x00002000    /* SPDIF output user data needs 16 more bits */
#define INTE_SPDIF_OUT_FRAME  0x00001000    /* SPDIF frame about to start */
#define INTE_SPI              0x00000800    /* SPI transaction completed */
#define INTE_I2C_EEPROM       0x00000400    /* I2C EEPROM transaction completed */
#define INTE_I2C_DAC          0x00000200    /* I2C DAC transaction completed */
#define INTE_AI               0x00000100    /* Audio pending register changed. See PTR reg 0x75 */
#define INTE_GPI              0x00000080    /* General Purpose input changed */
#define INTE_SRC_LOCKED       0x00000040    /* SRC lock status changed */
#define INTE_SPDIF_STATUS     0x00000020    /* SPDIF status changed */
#define INTE_TIMER2           0x00000010    /* 192000Hz Timer */
#define INTE_TIMER1           0x00000008    /* 44100Hz Timer */
#define INTE_MIDI_RX_A        0x00000004    /* MIDI UART-A Receive buffer non-empty */
#define INTE_MIDI_TX_A        0x00000002    /* MIDI UART-A Transmit buffer empty */
#define INTE_PCI              0x00000001    /* PCI Bus error */

#define UNKNOWN10        0x10        /* Unknown ??. Defaults to 0 */

#define HCFG                       0x14          /* Hardware config register */
                                                 /* 0x1000 causes AC3 to fails. It adds a dither bit. */
#define HCFG_STAC                  0x10000000    /* Special mode for STAC9460 Codec. */
#define HCFG_CAPTURE_I2S_BYPASS    0x08000000    /* 1 = bypass I2S input async SRC. */
#define HCFG_CAPTURE_SPDIF_BYPASS  0x04000000    /* 1 = bypass SPDIF input async SRC. */
#define HCFG_PLAYBACK_I2S_BYPASS   0x02000000    /* 0 = I2S IN mixer output, 1 = I2S IN1. */
#define HCFG_FORCE_LOCK            0x01000000    /* For test only. Force input SRC tracker to lock. */
#define HCFG_PLAYBACK_ATTENUATION  0x00006000    /* Playback attenuation mask. 0 = 0dB, 1 = 6dB, 2 = 12dB, 3 = Mute. */
#define HCFG_PLAYBACK_DITHER       0x00001000    /* 1 = Add dither bit to all playback channels. */
#define HCFG_PLAYBACK_S32_LE       0x00000800    /* 1 = S32_LE, 0 = S16_LE                       */
#define HCFG_CAPTURE_S32_LE        0x00000400    /* 1 = S32_LE, 0 = S16_LE (S32_LE current not working) */
#define HCFG_8_CHANNEL_PLAY        0x00000200    /* 1 = 8 channels, 0 = 2 channels per substream.*/
#define HCFG_8_CHANNEL_CAPTURE     0x00000100    /* 1 = 8 channels, 0 = 2 channels per substream.*/
#define HCFG_MONO                  0x00000080    /* 1 = I2S Input mono                           */
#define HCFG_I2S_OUTPUT            0x00000010    /* 1 = I2S Output disabled                      */
#define HCFG_AC97                  0x00000008    /* 0 = AC97 1.0, 1 = AC97 2.0                   */
#define HCFG_LOCK_PLAYBACK_CACHE   0x00000004    /* 1 = Cancel bustmaster accesses to soundcache */
                                                 /* NOTE: This should generally never be used. */
#define HCFG_LOCK_CAPTURE_CACHE    0x00000002    /* 1 = Cancel bustmaster accesses to soundcache */
                                                 /* NOTE: This should generally never be used. */
#define HCFG_AUDIOENABLE           0x00000001    /* 0 = CODECs transmit zero-valued samples */
                                                 /* Should be set to 1 when the EMU10K1 is */
                                                 /* completely initialized. */

#define GPIO            0x18        /* Defaults: 005f03a3-Analog, 005f02a2-SPDIF.   */
                        /* Here pins 0,1,2,3,4,,6 are output. 5,7 are input */
                        /* For the Audigy LS, pin 0 (or bit 8) controls the SPDIF/Analog jack. */
                        /* SB Live 24bit:
                         * bit 8 0 = SPDIF in and out / 1 = Analog (Mic or Line)-in.
                         * bit 9 0 = Mute / 1 = Analog out.
                         * bit 10 0 = Line-in / 1 = Mic-in.
                         * bit 11 0 = ? / 1 = ?
                         * bit 12 0 = ? / 1 = ?
                         * bit 13 0 = ? / 1 = ?
                         * bit 14 0 = Mute / 1 = Analog out
                         * bit 15 0 = ? / 1 = ?
                         * Both bit 9 and bit 14 have to be set for analog sound to work on the SB Live 24bit.
                         */
                        /* 8 general purpose programmable In/Out pins.
                         * GPI [8:0] Read only. Default 0.
                         * GPO [15:8] Default 0x9. (Default to SPDIF jack enabled for SPDIF)
                         * GPO Enable [23:16] Default 0x0f. Setting a bit to 1, causes the pin to be an output pin.
                         */
#define AC97DATA        0x1c        /* AC97 register set data register (16 bit)    */

#define AC97ADDRESS        0x1e        /* AC97 register set address register (8 bit)    */

