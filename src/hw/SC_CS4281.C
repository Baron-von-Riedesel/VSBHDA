//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2009 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: Cirrus Logic CS4281 low level routines (e.g. Genius Sound Maker
//          Value 5.1, various "CS4281-CM" OEM cards). Has an on-board
//          standard AC'97 codec, so the existing AC97MIX.C mixer table is
//          reused unchanged - no new mixer file needed for this card.
//
//Written as an ADD-ON patch for VSBHDA, following the same structure as
//SC_E1370.C/SC_E1371.C, based on register information from the ALSA driver
//sound/pci/cs4281.c (Jaroslav Kysela) and cross-checked against the
//FreeBSD/OpenBSD sound/pci/cs4281.c drivers.
//
//Unlike every other card in this project, the CS4281 is accessed through
//MEMORY-MAPPED I/O (PCI BAR0, "BA0", a 4KB register window) instead of
//port I/O - see SC_INTHD.C (Intel HDA) for the only other MMIO-based
//driver already in this codebase, whose DPMI mapping approach is reused
//here (__dpmi_physical_address_mapping + NearPtr/LinearAddr, see LINEAR.H).
//
//This file has NOT been tested against real hardware yet - please verify
//against a real CS4281 card and report back any needed fixes (see the
//debug log walkthrough done for SC_E1370.C for the kind of issues to
//expect from a blind, spec-only first implementation).

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "CONFIG.H"
#include "DPMI.H"
#include "LINEAR.H"
#include "AU_CARDS.H"
#include "DMABUFF.H"
#include "PCIBIOS.H"
#include "AC97MIX.H"

#define CS4281_BA0_SIZE 0x1000  /* size of the BA0 MMIO register window */
#define CS4281_FIFO_SIZE 32     /* per-channel FIFO size in samples, full-duplex 2ch */
#define CS4281_DMABUF_ALIGN 512

//-------------------------------------------------------------------------
// BA0 register offsets/bits actually used by this (playback-only) driver.
// Full register set is much larger - see the ALSA driver for anything not
// listed here (MIDI, secondary/dual codec, joystick low-level access...).

#define BA0_HISR        0x0000  /* Host Interrupt Status (R/O, ack via HDSR/EOI) */
#define  BA0_HISR_DMA(c) (1UL<<(8+(c)))
/* Global/summary "a DMA event (half or end) happened" bit - separate from
 * the per-engine DMA(c) bits above. Confirmed (by testing) to be a
 * hierarchical gate: with only DMA(0) unmasked in HIMR and this bit left
 * masked, the per-engine interrupt never propagates to the PCI INTx# pin
 * at all - HISR_DMA(0) can be pending "beneath" it, but the top-level
 * interrupt is simply never asserted, so CS4281_IRQRoutine() is never
 * called (confirmed: zero calls logged across a full play session).
 * ALSA's snd_cs4281_chip_init() unmasks this alongside every per-engine
 * DMA(c) bit; see cs4281.c, BA0_HISR_DMAI. */
#define  BA0_HISR_DMAI   (1UL<<18)

#define BA0_HICR        0x0008  /* Host Interrupt Control */
#define  BA0_HICR_EOI    0x03   /* End Of Interrupt command */

#define BA0_HIMR        0x000c  /* Host Interrupt Mask (1=masked/disabled) */

#define BA0_HDSR0       0x00f0  /* Host DMA Engine 0 Status (playback) */
#define  BA0_HDSR_DHTC   (1<<17) /* DMA Half Terminal Count */
#define  BA0_HDSR_DTC    (1<<16) /* DMA Terminal Count */

#define BA0_DCA0        0x0110  /* Host DMA Engine 0 Current Address */
#define BA0_DCC0        0x0114  /* Host DMA Engine 0 Current Count (frames, counts down) */
#define BA0_DBA0        0x0118  /* Host DMA Engine 0 Base Address */
#define BA0_DBC0        0x011c  /* Host DMA Engine 0 Base Count (frames-1) */

/* per-engine register spacing, engines 1-3 (confirmed against ALSA's
 * snd_cs4281_create(): regDMR = BA0_DMR0 + engine*8, regDCR = BA0_DCR0 +
 * engine*8) - used only to defensively mask engines we don't otherwise
 * touch, see cs4281_chip_init(). */
#define BA0_DMR_N(n)    (0x0150 + (n)*8)
#define BA0_DCR_N(n)    (0x0154 + (n)*8)

#define BA0_DMR0        0x0150  /* Host DMA Engine 0 Mode */
#define  BA0_DMR_DMA     (1<<29) /* enable DMA mode */
#define  BA0_DMR_AUTO    (1<<4)  /* auto-initialize (loop) */
#define  BA0_DMR_TYPE_SINGLE (1<<6)
#define  BA0_DMR_TR_READ (2<<2)  /* "read transfer" = fetch FROM system memory (playback) */

#define BA0_DCR0        0x0154  /* Host DMA Engine 0 Command */
#define  BA0_DCR_TCIE    (1<<16) /* terminal count interrupt enable */
#define  BA0_DCR_HTCIE   (1<<17) /* half terminal count interrupt enable */
#define  BA0_DCR_MSK     (1<<0)  /* DMA mask (1 = channel masked/stopped) */

