//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2015 by PDSoft (Attila Padar)                *
//*                 http://mpxplay.sourceforge.net                         *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: audio main functions

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "CONFIG.H"
#include "MPXPLAY.H"
#include "DMAIRQ.H"
#ifndef SBEMU
#include "AU_MIXER.H"
#endif

static unsigned int cardinit(struct mpxplay_audioout_info_s *aui);
static unsigned int carddetect(struct mpxplay_audioout_info_s *aui);

static int aucards_writedata_intsound(struct mpxplay_audioout_info_s *aui,unsigned long outbytes_left);
//static void aucards_dma_monitor(void);
#ifndef SBEMU
static void aucards_interrupt_decoder(void);
#endif

extern one_sndcard_info ES1371_sndcard_info;
extern one_sndcard_info ICH_sndcard_info;
extern one_sndcard_info IHD_sndcard_info;
extern one_sndcard_info VIA82XX_sndcard_info;
#if SBLIVE
extern one_sndcard_info SBLIVE_sndcard_info;
#endif

static one_sndcard_info *all_sndcard_info[]={
 &ES1371_sndcard_info,
 &ICH_sndcard_info,
 &IHD_sndcard_info,
#if SBLIVE
 &SBLIVE_sndcard_info,
#endif
 &VIA82XX_sndcard_info,
 NULL
};

static bool bPlaying;

/* scan for audio devices */

int AU_init(struct mpxplay_audioout_info_s *aui)
////////////////////////////////////////////////
{
	one_sndcard_info **asip;

	dbgprintf("AU_init\n");
	aui->card_dmasize = aui->card_dma_buffer_size = MDma_get_max_pcmoutbufsize( aui, 65535, 4608, 2, 0);

	if(!(aui->card_controlbits & AUINFOS_CARDCNTRLBIT_TESTCARD)){
		asip = &all_sndcard_info[0];
		aui->card_handler = *asip;
		do{
			if(!(aui->card_handler->infobits & SNDCARD_SELECT_ONLY))
				if( cardinit(aui) )
					break;
			asip++;
			aui->card_handler = *asip;
		}while(aui->card_handler);
	}

	if(!aui->card_handler || (aui->card_controlbits & AUINFOS_CARDCNTRLBIT_TESTCARD)){
		asip = &all_sndcard_info[0];
		aui->card_handler = *asip;
		do{
			dbgprintf("AU_init: checking card %s\n", aui->card_handler->shortname);
			if( aui->card_handler->card_detect ) {
				if( carddetect( aui ) )
					break;
			}
			asip++;
			aui->card_handler = *asip;
		} while( aui->card_handler );
	}

	if( !aui->card_handler )
		return(0);

	aui->freq_card = aui->chan_card = aui->bits_card = 0;
	return(1);
}

static unsigned int cardinit(struct mpxplay_audioout_info_s *aui)
/////////////////////////////////////////////////////////////////
{
	if(aui->card_handler->card_init)
		if(aui->card_handler->card_init(aui))
			return(1);
	return(0);
}

static unsigned int carddetect(struct mpxplay_audioout_info_s *aui)
///////////////////////////////////////////////////////////////////
{
	aui->card_wave_id = MPXPLAY_WAVEID_PCM_SLE; // integer pcm
	aui->bits_card = 16;
	aui->bytespersample_card = aui->bits_card/8;
	aui->card_port = aui->card_type = 0xffff;
	aui->card_irq = aui->card_isa_dma = aui->card_isa_hidma = 0xff;
	aui->freq_card = 44100;
	aui->chan_card = 2;

	if( aui->card_handler->card_detect )
		if( aui->card_handler->card_detect(aui) )
			return(1);
	return(0);
}

void AU_ini_interrupts(struct mpxplay_audioout_info_s *aui)
///////////////////////////////////////////////////////////
{
}

void AU_del_interrupts(struct mpxplay_audioout_info_s *aui)
///////////////////////////////////////////////////////////
{
	dbgprintf("AU_del_interrupts\n");
	AU_close(aui);
}

void AU_prestart(struct mpxplay_audioout_info_s *aui)
/////////////////////////////////////////////////////
{
	if(aui->card_controlbits & AUINFOS_CARDCNTRLBIT_DMACLEAR)
		AU_clearbuffs(aui);
	bPlaying = true;
}

