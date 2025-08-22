#ifndef _SB_H_
#define _SB_H_
//Sound blaster definitions
//Sound Blaster Series Hardware Programming Guide: https://www.phatcode.net/articles.php?id=243
//https://github.com/joncampbell123/dosbox-x/wiki/Hardware:Sound-Blaster:DSP-commands

//address: 2x?. those are only offsets
#define SB_PORT_FM_LADDR     0x00    //left addr(W), left status(R)
#define SB_PORT_FM_LDATA     0x01
#define SB_PORT_FM_RADDR     0x02    //right addr(W), right status(R)
#define SB_PORT_FM_RDATA     0x03
#define SB_PORT_MIXER        0x04
#define SB_PORT_MIXER_DATA   0x05
#define SB_PORT_DSP_RESET    0x06
#define SB_PORT_DSP_READ     0x0A
#define SB_PORT_DSP_WRITE_WS 0x0C //cmd/data (W), write buffer status(R)
#define SB_PORT_DSP_RS       0x0E //read buffer status (R). reading this port also acknowledge 8bit interrupt
#define SB_PORT_DSP_16ACK    0x0F //acknowledge 16bit interrupt

/* mixer chip 1335 ( SB 2.0 with CD )
 * 02: master volume ( 3 bits - D1-D3 )
 * 06: midi volume ( 3 bits - D1-D3 )
 * 08: CD volume ( 3 bits - D1-D3 )
 * 0A: voice volume ( 2 bits - D1-D2 )
 *
 * mixer chip 1345 (SB Pro )
 * 04: voice volume ( 2*3 bits - D1-D3 & D5-D7, 2*4 bits for SB16 )
 * 0A: mic level ( 2 bits - D1-D2, 3 bits for SB16 - D0-D2 )
 * 0C: input source: ( bits 1-2 )
 *  low-pass filter ( bit 3 )
 *  input filer ( bit 5 )
 * 0E: stereo switch ( bit 1 )
 *  output filter ( bit 5 ) 
 * 22: master volume ( 2*3 bits, 2*4 bits for SB16 )
 * 26: midi volume ( 2*3 bits, 2*4 bits for SB16 )
 * 28: CD volume ( 2*3 bits, 2*4 bits for SB16 )
 * 2E: line volume ( 2*3 bits, 2*4 bits for SB16 )
 *
 * mixer chip 1745 ( SB16 )
 * compatible with 1345 ( regs 04, 0a, 22, 26, 28, 2E )
 * 3x/4X: 5 bits ( D3-D7) , default 24
 * 3D: input L switches ( mic, cd.r, cr.l, line.r, line.l, midi.r, midi.l )
 * 3E: R switches
 */

//register index through mixer port
#define SB_MIXERREG_RESET        0x00 /* write only */
#define SB_MIXERREG_STATUS       0x01 /* read only: previously selected index */
#define SB_MIXERREG_MASTERVOL    0x02 /* SB, bits[1-3]; SBPro: bits[1-3/5-7], R/L */
#define SB_MIXERREG_MIDIVOL      0x06 /* SB, FM Level, 3 bits [1-3]; SBPro: bits 5+6: L/R on/off */
#define SB_MIXERREG_VOICEVOL     0x0A /* SB, Voice Level, 2 bits [1-2]; SBPro+ - Microphone level, 2/3 bits */

#define SB_MIXERREG_INPUTFILTER  0x0C /* SBPro only */
#define SB_MIXERREG_MODEFILTER   0x0E /* SBPro only - bit 1:Stereo select, bit 5: Filter output */
#define SB_MIXERREG_MODEFILTER_STEREO 2