#define BA0_FCR0        0x0180  /* FIFO Control 0 (playback FIFO) */
#define  BA0_FCR_FEN     (1UL<<31) /* FIFO enable */
#define  BA0_FCR_RS(x)   (((x)&0x1f)<<24) /* right slot mapping */
#define  BA0_FCR_LS(x)   (((x)&0x1f)<<16) /* left slot mapping */
#define  BA0_FCR_SZ(x)   (((x)&0x7f)<<8)  /* FIFO size in samples */
#define  BA0_FCR_OF(x)   (((x)&0x7f)<<0)  /* FIFO starting offset in samples */

#define BA0_FSIC0       0x0210  /* FIFO Status/Interrupt Control 0 */

#define BA0_EPPMC       0x03e4  /* Extended PCI Power Management Control */
#define  BA0_EPPMC_FPDN  (1<<14) /* Full Power DowN - must be 0 or init fails */

#define BA0_CWPR        0x03e0  /* Configuration Write Protect */
#define BA0_SPMC        0x03ec  /* Serial Port Power Management Control */
#define  BA0_SPMC_RSTN   (1<<0)  /* Reset-Not: 0=AC97 ARST# asserted, 1=released */

#define BA0_CFLR        0x03f0  /* Configuration Load Register */
#define  BA0_CFLR_DEFAULT 0x00000001 /* must read back this value = AC97 link mode */

#define BA0_SERMC       0x0420  /* Serial Port Master Control */
#define  BA0_SERMC_PTC_AC97 (1<<1) /* port timing configuration = AC97 */
#define  BA0_SERMC_MSPE  (1<<0)  /* master serial port enable */

#define BA0_SERC1       0x0428  /* Serial Port Configuration 1 (R/O sanity check) */
#define  BA0_SERC1_AC97  (1<<1)
#define  BA0_SERC1_SO1EN (1<<0)

#define BA0_SERC2       0x042c  /* Serial Port Configuration 2 (R/O sanity check) */
#define  BA0_SERC2_AC97  (1<<1)
#define  BA0_SERC2_SI1EN (1<<0)

#define BA0_ACCTL       0x0460  /* AC'97 Control */
#define  BA0_ACCTL_CRW   (1<<4)  /* 0=write, 1=read command */
#define  BA0_ACCTL_DCV   (1<<3)  /* dynamic command valid (self-clears when done) */
#define  BA0_ACCTL_VFRM  (1<<2)  /* valid frame */
#define  BA0_ACCTL_ESYN  (1<<1)  /* enable sync generation */

#define BA0_ACSTS       0x0464  /* AC'97 Status */
#define  BA0_ACSTS_VSTS  (1<<1)  /* valid status (read data ready) */
#define  BA0_ACSTS_CRDY  (1<<0)  /* codec ready */

#define BA0_ACOSV       0x0468  /* AC'97 Output Slot Valid */
#define BA0_ACCAD       0x046c  /* AC'97 Command Address */
#define BA0_ACCDA       0x0470  /* AC'97 Command Data */
#define BA0_ACISV       0x0474  /* AC'97 Input Slot Valid */
#define  BA0_ACISV_SLV(x) (1UL<<((x)-3))
#define  BA0_ACOSV_SLV(x) (1UL<<((x)-3))
#define BA0_ACSDA       0x047c  /* AC'97 Status Data */

#define BA0_CLKCR1      0x0400  /* Clock Control Register 1 */
#define  BA0_CLKCR1_DLLRDY (1<<24) /* DLL ready (R/O) */
#define  BA0_CLKCR1_SWCE (1<<5)  /* software clock enable */
#define  BA0_CLKCR1_DLLP (1<<4)  /* DLL power up */

#define BA0_SSPM        0x0740  /* Sound System Power Management */
#define  BA0_SSPM_MIXEN  (1<<6)
#define  BA0_SSPM_CSRCEN (1<<5)
#define  BA0_SSPM_PSRCEN (1<<4)
#define  BA0_SSPM_JSEN   (1<<3)  /* joystick/gameport enable */
#define  BA0_SSPM_ACLEN  (1<<2)
#define  BA0_SSPM_FMEN   (1<<1)

#define BA0_DACSR       0x0744  /* DAC Sample Rate (playback SRC divisor) */

#define BA0_SRCSA       0x075c  /* SRC Slot Assignments */
#define BA0_PPLVC       0x0760  /* PCM Playback Left digital Volume Control */
#define BA0_PPRVC       0x0764  /* PCM Playback Right digital Volume Control */

//-------------------------------------------------------------------------
// generic PCI power management: force the device into D0. Required on the
// CS4281 - if left in a non-D0 state by the BIOS/a previous OS, every BA0
// register reads back as 0xFFFFFFFF and nothing else in this driver will
// work (see chip_init()'s sanity checks below, which catch this case).

#define PCIR_CAPPTR     0x34
#define PCI_CAP_ID_PM   0x01