void AU_start(struct mpxplay_audioout_info_s *aui)
//////////////////////////////////////////////////
{
	if(!(aui->card_infobits & AUINFOS_CARDINFOBIT_PLAYING)){
		MPXPLAY_INTSOUNDDECODER_DISALLOW;

		if(aui->card_controlbits & AUINFOS_CARDCNTRLBIT_DMACLEAR)
			AU_clearbuffs(aui);
		if(aui->card_handler->card_start)
			aui->card_handler->card_start(aui);
		aui->card_infobits |= AUINFOS_CARDINFOBIT_PLAYING;

		MPXPLAY_INTSOUNDDECODER_ALLOW;
	}
	bPlaying = true;
	aui->card_infobits |= AUINFOS_CARDINFOBIT_DMAFULL;
}

void AU_stop(struct mpxplay_audioout_info_s *aui)
/////////////////////////////////////////////////
{
	bPlaying = false;

	if(aui->card_infobits & AUINFOS_CARDINFOBIT_PLAYING){
		aui->card_infobits &= ~AUINFOS_CARDINFOBIT_PLAYING;
		MPXPLAY_INTSOUNDDECODER_DISALLOW;

		if(aui->card_handler && aui->card_handler->card_stop)
			aui->card_handler->card_stop(aui);
		aui->card_dmafilled = aui->card_dmalastput;
		aui->card_dmaspace = aui->card_dmasize - aui->card_dmafilled;
		aui->card_infobits &= ~AUINFOS_CARDINFOBIT_DMAUNDERRUN;

		MPXPLAY_INTSOUNDDECODER_ALLOW;
	}
}

void AU_close(struct mpxplay_audioout_info_s *aui)
//////////////////////////////////////////////////
{
	if(!aui)
		return;
	AU_stop(aui);
	if(aui->card_handler && aui->card_handler->card_close)
		aui->card_handler->card_close(aui);
}

void AU_pause_process(struct mpxplay_audioout_info_s *aui)
//////////////////////////////////////////////////////////
{
}

void AU_clearbuffs(struct mpxplay_audioout_info_s *aui)
///////////////////////////////////////////////////////
{
	if(aui->card_handler->cardbuf_clear)
		aui->card_handler->cardbuf_clear(aui);
	aui->card_controlbits &= ~AUINFOS_CARDCNTRLBIT_DMACLEAR;
}

void AU_setrate(struct mpxplay_audioout_info_s *aui,struct mpxplay_audio_decoder_info_s *adi)
/////////////////////////////////////////////////////////////////////////////////////////////