/* the following regs are 3 bits for SBPro [1-3, 5-7], 4 bit [0-3, 4-7] for SB16 */
#define SB_MIXERREG_VOICESTEREO  0x04 /* SBPro+, DAC level, 3/4 bit L & R */
#define SB_MIXERREG_MASTERSTEREO 0x22 /* SBPro+, Master level, 3/4 bit, L & R */
#define SB_MIXERREG_MIDISTEREO   0x26 /* SBPro+, FM Level, 3/4 bit, L & R */
// 0x28 CD Level ( 3/4 bit, L & R )
// 0x2E line-in Level ( 3/4 bit, L & R )
// 0x24 and 0x2A don't exist ( are 0x04/0x0A )

#if SB16
#define SB16_MIXERREG_MASTERL      0x30 /* bits 3-7; connected with 22h */
#define SB16_MIXERREG_MASTERR      0x31
#define SB16_MIXERREG_VOICEL       0x32 /* bits 3-7; connected with 24h (rather 04?) */
#define SB16_MIXERREG_VOICER       0x33
#define SB16_MIXERREG_MIDIL        0x34 /* bits 3-7; connected with 26h */
#define SB16_MIXERREG_MIDIR        0x35
#define SB16_MIXERREG_CDL          0x36 /* bits 3-7; CD level; connected with 28h */
#define SB16_MIXERREG_CDR          0x37
#define SB16_MIXERREG_LINEINL      0x38 /* bits 3-7; linein level; connected with 2Eh */
#define SB16_MIXERREG_LINEINR      0x39
#define SB16_MIXERREG_MIC          0x3A /* bits 3-7; mic level */
#define SB16_MIXERREG_SPEAKER      0x3B /* bits 6-7; speaker level */
#define SB16_MIXERREG_OUTCTRL      0x3C /* 0=Mic; 1,2=CD, 3,4=LineIn */
#define SB16_MIXERREG_INPCTRLL     0x3D /* 0=Mic; 1,2=CD; 3,4=LineIn; 5,6=FM */
#define SB16_MIXERREG_INPCTRLR     0x3E
#define SB16_MIXERREG_INPGAINCTRLL 0x3F /* bits 6-7, def 0x00 */
#define SB16_MIXERREG_INPGAINCTRLR 0x40 /* bits 6-7, def 0x00 */
#define SB16_MIXERREG_OUTGAINCTRLL 0x41 /* bits 6-7, def 0x00 */
#define SB16_MIXERREG_OUTGAINCTRLR 0x42 /* bits 6-7, def 0x00 */
#define SB16_MIXERREG_AUTOGAINCTRL 0x43
#define SB16_MIXERREG_TREBLEL      0x44 /* bits 4-7, def 0x80 */
#define SB16_MIXERREG_TREBLER      0x45 /* bits 4-7, def 0x80 */
#define SB16_MIXERREG_BASSL        0x46 /* bits 4-7, def 0x80 */
#define SB16_MIXERREG_BASSR        0x47 /* bits 4-7, def 0x80 */
#endif
#define SB_MIXERREG_INT_SETUP    0x80 /* IRQ select, bits 0-3; SB16 only, but used internally */
#define SB_MIXERREG_DMA_SETUP    0x81 /* DMA select, bits 0-7; SB16 only, but used internally */
#define SB_MIXERREG_IRQ_STATUS   0x82 /* IRQ status, bits 0-2; SB16 only, but used internally */
#define SB_MIXERREG_IRQ_STAT8BIT  1
#define SB_MIXERREG_IRQ_STAT16BIT 2
#define SB_MIXERREG_IRQ_STATMPU   4
/* bits 4-7 of IRQ_STATUS: 01=4.04, 02=4.05, 08=4.12 */

//DSP commands
//time constant used
#define SB_DSP_NOCMD                0
#define SB_DSP_8BIT_DIRECT       0x10
#define SB_DSP_8BIT_OUT_SNGL     0x14 //single cycle
#define SB_DSP_2BIT_OUT_SNGL_NREF 0x16 //2bit ADPCM
#define SB_DSP_2BIT_OUT_SNGL     0x17 //2bit ADPCM
#define SB_DSP_8BIT_OUT_AUTO     0x1C
#define SB_DSP_2BIT_OUT_AUTO     0x1F