static void cs4281_set_power_d0(struct pci_config_s *dev)
//////////////////////////////////////////////////////////
{
	uint16_t status = pcibios_ReadConfig_Word(dev, PCIR_STATUS);
	uint8_t ptr;
	int guard;

	if (!(status & (1<<4))) /* no capabilities list present */
		return;
	ptr = pcibios_ReadConfig_Byte(dev, PCIR_CAPPTR) & 0xfc;
	for (guard = 0; ptr && guard < 16; guard++) {
		uint8_t capid = pcibios_ReadConfig_Byte(dev, ptr);
		if (capid == PCI_CAP_ID_PM) {
			uint16_t pmcsr = pcibios_ReadConfig_Word(dev, ptr+4);
			dbgprintf(("cs4281_set_power_d0: PM cap at %X, pmcsr=%X\n", ptr, pmcsr));
			pcibios_WriteConfig_Word(dev, ptr+4, pmcsr & 0xfffc); /* power state bits = 00 = D0 */
			pds_delay_10us(1000); /* PCI PM spec: allow ~10ms for D3->D0 transition */
			return;
		}
		ptr = pcibios_ReadConfig_Byte(dev, ptr+1) & 0xfc;
	}
}

//-------------------------------------------------------------------------

struct cs4281_card_s
{
	struct pci_config_s pci_dev;
	uint32_t      ba0;    /* linear address of the mapped BA0 MMIO window */
	__dpmi_meminfo ba0_mapinfo;
	struct cardmem_s dm;
	char *pcmout_buffer;
	long  pcmout_bufsize;
	unsigned int frag; /* half/full transfer toggle - see CS4281_IRQRoutine() */
};

static uint32_t cs4281_peek(struct cs4281_card_s *card, unsigned long offs)
///////////////////////////////////////////////////////////////////////////
{
	return ReadLinearD(card->ba0 + offs);
}

static void cs4281_poke(struct cs4281_card_s *card, unsigned long offs, uint32_t val)
/////////////////////////////////////////////////////////////////////////////////////
{
	WriteLinearD(card->ba0 + offs, val);
}

//-------------------------------------------------------------------------
// AC'97 codec access - standard ACCAD/ACCDA/ACCTL/ACSTS/ACSDA protocol,
// same shape as SC_E1371.C's AC97 access, just through MMIO instead of I/O
// ports and with CS4281-specific register offsets/bit positions.

static void cs4281_ac97_write(struct cs4281_card_s *card, unsigned long reg, unsigned long val)
///////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int t;

	cs4281_poke(card, BA0_ACCAD, reg);
	cs4281_poke(card, BA0_ACCDA, val);
	cs4281_poke(card, BA0_ACCTL, BA0_ACCTL_DCV | BA0_ACCTL_VFRM | BA0_ACCTL_ESYN);
	for (t = 0; t < 2000; t++) {
		if (!(cs4281_peek(card, BA0_ACCTL) & BA0_ACCTL_DCV))
			return;
		pds_delay_10us(1);
	}
	dbgprintf(("cs4281_ac97_write: timeout, reg=%X val=%X\n", reg, val));
}

static unsigned long cs4281_ac97_read(struct cs4281_card_s *card, unsigned long reg)
////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int t;

	cs4281_peek(card, BA0_ACSDA); /* discard stale state, as ALSA does */

	cs4281_poke(card, BA0_ACCAD, reg);
	cs4281_poke(card, BA0_ACCDA, 0);
	cs4281_poke(card, BA0_ACCTL, BA0_ACCTL_DCV | BA0_ACCTL_CRW | BA0_ACCTL_VFRM | BA0_ACCTL_ESYN);

	for (t = 0; t < 500; t++) {
		if (!(cs4281_peek(card, BA0_ACCTL) & BA0_ACCTL_DCV))
			goto dcv_ok;
		pds_delay_10us(1);
	}
	dbgprintf(("cs4281_ac97_read: DCV timeout, reg=%X\n", reg));
	return 0xffff;

dcv_ok:
	for (t = 0; t < 100; t++) {
		if (cs4281_peek(card, BA0_ACSTS) & BA0_ACSTS_VSTS)
			return cs4281_peek(card, BA0_ACSDA);
		pds_delay_10us(1);
	}
	dbgprintf(("cs4281_ac97_read: VSTS timeout, reg=%X\n", reg));
	return 0xffff;
}

//-------------------------------------------------------------------------

static unsigned int cs4281_rate_to_rv(unsigned int rate)
////////////////////////////////////////////////////////
{
	/* a handful of rates map to fixed hardware indices; anything else
	 * uses the direct divisor formula. Both forms are accepted by
	 * BA0_DACSR - see ALSA's snd_cs4281_rate(). */
	switch (rate) {
		case 8000:  return 5;
		case 11025: return 4;
		case 16000: return 3;
		case 22050: return 2;
		case 44100: return 1;
		case 48000: return 0;
	}
	return 1536000UL / rate;
}

//-------------------------------------------------------------------------