{
	unsigned int new_cardcontrolbits;
	dbgprintf("AU_setrate enter\n");

	aui->chan_song = adi->outchannels;
	aui->bits_song = adi->bits;

	new_cardcontrolbits = aui->card_controlbits;

	if(adi->infobits & ADI_FLAG_BITSTREAMOUT){
		new_cardcontrolbits |= AUINFOS_CARDCNTRLBIT_BITSTREAMOUT;
		if(adi->infobits & ADI_FLAG_BITSTREAMHEAD)
			new_cardcontrolbits |= AUINFOS_CARDCNTRLBIT_BITSTREAMHEAD;
		if(adi->infobits & ADI_FLAG_BITSTREAMNOFRH)
			new_cardcontrolbits |= AUINFOS_CARDCNTRLBIT_BITSTREAMNOFRH;
	}else{
		new_cardcontrolbits &= ~( AUINFOS_CARDCNTRLBIT_BITSTREAMOUT | AUINFOS_CARDCNTRLBIT_BITSTREAMHEAD | AUINFOS_CARDCNTRLBIT_BITSTREAMNOFRH );
	}
	if(new_cardcontrolbits & AUINFOS_CARDCNTRLBIT_BITSTREAMOUT)
		aui->card_wave_name = adi->shortname;

	// We (stop and) reconfigure the card if the frequency has changed (and crossfade is disabled)
	// The channel and bit differences are allways handled by the AU_MIXER

	if( (aui->freq_set && (aui->freq_set != aui->freq_card))
	   || (!aui->freq_set && (aui->freq_song != adi->freq ))
	   || (aui->card_controlbits & AUINFOS_CARDCNTRLBIT_UPDATEFREQ)
	   || (new_cardcontrolbits != aui->card_controlbits)
	   || ((aui->card_controlbits & AUINFOS_CARDCNTRLBIT_BITSTREAMOUT) && (aui->card_wave_id != adi->wave_id))
	   || (aui->card_handler->infobits & SNDCARD_SETRATE)
	  ){
		dbgprintf("AU_setrate: changing rate\n");
		if(aui->card_handler->infobits & SNDCARD_SETRATE){ // !!!
			if(aui->card_handler->card_stop)
				aui->card_handler->card_stop(aui);
		} else {
			if( bPlaying ){
				AU_stop(aui);
			}
		}

		aui->freq_song = adi->freq;

		aui->freq_card = (aui->freq_set) ? aui->freq_set : adi->freq;
		aui->chan_card = (aui->chan_set) ? aui->chan_set : adi->outchannels;
		aui->bits_card = (aui->bits_set) ? aui->bits_set : adi->bits;
		if(new_cardcontrolbits & AUINFOS_CARDCNTRLBIT_BITSTREAMOUT)
			aui->card_wave_id=adi->wave_id;
		else {
			aui->card_wave_id = MPXPLAY_WAVEID_PCM_SLE; // integer pcm
		}
		aui->bytespersample_card = 0;
		aui->card_controlbits = new_cardcontrolbits;
		aui->card_infobits &= ~(AUINFOS_CARDINFOBIT_BITSTREAMOUT | AUINFOS_CARDINFOBIT_BITSTREAMNOFRH);

		MPXPLAY_INTSOUNDDECODER_DISALLOW;    // ???
		if( aui->card_handler->card_setrate )
			aui->card_handler->card_setrate(aui);
		MPXPLAY_INTSOUNDDECODER_ALLOW;       // ???

		if(aui->card_wave_id==MPXPLAY_WAVEID_PCM_FLOAT)
			aui->bytespersample_card = 4;
		else
			if(!aui->bytespersample_card) // card haven't set it (not implemented in the au_mixer yet!: bits/8 !=bytespersample_card)
				aui->bytespersample_card=(aui->bits_card+7)/8;

		aui->card_controlbits |= AUINFOS_CARDCNTRLBIT_DMACLEAR;
		aui->card_controlbits &= ~AUINFOS_CARDCNTRLBIT_UPDATEFREQ;

		if(aui->freq_set) aui->freq_set = aui->freq_card;
		if(aui->chan_set) aui->chan_set = aui->chan_card;
		if(aui->bits_set) aui->bits_set = aui->bits_card;

		if(aui->card_infobits & AUINFOS_CARDINFOBIT_BITSTREAMOUT){
			if(!(aui->card_infobits & AUINFOS_CARDINFOBIT_BITSTREAMNOFRH))
				adi->infobits &= ~ADI_CNTRLBIT_BITSTREAMNOFRH;
			aui->bytespersample_card = 1;
		}else{
			aui->card_controlbits &= ~(AUINFOS_CARDCNTRLBIT_BITSTREAMOUT | AUINFOS_CARDCNTRLBIT_BITSTREAMHEAD);
			adi->infobits &= ~(ADI_FLAG_BITSTREAMOUT | ADI_FLAG_BITSTREAMHEAD | ADI_CNTRLBIT_BITSTREAMOUT | ADI_CNTRLBIT_BITSTREAMNOFRH);
		}

		aui->card_bytespersign = aui->chan_card*aui->bytespersample_card;
#ifdef SBEMU
		aui->card_outbytes = aui->card_dmasize;
#else
		aui->card_outbytes = aui->card_dmasize/4; // ??? for interrupt_decoder
#endif
	}
	aui->freq_song = adi->freq;
}

void AU_setmixer_init(struct mpxplay_audioout_info_s *aui)
//////////////////////////////////////////////////////////
{
	unsigned int c;

	aui->card_master_volume = -1;

	for( c=0; c < AU_MIXCHANS_NUM; c++)
		aui->card_mixer_values[c] = -1;
}

static aucards_onemixerchan_s *AU_search_mixerchan(aucards_allmixerchan_s *mixeri,unsigned int mixchannum)
//////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int i=0;
	while(*mixeri){
		if((*mixeri)->mixchan == mixchannum)
			return (*mixeri);
		if(++i >= AU_MIXCHANS_NUM)
			break;
		mixeri++;
	}
	return NULL;
}