#define SB_DSP_SET_TIMECONST     0x40
#define SB_DSP_SET_SAMPLERATE    0x41 /* SB16 set sample rate */
#define SB_DSP_SET_SAMPLERATE_I  0x42 //input sample rate
#if 1 //SB16
#define SB_DSP_CONT_8BIT_AUTO    0x45 /* SB16 */
#define SB_DSP_CONT_16BIT_AUTO   0x47 /* SB16 */
#endif
#define SB_DSP_SET_SIZE          0x48 //size-1

#define SB_DSP_4BIT_OUT_SNGL_NREF 0x74 //4bit ADPCM
#define SB_DSP_4BIT_OUT_SNGL     0x75 //4bit ADPCM
#define SB_DSP_4BIT_OUT_AUTO     0x7D //4bit ADPCM
#define SB_DSP_3BIT_OUT_SNGL_NREF 0x76 //3bit ADPCM
#define SB_DSP_3BIT_OUT_SNGL     0x77 //3bit ADPCM
#define SB_DSP_3BIT_OUT_AUTO     0x7F //3bit ADPCM

#define SB_DSP_SILENCE_DAC       0x80 /* SB; output silent samples */
#define SB_DSP_8BIT_OUT_AUTO_HS  0x90
#define SB_DSP_8BIT_OUT_SNGL_HS  0x91 /* high speed mode, need a reset(actually restore to previous) to exit */
#define SB_DSP_HALT_DMA          0xD0
#define SB_DSP_SPEAKER_ON        0xD1
#define SB_DSP_SPEAKER_OFF       0xD3
#define SB_DSP_SPEAKER_STATUS    0xD8
#define SB_DSP_CONTINUE_DMA      0xD4
#if 1 //SB16
#define SB_DSP_HALT_DMA16        0xD5 /* SB16 */
#define SB_DSP_CONTINUE_DMA16    0xD6 /* SB16 */
#define SB_DSP_EXIT_16BIT_AUTO   0xD9 /* SB16 */
#endif
#define SB_DSP_EXIT_8BIT_AUTO    0xDA /* stop DMA auto init mode */
#define SB_DSP_ID                0xE0 /* SB: DSP identification */
#define SB_DSP_GETVER            0xE1 /* 1st byte major, 2nd byte minor */
#define SB_DSP_DMA_ID            0xE2 /* undocumented */
#define SB_DSP_COPYRIGHT         0xE3
#define SB_DSP_WRITE_TESTREG     0xE4 /* SB 2.0 */
#define SB_DSP_READ_TESTREG      0xE8 /* SB 2.0 */
#define SB_DSP_TRIGGER_IRQ       0xF2
#if 1 //SB16
#define SB_DSP_TRIGGER_IRQ16     0xF3 /* SB16 trigger interrupt */
#define SB_DSP_STATUS            0xFB /* SB16 DSP status */
#define SB_DSP_DSP_AUX_STATUS    0xFC /* SB16 DSP aux status */
#define SB_DSP_DSP_CMD_STATUS    0xFD /* SB16 DSP cmd status */
#endif

// SB16: 0Bxh=16-bit, 0Cxh=8-bit
// bits 0-3: bit0=0, bit1=FIFO mode, bit2=DMA mode (0=single, 1=auto), bit3=DAC/ADC (0/1)
//
#define SB16_DSP_16_OUT_SNGL 0xB0
#define SB16_DSP_16_OUT_AUTO 0xB6
#define SB16_DSP_8_OUT_SNGL  0xC0
#define SB16_DSP_8_OUT_AUTO  0xC6
#define SB16_DSP_8_OUT_AUTO_NOFIFO 0xC4 //undocumented, FIFO bit(bit 1) off. from DOSBox source comments
//mode bits:
// bit 4: 1=signed
// bit 5: 1=stereo

#endif//_SB_H_