static unsigned int cs4281_buffer_init( struct cs4281_card_s *card, struct audioout_info_s *aui )
//////////////////////////////////////////////////////////////////////////////////////////////////
{
	card->pcmout_bufsize = MDma_get_bufsize( aui, 0, aui->gvars->period_size ? aui->gvars->period_size : CS4281_DMABUF_ALIGN );
	if (!MDma_alloc_cardmem( &card->dm, card->pcmout_bufsize ) ) return 0;
	card->pcmout_buffer = card->dm.pMem;
	aui->card_pDmaBuffer = card->pcmout_buffer;
	dbgprintf(("cs4281_buffer_init: pcmout_buffer:%X size:%d\n",(unsigned long)card->pcmout_buffer,card->pcmout_bufsize));
	return 1;
}

/* full hardware bring-up sequence - see ALSA's snd_cs4281_chip_init() for
 * the reasoning behind each step; this is NOT a "just enable it" chip,
 * every one of these stages has been observed to be required in practice.
 */
static int cs4281_chip_init(struct cs4281_card_s *card)
///////////////////////////////////////////////////////
{
	uint32_t tmp;
	unsigned int t;

	dbgprintf(("cs4281_chip_init: enter\n"));

	/* sanity check: if the chip wasn't successfully forced to D0, every
	 * BA0 register reads back as all-ones. Bail out cleanly instead of
	 * spinning through every wait-loop below for nothing. */
	if (cs4281_peek(card, BA0_HISR) == 0xffffffffUL) {
		dbgprintf(("cs4281_chip_init: BA0 unreadable (0xFFFFFFFF) - PCI power state not D0?\n"));
		return 0;
	}

	/* mask ALL FOUR DMA engines first thing, not just the one we use -
	 * matches the vendor DOS driver's own very first init action
	 * ("Writing DCRn with 0x1, Mask DMA engine", for n=0..3). This
	 * matters because BA0_HISR_DMAI (unmasked further below, needed for
	 * engine 0's interrupt to ever reach the PCI pin at all) is a
	 * SUMMARY bit covering all four engines - if engines 1-3 were left
	 * in a dirty/enabled state by a previous OS or driver, unmasking
	 * DMAI without also masking them first can let spurious interrupts
	 * from those unused engines through, which is exactly what a crash
	 * that appeared only after unmasking DMAI would look like. */
	{
		unsigned int n;
		for (n = 0; n < 4; n++)
			cs4281_poke(card, BA0_DCR_N(n), BA0_DCR_MSK);
	}

	tmp = cs4281_peek(card, BA0_EPPMC);
	if (tmp & BA0_EPPMC_FPDN)
		cs4281_poke(card, BA0_EPPMC, tmp & ~BA0_EPPMC_FPDN);

	tmp = cs4281_peek(card, BA0_CFLR);
	if (tmp != BA0_CFLR_DEFAULT) {
		cs4281_poke(card, BA0_CFLR, BA0_CFLR_DEFAULT);
		tmp = cs4281_peek(card, BA0_CFLR);
		if (tmp != BA0_CFLR_DEFAULT) {
			dbgprintf(("cs4281_chip_init: CFLR setup failed (%X)\n", tmp));
			return 0;
		}
	}

	/* allow the vendor-defined configuration space (E4h-FFh) to be written */
	cs4281_poke(card, BA0_CWPR, 0x4281);

	tmp = cs4281_peek(card, BA0_SERC1);
	if (tmp != (BA0_SERC1_SO1EN | BA0_SERC1_AC97)) {
		dbgprintf(("cs4281_chip_init: SERC1 AC97 check failed (%X)\n", tmp));
		return 0;
	}
	tmp = cs4281_peek(card, BA0_SERC2);
	if (tmp != (BA0_SERC2_SI1EN | BA0_SERC2_AC97)) {
		dbgprintf(("cs4281_chip_init: SERC2 AC97 check failed (%X)\n", tmp));
		return 0;
	}

	cs4281_poke(card, BA0_SSPM, BA0_SSPM_MIXEN | BA0_SSPM_CSRCEN | BA0_SSPM_PSRCEN |
	                             BA0_SSPM_JSEN | BA0_SSPM_ACLEN | BA0_SSPM_FMEN);

	/* known state for the clock/serial-port logic before bringing it up */
	cs4281_poke(card, BA0_CLKCR1, 0);
	cs4281_poke(card, BA0_SERMC, 0);

	/* ESYN=0 turns off the AC97 sync pulse */
	cs4281_poke(card, BA0_ACCTL, 0);
	pds_delay_10us(5);

	/* pulse ARST# (AC97 reset) low then high, per the AC97 spec */
	cs4281_poke(card, BA0_SPMC, 0);
	pds_delay_10us(5);
	cs4281_poke(card, BA0_SPMC, BA0_SPMC_RSTN);
	pds_delay_10us(5000); /* 50ms */

	cs4281_poke(card, BA0_SERMC, (1<<16)/*TCID(1)*/ | BA0_SERMC_PTC_AC97 | BA0_SERMC_MSPE);

	/* start the DLL clock logic and wait for lock */
	cs4281_poke(card, BA0_CLKCR1, BA0_CLKCR1_DLLP);
	pds_delay_10us(5000); /* 50ms */
	cs4281_poke(card, BA0_CLKCR1, BA0_CLKCR1_SWCE | BA0_CLKCR1_DLLP);

	for (t = 0; t < 10000; t++) { /* up to ~1s */
		if (cs4281_peek(card, BA0_CLKCR1) & BA0_CLKCR1_DLLRDY)
			goto dllrdy_ok;
		pds_delay_10us(10);
	}
	dbgprintf(("cs4281_chip_init: DLLRDY not seen\n"));
	return 0;

dllrdy_ok:
	/* enable sync generation - once bit clock is seen, SYNC starts too */
	cs4281_poke(card, BA0_ACCTL, BA0_ACCTL_ESYN);

	for (t = 0; t < 10000; t++) {
		if (cs4281_peek(card, BA0_ACSTS) & BA0_ACSTS_CRDY)
			goto crdy_ok;
		pds_delay_10us(10);
	}
	dbgprintf(("cs4281_chip_init: codec ready (CRDY) not seen, status=%X\n", cs4281_peek(card, BA0_ACSTS)));
	return 0;

crdy_ok:
	/* start sending commands to the AC97 codec */
	cs4281_poke(card, BA0_ACCTL, BA0_ACCTL_VFRM | BA0_ACCTL_ESYN);

	for (t = 0; t < 10000; t++) {
		if ((cs4281_peek(card, BA0_ACISV) & (BA0_ACISV_SLV(3)|BA0_ACISV_SLV(4))) == (BA0_ACISV_SLV(3)|BA0_ACISV_SLV(4)))
			goto isv_ok;
		pds_delay_10us(10);
	}
	dbgprintf(("cs4281_chip_init: ISV3/4 not seen\n"));
	return 0;

isv_ok:
	/* commence digital audio transfer to the codec (slots 3+4 = PCM L/R) */
	cs4281_poke(card, BA0_ACOSV, BA0_ACOSV_SLV(3) | BA0_ACOSV_SLV(4));

	/* playback FIFO (DMA engine/FIFO index 0): left=slot0, right=slot1 */
	cs4281_poke(card, BA0_FCR0, BA0_FCR_FEN | BA0_FCR_LS(0) | BA0_FCR_RS(1) |
	                             BA0_FCR_SZ(CS4281_FIFO_SIZE) | BA0_FCR_OF(0));
	cs4281_poke(card, BA0_SRCSA, (0<<0) | (1<<8) | (10<<16) | (11<<24));

	/* digital volume trim: 0 = unattenuated (all real volume control goes
	 * through the AC97 mixer registers, same as every other card here) */
	cs4281_poke(card, BA0_PPLVC, 0);
	cs4281_poke(card, BA0_PPRVC, 0);

	/* CRITICAL: AC97 registers power up MUTED (bit15 set) with 0dB
	 * attenuation. AU_setmixer_outs() (via /VOL) only ever touches the
	 * attenuation bits and leaves the mute bit exactly as found here -
	 * so without this, these stay muted forever no matter what volume
	 * is requested (confirmed by testing - see SC_E1371.C's
	 * es1371_ac97_init(), which does the same thing for Master/PCM on
	 * the ES1371's identical AC97 mixer).
	 *
	 * Unlike SC_E1371.C, AC97_HEADPHONE_VOL is unmuted here too: cheap
	 * single-jack consumer cards (such as this CS4281-based Genius
	 * card) commonly wire their only physical output jack through the
	 * codec's headphone amplifier rather than "Line Out" - confirmed
	 * by testing, this register stayed muted and silent even with
	 * Master and PCM both unmuted.
	 */
	cs4281_ac97_write(card, AC97_MASTER_VOL_STEREO, 0x0C0C);
	cs4281_ac97_write(card, AC97_PCMOUT_VOL,        0x0C0C);
	cs4281_ac97_write(card, AC97_HEADPHONE_VOL,     0x0C0C);

	/* CD-IN: unmute too. This carries the ANALOG audio signal coming in
	 * on the card's 4-pin CD-audio header (from a CD-ROM drive's own
	 * analog output), entirely independent of any digital playback -
	 * it's a pure hardware passthrough through the codec's own mixer.
	 * Same root cause as Master/PCM/Headphone above: every AC97 volume
	 * register powers up muted, and nothing else was unmuting this one.
	 * Unlike the ES1370's AK4531, a standard AC97 codec's CD-In feeds
	 * the output mix automatically once unmuted - no separate routing/
	 * switch register is needed here.
	 */
	cs4281_ac97_write(card, AC97_CD_VOL, 0x0C0C);

#ifdef _DEBUG
	/* diagnostic-only: identify the codec and its output-routing type
	 * (HP/4CH/LNLVL - see the block comment above AC97_MUTE in AC97.H).
	 * A "4CH" codec needs its extended/surround section explicitly
	 * powered up via AC97_EXTENDED_STATUS bit PRJ before ANY analog
	 * output works, which nothing in this driver does yet.
	 */
	{
		unsigned long reset, extid, extstat, pwr, vid1, vid2, auxvol;
		reset   = cs4281_ac97_read(card, AC97_RESET);
		extid   = cs4281_ac97_read(card, AC97_EXTENDED_ID);
		extstat = cs4281_ac97_read(card, AC97_EXTENDED_STATUS);
		pwr     = cs4281_ac97_read(card, AC97_POWER_CONTROL);
		vid1    = cs4281_ac97_read(card, AC97_VENDOR_ID1);
		vid2    = cs4281_ac97_read(card, AC97_VENDOR_ID2);
		auxvol  = cs4281_ac97_read(card, 0x0004 /* re-read HEADPHONE_VOL after our write, for reference */);
		dbgprintf(("cs4281_chip_init: AC97 RESET=%X (HPsup=%d) EXTID=%X EXTSTAT=%X PWRCTL=%X VENDOR=%X%X HPVOL_now=%X\n",
			reset, (reset>>4)&1, extid, extstat, pwr, vid1, vid2, auxvol));
	}
#endif

	/* EAPD ("External Amplifier Power Down", reg 0x26 bit15) - on the
	 * CS4297A/compatible codec family used on this card, this bit
	 * disables an off-chip amplifier feeding the physical output jack.
	 * Its readback already showed bit15 clear (see the diagnostic
	 * above), but writing it explicitly costs nothing and closes off a
	 * well-documented, common "everything reports fine, but silent"
	 * failure mode for cards with an external amp. This register isn't
	 * touched by any other card driver in this project - none of them
	 * needed it, so it's easy to have missed.
	 */
	cs4281_ac97_write(card, AC97_POWER_CONTROL, 0x0000);

	cs4281_poke(card, BA0_HICR, BA0_HICR_EOI);
	/* unmask DMA engine 0's interrupt (our playback channel) AND the
	 * global DMAI summary bit - both are required, see BA0_HISR_DMAI
	 * comment above. */
	cs4281_poke(card, BA0_HIMR, 0x7fffffffUL & ~(BA0_HISR_DMA(0) | BA0_HISR_DMAI));

	dbgprintf(("cs4281_chip_init: exit, OK\n"));
	return 1;
}