/* set mixer volume for what exactly?
 * mixchannum???
 */

void AU_setmixer_one( struct mpxplay_audioout_info_s *aui, unsigned int mixchannum, unsigned int setmode, int newvalue)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	one_sndcard_info *cardi;
	aucards_onemixerchan_s *onechi; // one mixer channel infos (master,pcm,etc.)
	unsigned int subchannelnum, sch, channel, function;
	long newpercentval, maxpercentval;

	dbgprintf("AU_setmixer_one( %u, %u, %u )\n", mixchannum, setmode, newvalue );
	//mixer structure/values verifying
	function = AU_MIXCHANFUNCS_GETFUNC(mixchannum);
	if(function >= AU_MIXCHANFUNCS_NUM)
		return;
	channel = AU_MIXCHANFUNCS_GETCHAN(mixchannum);
	if(channel > AU_MIXCHANS_NUM)
		return;
	cardi = aui->card_handler;
	if(!cardi)
		return;
	if(!cardi->card_writemixer || !cardi->card_readmixer || !cardi->card_mixerchans)
		return;
	onechi = AU_search_mixerchan(cardi->card_mixerchans,mixchannum);
	if(!onechi)
		return;
	subchannelnum = onechi->subchannelnum;
	if(!subchannelnum || (subchannelnum>AU_MIXERCHAN_MAX_SUBCHANNELS))
		return;

	switch(mixchannum){
	case AU_MIXCHAN_BASS:
	case AU_MIXCHAN_TREBLE: maxpercentval = AU_MIXCHAN_MAX_VALUE_TONE; break;
	default: maxpercentval = AU_MIXCHAN_MAX_VALUE_VOLUME; break;
	}

	//calculate new percent
	switch(setmode){
	case MIXER_SETMODE_ABSOLUTE:
		dbgprintf("AU_setmixer_one( SETMOVE_ABSOLUE, %u %% )\n", newvalue );
		newpercentval = newvalue;
	break;
	case MIXER_SETMODE_RELATIVE:
		if(function == AU_MIXCHANFUNC_VOLUME)
			newpercentval = aui->card_mixer_values[channel] + newvalue;
		else
			if(newvalue < 0)
				newpercentval = 0;
			else
				newpercentval = maxpercentval;
		break;
	default:
		return;
	}
	if(newpercentval < 0)
		newpercentval = 0;
	if(newpercentval > maxpercentval)
		newpercentval = maxpercentval;

	MPXPLAY_INTSOUNDDECODER_DISALLOW;
	//ENTER_CRITICAL;

	//read current register value, mix it with the new one, write it back
	for(sch=0;sch<subchannelnum;sch++){
		aucards_submixerchan_s *subchi=&(onechi->submixerchans[sch]); // one subchannel infos (left,right,etc.)
		unsigned long currchval,newchval;

		if((subchi->submixch_register>AU_MIXERCHAN_MAX_REGISTER) || !subchi->submixch_max || (subchi->submixch_shift>AU_MIXERCHAN_MAX_BITS)) // invalid subchannel infos
			continue;

        /* don't use floats here - function is called during interrupt time */
		//newchval=(long)(((float)newpercentval*(float)subchi->submixch_max+((float)((maxpercentval >> 1) - 1)))/(float)maxpercentval);   // percent to chval (rounding up)
		newchval=((newpercentval * subchi->submixch_max + (((maxpercentval >> 1) - 1))) / maxpercentval);   // percent to chval (rounding up)
		if( newchval > subchi->submixch_max)
			newchval = subchi->submixch_max;
		if(subchi->submixch_infobits & SUBMIXCH_INFOBIT_REVERSEDVALUE)   // reverse value if required
			newchval = subchi->submixch_max-newchval;

		newchval <<= subchi->submixch_shift;                           // shift to position

		currchval=cardi->card_readmixer(aui,subchi->submixch_register);// read current value
		currchval&=~(subchi->submixch_max<<subchi->submixch_shift);    // unmask
		newchval=(currchval|newchval);                                 // add new value

		cardi->card_writemixer(aui,subchi->submixch_register,newchval);// write it back
	}
	//LEAVE_CRITICAL;
	MPXPLAY_INTSOUNDDECODER_ALLOW;
	if( function==AU_MIXCHANFUNC_VOLUME )
		aui->card_mixer_values[channel]=newpercentval;
}

