//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2009 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: AK4531 mixer definitions (for ES1370-based SB PCI64/128 cards)
//
//Unlike AC97MIX.C, each AK4531 channel uses TWO separate 8-bit registers
//for left/right (not one register with a left/right bitfield), and the
//codec cannot be read back from hardware - card_readmixer() (see
//SC_E1370.C) returns a value cached in software instead.

#include <stdint.h>
#include <stddef.h>

#include "AU_CARDS.H"
#include "AK4531MIX.H"

static const struct aucards_mixerchan_s aucards_ak4531chan_master_vol = {
	AU_MIXCHAN_MASTER,AU_MIXCHANFUNC_VOLUME,2,{
		{ AK4531_LMASTER,5,0,SUBMIXCH_INFOBIT_REVERSEDVALUE },
		{ AK4531_RMASTER,5,0,SUBMIXCH_INFOBIT_REVERSEDVALUE }
	}};

/* the "PCM"/voice channel is the DAC output level - this is the one that
 * actually controls how loud the digital audio played by VSBHDA is.
 */
static const struct aucards_mixerchan_s aucards_ak4531chan_pcm_vol = {
	AU_MIXCHAN_PCM,AU_MIXCHANFUNC_VOLUME,2,{
		{ AK4531_LVOICE,5,0,SUBMIXCH_INFOBIT_REVERSEDVALUE },
		{ AK4531_RVOICE,5,0,SUBMIXCH_INFOBIT_REVERSEDVALUE }
	}};

static const struct aucards_mixerchan_s aucards_ak4531chan_micin_vol = {
	AU_MIXCHAN_MICIN,AU_MIXCHANFUNC_VOLUME,1, {
		{ AK4531_MIC,5,0,SUBMIXCH_INFOBIT_REVERSEDVALUE }
	}};

static const struct aucards_mixerchan_s aucards_ak4531chan_linein_vol = {
	AU_MIXCHAN_LINEIN,AU_MIXCHANFUNC_VOLUME,2, {
		{ AK4531_LLINE,5,0,SUBMIXCH_INFOBIT_REVERSEDVALUE },
		{ AK4531_RLINE,5,0,SUBMIXCH_INFOBIT_REVERSEDVALUE }
	}};

static const struct aucards_mixerchan_s aucards_ak4531chan_cdin_vol = {
	AU_MIXCHAN_CDIN,AU_MIXCHANFUNC_VOLUME,2, {
		{ AK4531_LCD,5,0,SUBMIXCH_INFOBIT_REVERSEDVALUE },
		{ AK4531_RCD,5,0,SUBMIXCH_INFOBIT_REVERSEDVALUE }
	}};

static const struct aucards_mixerchan_s aucards_ak4531chan_auxin_vol = {
	AU_MIXCHAN_AUXIN,AU_MIXCHANFUNC_VOLUME,2, {
		{ AK4531_LAUXA,5,0,SUBMIXCH_INFOBIT_REVERSEDVALUE },
		{ AK4531_RAUXA,5,0,SUBMIXCH_INFOBIT_REVERSEDVALUE }
	}};

/* note: AK4531 has no dedicated headphone or S/PDIF output, so
 * AU_MIXCHAN_HEADPHONE / AU_MIXCHAN_SPDIFOUT are intentionally not listed
 * here. AU_setmixer_one() looks channels up via AU_search_mixerchan() and
 * silently does nothing if a channel isn't present in this table, so
 * AU_setmixer_outs() (which touches MASTER/PCM/HEADPHONE/SPDIFOUT) remains
 * safe to call unmodified.
 */
const struct aucards_mixerchan_s *aucards_ak4531chan_mixerset[] = {
	&aucards_ak4531chan_master_vol,
	&aucards_ak4531chan_pcm_vol,
	&aucards_ak4531chan_micin_vol,
	&aucards_ak4531chan_linein_vol,
	&aucards_ak4531chan_cdin_vol,
	&aucards_ak4531chan_auxin_vol,
	NULL
};