static void cs4281_chip_close(struct cs4281_card_s *card)
/////////////////////////////////////////////////////////
{
	cs4281_poke(card, BA0_HIMR, 0x7fffffffUL); /* mask all interrupts */
	cs4281_poke(card, BA0_DCR0, BA0_DCR_MSK);
	cs4281_poke(card, BA0_DMR0, 0);
	cs4281_poke(card, BA0_CLKCR1, 0);
	cs4281_poke(card, BA0_SSPM, 0);
	cs4281_poke(card, BA0_SPMC, 0);
}

static void cs4281_prepare_playback( struct cs4281_card_s *card, struct audioout_info_s *aui )
///////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int rv;

	dbgprintf(("cs4281_prepare_playback: enter, dmasize=%X\n", aui->card_dmasize));

	cs4281_poke(card, BA0_DCR0, BA0_DCR_MSK); /* stop/mask channel while reprogramming */
	cs4281_poke(card, BA0_DMR0, 0);

	cs4281_poke(card, BA0_DBA0, (unsigned long) pds_cardmem_physicalptr(card->dm, card->pcmout_buffer));
	cs4281_poke(card, BA0_DBC0, (aui->card_dmasize >> 2) - 1); /* frames-1, 16-bit stereo = 4 bytes/frame */

	cs4281_poke(card, BA0_SRCSA, (0<<0) | (1<<8) | (10<<16) | (11<<24));

	rv = cs4281_rate_to_rv(aui->freq_card);
	cs4281_poke(card, BA0_DACSR, rv);

	/* re-assert the playback FIFO's slot mapping (shares FIFO index 0
	 * with the chip's FM/wavetable block - see cs4281_chip_init()) */
	cs4281_poke(card, BA0_FCR0, cs4281_peek(card, BA0_FCR0) & ~BA0_FCR_FEN);
	cs4281_poke(card, BA0_FCR0, BA0_FCR_FEN | BA0_FCR_LS(0) | BA0_FCR_RS(1) |
	                             BA0_FCR_SZ(CS4281_FIFO_SIZE) | BA0_FCR_OF(0));
	cs4281_poke(card, BA0_FSIC0, 0);

	dbgprintf(("cs4281_prepare_playback: exit\n"));
}