static int AU_getmixer_one(struct mpxplay_audioout_info_s *aui,unsigned int mixchannum)
///////////////////////////////////////////////////////////////////////////////////////
{
	one_sndcard_info *cardi;
	aucards_onemixerchan_s *onechi; // one mixer channel infos (master,pcm,etc.)
	aucards_submixerchan_s *subchi; // one subchannel infos (left,right,etc.)
	unsigned long channel,function,subchannelnum;
	long value,maxpercentval;

	//mixer structure/values verifying
	function = AU_MIXCHANFUNCS_GETFUNC(mixchannum);
	if(function >= AU_MIXCHANFUNCS_NUM)
		return -1;
	channel = AU_MIXCHANFUNCS_GETCHAN(mixchannum);
	if(channel > AU_MIXCHANS_NUM)
		return -1;
	cardi = aui->card_handler;
	if(!cardi)
		return -1;
	if(!cardi->card_readmixer || !cardi->card_mixerchans)
		return -1;
	onechi = AU_search_mixerchan(cardi->card_mixerchans,mixchannum);
	if(!onechi)
		return -1;
	subchannelnum=onechi->subchannelnum;
	if(!subchannelnum || (subchannelnum>AU_MIXERCHAN_MAX_SUBCHANNELS))
		return -1;

	switch(mixchannum){
	case AU_MIXCHAN_BASS:
	case AU_MIXCHAN_TREBLE: maxpercentval = AU_MIXCHAN_MAX_VALUE_TONE; break;
	default: maxpercentval = AU_MIXCHAN_MAX_VALUE_VOLUME; break;
	}

	// we read one (the left at stereo) sub-channel only
	subchi = &(onechi->submixerchans[0]);
	if((subchi->submixch_register>AU_MIXERCHAN_MAX_REGISTER) || (subchi->submixch_shift>AU_MIXERCHAN_MAX_BITS)) // invalid subchannel infos
		return -1;

	value = cardi->card_readmixer(aui,subchi->submixch_register); // read
	value >>= subchi->submixch_shift;                             // shift
	value &= subchi->submixch_max;                                // mask

	if(subchi->submixch_infobits & SUBMIXCH_INFOBIT_REVERSEDVALUE)// reverse value if required
		value = subchi->submixch_max - value;

	//value=(long)((float)value*(float)maxpercentval/(float)subchi->submixch_max);       // chval to percent
	value = ( value * maxpercentval / subchi->submixch_max );       // chval to percent
	if( value > maxpercentval )
		value = maxpercentval;
	return value;
}

#define AU_MIXCHANS_OUTS 4

static const unsigned int au_mixchan_outs[AU_MIXCHANS_OUTS] = {
	AU_MIXCHAN_MASTER, AU_MIXCHAN_PCM, AU_MIXCHAN_HEADPHONE, AU_MIXCHAN_SPDIFOUT };

void AU_setmixer_outs(struct mpxplay_audioout_info_s *aui,unsigned int setmode,int newvalue)
////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int i;

	for( i = 0; i < AU_MIXCHANS_OUTS; i++ )
		AU_setmixer_one( aui, AU_MIXCHANFUNCS_PACK(au_mixchan_outs[i], AU_MIXCHANFUNC_VOLUME ), setmode, newvalue );

	aui->card_master_volume = aui->card_mixer_values[AU_MIXCHAN_MASTER]; // ???
}

void AU_setmixer_all(struct mpxplay_audioout_info_s *aui)
/////////////////////////////////////////////////////////
{
	unsigned int i;
	int vol=aui->card_master_volume;

	if(vol >= 0) // we set all output channels to the master volume
		for( i = 0; i < AU_MIXCHANS_OUTS; i++)
			if(aui->card_mixer_values[au_mixchan_outs[i]] < 0) // except the separated settings
				aui->card_mixer_values[au_mixchan_outs[i]] = vol;

	for( i = 0; i < AU_MIXCHANS_NUM; i++ ){
		vol = aui->card_mixer_values[i];
		if(vol >= 0){
#ifdef AU_AUTO_UNMUTE
			AU_setmixer_one(aui,AU_MIXCHANFUNCS_PACK(i,AU_MIXCHANFUNC_MUTE),MIXER_SETMODE_ABSOLUTE, ((i==AU_MIXCHAN_BASS) || (i==AU_MIXCHAN_TREBLE))? AU_MIXCHAN_MAX_VALUE_TONE:AU_MIXCHAN_MAX_VALUE_VOLUME);
#endif
			AU_setmixer_one(aui,AU_MIXCHANFUNCS_PACK(i,AU_MIXCHANFUNC_VOLUME),MIXER_SETMODE_ABSOLUTE,vol);
		}else{
			vol = AU_getmixer_one(aui,AU_MIXCHANFUNCS_PACK(i,AU_MIXCHANFUNC_VOLUME));
			if(vol >= 0)
				aui->card_mixer_values[i] = vol;
		}
	}
}