//-------------------------------------------------------------------------
static const struct pci_device_s cs4281_devices[] = {
 {"CS4281",0x1013,0x6005, 0},
 {NULL,0,0,0}
};

static void CS4281_close( struct audioout_info_s *aui );

static int CS4281_adetect( struct audioout_info_s *aui )
////////////////////////////////////////////////////////
{
	struct cs4281_card_s *card = aui->card_private_data;
	unsigned long physaddr;

	if(pcibios_search_devices( cs4281_devices, &card->pci_dev ) != PCI_SUCCESSFUL)
		return 0;

	dbgprintf(("CS4281_adetect: known card found, enable PCI io and busmaster\n"));
	/* NOTE: reverted from pcibios_enable_BM_MM() back to pcibios_enable_BM_IO().
	 * BA0 is a memory BAR, so BM_MM looked like the "more correct" call,
	 * but switching to it explicitly clears the I/O-space-enable bit in
	 * the PCI command register - and doing so coincided with a page
	 * fault (Exception 0E) at load time on real hardware. MMIO already
	 * worked fine with BM_IO in every earlier test (Memory Space Enable
	 * was apparently already set some other way, e.g. by the BIOS), so
	 * BM_MM wasn't fixing anything real - only adding risk. Kept as
	 * BM_IO until there's a concrete reason to revisit this. */
	pcibios_enable_BM_IO( &card->pci_dev );
	cs4281_set_power_d0( &card->pci_dev ); /* required - see comment above */

	physaddr = pcibios_ReadConfig_Dword(&card->pci_dev, PCIR_NAMBAR);
	aui->card_irq = card->pci_dev.bIrq;
	dbgprintf(("CS4281_adetect: vend_id=%X dev_id=%X BA0=%X irq=%u\n",
			  card->pci_dev.vendor_id,card->pci_dev.device_id,physaddr,aui->card_irq));

	if( physaddr & 0x1 ) { /* I/O space bit set? shouldn't happen, BA0 is memory-mapped */
		dbgprintf(("CS4281_adetect: BA0 looks like an I/O BAR (%X) - unexpected, aborting\n", physaddr));
		return 0;
	}
	physaddr &= 0xfffffff0UL;
	if(!physaddr)
		return 0;

	card->ba0_mapinfo.address = physaddr;
	card->ba0_mapinfo.size = CS4281_BA0_SIZE;
	if (__dpmi_physical_address_mapping(&card->ba0_mapinfo) != 0) {
		dbgprintf(("CS4281_adetect: MMIO mapping of BA0 failed\n"));
		return 0;
	}
	card->ba0 = card->ba0_mapinfo.address; /* mapping call rewrites .address to the linear addr */

	if( !cs4281_chip_init(card) ) {
		CS4281_close(aui);
		return 0;
	}
	if( !cs4281_buffer_init(card,aui) ) {
		CS4281_close(aui);
		return 0;
	}
	return 1;
}

static void CS4281_close( struct audioout_info_s *aui )
///////////////////////////////////////////////////////
{
	struct cs4281_card_s *card = aui->card_private_data;
	dbgprintf(("CS4281_close\n"));
	if( card ) {
		if( card->ba0 ) {
			cs4281_chip_close( card );
			card->ba0_mapinfo.address = LinearAddr( (void*)card->ba0 );
			__dpmi_free_physical_address_mapping( &card->ba0_mapinfo );
			card->ba0 = 0;
		}
		MDma_free_cardmem( &card->dm );
	}
}

static void CS4281_setrate( struct audioout_info_s *aui )
/////////////////////////////////////////////////////////
{
	struct cs4281_card_s *card = aui->card_private_data;

	dbgprintf(("CS4281_setrate\n"));
	if(aui->freq_card < 4000)
		aui->freq_card = 4000;
	else if(aui->freq_card > 48000)
		aui->freq_card = 48000;

	MDma_initbuf(aui, card->pcmout_bufsize);
	cs4281_prepare_playback(card,aui);
}

static void CS4281_start( struct audioout_info_s *aui )
///////////////////////////////////////////////////////
{
	struct cs4281_card_s *card = aui->card_private_data;
	uint32_t dmr = BA0_DMR_TYPE_SINGLE | BA0_DMR_AUTO | BA0_DMR_TR_READ;

	card->frag = 0;
	dbgprintf(("CS4281_start\n"));
	/* Force a clean (re)start of the DMA engine: write DMR with the
	 * DMA-enable bit still clear, THEN with it set (0->1 transition) -
	 * a single write with the bit already set was not enough to get
	 * audible output in testing. FIFO and unmask come last, matching
	 * ALSA's snd_cs4281_trigger() write order exactly (DMR, then FCR,
	 * then DCR - unmasking the channel is the very last step).
	 */
	cs4281_poke(card, BA0_DMR0, dmr);
	cs4281_poke(card, BA0_DMR0, dmr | BA0_DMR_DMA);
	cs4281_poke(card, BA0_FCR0, BA0_FCR_FEN | BA0_FCR_LS(0) | BA0_FCR_RS(1) |
	                             BA0_FCR_SZ(CS4281_FIFO_SIZE) | BA0_FCR_OF(0));
	cs4281_poke(card, BA0_DCR0, BA0_DCR_TCIE | BA0_DCR_HTCIE); /* MSK=0 last */

#ifdef _DEBUG
	/* diagnostic-only readback: nothing here changes behaviour. Logs the
	 * actual hardware state right after starting, so a stuck/ignored bit
	 * can be spotted directly instead of guessing from write-only logs.
	 *
	 * DCC0 is sampled repeatedly at a short, precisely-known interval
	 * (~2ms) rather than as a single before/after pair 50ms apart: at
	 * 22050Hz a 4096-byte (1024-frame) buffer takes ~46ms per full
	 * cycle, so two samples ~50ms apart can alias to nearly the same
	 * phase and look "stuck" even when DMA is actually running fine.
	 * A quick burst of samples makes the real direction/rate obvious:
	 * a steady run of ~44 counts between samples (in whichever
	 * direction DCC0 moves) is consistent with real playback at
	 * 22050Hz; near-zero movement across ALL samples means DMA truly
	 * is not progressing.
	 */
	{
		unsigned int t, i;
		uint32_t dcc0[8];
		dbgprintf(("CS4281_start: readback DMR0=%X DCR0=%X FCR0=%X HISR=%X ACOSV=%X ACSTS=%X PPLVC=%X PPRVC=%X DBC0=%X\n",
			cs4281_peek(card, BA0_DMR0), cs4281_peek(card, BA0_DCR0), cs4281_peek(card, BA0_FCR0),
			cs4281_peek(card, BA0_HISR), cs4281_peek(card, BA0_ACOSV), cs4281_peek(card, BA0_ACSTS),
			cs4281_peek(card, BA0_PPLVC), cs4281_peek(card, BA0_PPRVC), cs4281_peek(card, BA0_DBC0) ));
		for (i = 0; i < 8; i++) {
			dcc0[i] = cs4281_peek(card, BA0_DCC0);
			for (t = 0; t < 20; t++) pds_delay_10us(10); /* ~2ms */
		}
		dbgprintf(("CS4281_start: DCC0 burst (~2ms apart): %X %X %X %X %X %X %X %X\n",
			dcc0[0],dcc0[1],dcc0[2],dcc0[3],dcc0[4],dcc0[5],dcc0[6],dcc0[7] ));
	}
#endif
}