#define SOUNDCARD_BUFFER_PROTECTION 32 // in bytes (requried for PCI cards)

#ifndef SBEMU
static
#endif
unsigned int AU_cardbuf_space(struct mpxplay_audioout_info_s *aui)
//////////////////////////////////////////////////////////////////
{
	unsigned long buffer_protection;

	buffer_protection = SOUNDCARD_BUFFER_PROTECTION;     // rounding to bytespersign
	buffer_protection += aui->card_bytespersign-1;
	buffer_protection -= (buffer_protection % aui->card_bytespersign);

	if(aui->card_dmalastput >= aui->card_dmasize) // checking
		aui->card_dmalastput = 0;

	if(aui->card_handler->cardbuf_pos){
		if(aui->card_handler->infobits & SNDCARD_CARDBUF_SPACE){
			if(aui->card_infobits & AUINFOS_CARDINFOBIT_PLAYING){
				aui->card_dmaspace = aui->card_handler->cardbuf_pos(aui);
				aui->card_dmaspace -= (aui->card_dmaspace%aui->card_bytespersign); // round
			}else
				aui->card_dmaspace = (aui->card_dmaspace>aui->card_outbytes)? (aui->card_dmaspace-aui->card_outbytes):0;
		}else{
			unsigned long bufpos;

			if(aui->card_infobits & AUINFOS_CARDINFOBIT_PLAYING){
				bufpos = aui->card_handler->cardbuf_pos(aui);
				if(bufpos >= aui->card_dmasize)  // checking
					bufpos = 0;
				else
					bufpos -= (bufpos%aui->card_bytespersign); // round

				if(aui->card_infobits & AUINFOS_CARDINFOBIT_DMAUNDERRUN){   // sets a new put-pointer in this case
					if(bufpos >= aui->card_outbytes)
						aui->card_dmalastput = bufpos-aui->card_outbytes;
					else
						aui->card_dmalastput = aui->card_dmasize + bufpos - aui->card_outbytes;
					aui->card_infobits &= ~AUINFOS_CARDINFOBIT_DMAUNDERRUN;
				}
			} else {
				bufpos=0;
			}

			//if(aui->card_dmalastput>=aui->card_dmasize) // checking
			// aui->card_dmalastput=0;

			if(bufpos > aui->card_dmalastput)
				aui->card_dmaspace = bufpos-aui->card_dmalastput;
			else
				aui->card_dmaspace = aui->card_dmasize-aui->card_dmalastput + bufpos;
		}
	}else{
		aui->card_dmaspace = aui->card_outbytes+buffer_protection;
		aui->card_infobits |= AUINFOS_CARDINFOBIT_DMAFULL;
	}

	if(aui->card_dmaspace > aui->card_dmasize) // checking
		aui->card_dmaspace = aui->card_dmasize;

	aui->card_dmafilled = aui->card_dmasize-aui->card_dmaspace;

	return (aui->card_dmaspace > buffer_protection) ? (aui->card_dmaspace-buffer_protection) : 0;
}