static void CS4281_stop( struct audioout_info_s *aui )
//////////////////////////////////////////////////////
{
	struct cs4281_card_s *card = aui->card_private_data;
	cs4281_poke(card, BA0_DMR0, 0); /* DMA=0 */
	cs4281_poke(card, BA0_FCR0, cs4281_peek(card, BA0_FCR0) & ~BA0_FCR_FEN);
	cs4281_poke(card, BA0_DCR0, BA0_DCR_MSK); /* mask channel last */
}

static unsigned int CS4281_getbufpos( struct audioout_info_s *aui )
///////////////////////////////////////////////////////////////////
{
	struct cs4281_card_s *card = aui->card_private_data;
	unsigned int dmasize_frames = aui->card_dmasize >> 2;
	unsigned int dcc = cs4281_peek(card, BA0_DCC0) & 0xffffff;
	unsigned int bufpos_frames = dmasize_frames - (dcc + 1);
	return bufpos_frames << 2;
}

static void CS4281_writeMIXER( struct audioout_info_s *aui, unsigned long reg, unsigned long val )
//////////////////////////////////////////////////////////////////////////////////////////////////
{
	dbgprintf(("CS4281_writeMIXER(%X,%X)\n", reg, val ));
	cs4281_ac97_write( aui->card_private_data, reg, val );
}

static unsigned long CS4281_readMIXER( struct audioout_info_s *aui, unsigned long reg )
///////////////////////////////////////////////////////////////////////////////////////
{
	return cs4281_ac97_read( aui->card_private_data, reg );
}

static int CS4281_IRQRoutine( struct audioout_info_s *aui )
///////////////////////////////////////////////////////////
{
	struct cs4281_card_s *card = aui->card_private_data;
	uint32_t status = cs4281_peek(card, BA0_HISR);
	int ours = 0;

	/* Do NOT dbgprintf() in here, even with /LF: - after the DMAI fix
	 * above, this handler actually runs continuously once playback
	 * starts (previously it never ran at all, since interrupts never
	 * arrived - that absence is exactly what led to finding the DMAI
	 * bug). A debug build that logged from here reproducibly crashed
	 * at load time once interrupts started actually firing. Matches
	 * ES1371_IRQRoutine() in SC_E1371.C, whose own debug line is
	 * likewise commented out. */

	if (status & BA0_HISR_DMA(0)) {
		uint32_t hdsr = cs4281_peek(card, BA0_HDSR0); /* ack this DMA engine's pending status */
		/* CS4281 hardware quirk (documented in ALSA's cs4281.c): the
		 * chip sometimes reports the SAME half/full transfer boundary
		 * twice in a row instead of alternating. Track expected
		 * parity via card->frag and ignore a duplicate report at the
		 * same boundary, exactly as ALSA does - this avoids treating
		 * a single real event as two, which (left unhandled) may be
		 * why this card's interrupt kept re-triggering hard enough to
		 * crash once DMAI was unmasked. */
		card->frag++;
		if ((hdsr & BA0_HDSR_DHTC) && !(card->frag & 1))
			card->frag--;
		else if ((hdsr & BA0_HDSR_DTC) && (card->frag & 1))
			card->frag--;
		ours = 1;
	}
	cs4281_poke(card, BA0_HICR, BA0_HICR_EOI);
	return ours;
}

const struct sndcard_info_s CS4281_sndcard_info = {
 "CS4281",
 0,
 &CS4281_adetect,
 &CS4281_start,
 &CS4281_stop,
 &CS4281_close,
 &CS4281_setrate,

 &MDma_writedata,
 &CS4281_getbufpos,
 &CS4281_IRQRoutine,
 &CS4281_writeMIXER,
 &CS4281_readMIXER,
 aucards_ac97chan_mixerset,
 sizeof(struct cs4281_card_s)
};