int AU_writedata(struct mpxplay_audioout_info_s *aui)
/////////////////////////////////////////////////////
{
	unsigned int outbytes_left;

	if(!aui->samplenum)
		return 0;

	if(!(aui->card_infobits & AUINFOS_CARDINFOBIT_BITSTREAMOUT)) {
		aui->samplenum-=(aui->samplenum%aui->chan_card); // if samplenum is buggy (round to chan_card)
		outbytes_left = aui->samplenum * aui->bytespersample_card;
	}else
		outbytes_left = aui->samplenum;

#ifdef SBEMU
	aui->card_outbytes = min(outbytes_left,(aui->card_dmasize));
#else
	aui->card_outbytes = min(outbytes_left,(aui->card_dmasize/4));
#endif

	if(!(aui->card_infobits & AUINFOS_CARDINFOBIT_BITSTREAMOUT))
		aui->card_outbytes -= (aui->card_outbytes % aui->card_bytespersign);

	int left = aucards_writedata_intsound(aui,outbytes_left);

	aui->samplenum = 0;

	//slow processor test :)
	//{
	// unsigned int i;
	// for(i=0;i<0x0080ffff;i++);
	//}
	return left/aui->bytespersample_card;
}

#if 0
static int aucards_writedata_normal(struct mpxplay_audioout_info_s *aui,unsigned long outbytes_left)
////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned long space,first;
	char *pcm_outdata=(char *)aui->pcm_sample;

	aui->card_infobits &= ~AUINFOS_CARDINFOBIT_DMAFULL;
	allcputime += outbytes_left;
	first=1;

	do{
		space = AU_cardbuf_space(aui);            // pre-checking (because it's not called before)
		if(first){
			allcpuusage += space; // CPU usage
			first = 0;
		}
		if(space <= aui->card_outbytes){
			AU_start(aui); // start playing (only then) if the DMA buffer is full
			if(aui->card_controlbits & AUINFOS_CARDCNTRLBIT_DMADONTWAIT){
				aui->card_controlbits &= ~AUINFOS_CARDCNTRLBIT_DMADONTWAIT;
				return aui->card_outbytes;
			}
		}
		if(space>=aui->card_bytespersign){
			unsigned int outbytes_putblock=min(space,outbytes_left);

			aui->card_handler->cardbuf_writedata(aui,pcm_outdata,outbytes_putblock);
			pcm_outdata += outbytes_putblock;
			outbytes_left -= outbytes_putblock;

			aui->card_dmafilled += outbytes_putblock; // dma monitor needs this
		}
		if(!outbytes_left)
			break;
	}while(1);
	return 0;
}
#endif

static int aucards_writedata_intsound(struct mpxplay_audioout_info_s *aui,unsigned long outbytes_left)
//////////////////////////////////////////////////////////////////////////////////////////////////////
{
	char *pcm_outdata = (char *)aui->pcm_sample;
	unsigned long buffer_protection,space;

	buffer_protection = SOUNDCARD_BUFFER_PROTECTION;
	buffer_protection += aui->card_bytespersign - 1;
	buffer_protection -= (buffer_protection%aui->card_bytespersign);

	space= (aui->card_dmaspace > buffer_protection) ? (aui->card_dmaspace-buffer_protection) : 0;

	do{
		if( space >= aui->card_bytespersign ) {
			unsigned int outbytes_putblock = min( space, outbytes_left);
			aui->card_handler->cardbuf_writedata( aui, pcm_outdata, outbytes_putblock );
			pcm_outdata += outbytes_putblock;
			outbytes_left -= outbytes_putblock;
#ifdef SBEMU
			space -= outbytes_putblock;
#endif
			aui->card_dmafilled += outbytes_putblock;
			if(aui->card_dmafilled > aui->card_dmasize)
				aui->card_dmafilled = aui->card_dmasize;
			if(aui->card_dmaspace > outbytes_putblock)
				aui->card_dmaspace -= outbytes_putblock;
			else
				aui->card_dmaspace = 0;
		}
		if(!outbytes_left)
			break;
#ifndef SBEMU
		space = AU_cardbuf_space(aui); // post-checking (because aucards_interrupt_decoder also calls it)
	} while ( aui->card_infobits & AUINFOS_CARDINFOBIT_PLAYING );
	return 0;
#else
	} while ( space >= aui->card_bytespersign );
	return outbytes_left;
#endif
}

#if 0 /* not used by SBEMU */
static void aucards_dma_monitor(void)
/////////////////////////////////////
{
	struct mpxplay_audioout_info_s *aui = &au_infos;
	if(aui->card_infobits & AUINFOS_CARDINFOBIT_PLAYING)
		if(aui->card_handler->cardbuf_int_monitor)
			aui->card_handler->cardbuf_int_monitor(aui);
}

//---------------- Timer Interrupt ------------------------------------------
unsigned long intdec_timer_counter;

#endif

