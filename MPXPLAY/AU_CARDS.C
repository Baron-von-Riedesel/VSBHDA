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

extern uint8_t bOMode;

#ifndef NOES1371
extern struct sndcard_info_s ES1371_sndcard_info;
#endif
#ifndef NOICH
extern struct sndcard_info_s ICH_sndcard_info;
#endif
#ifndef NOHDA
extern struct sndcard_info_s HDA_sndcard_info;
#endif
#ifndef NOVIA82
extern struct sndcard_info_s VIA82XX_sndcard_info;
#endif
#ifndef NOSBLIVE
extern struct sndcard_info_s SBLIVE_sndcard_info;
#endif

static const struct sndcard_info_s *all_sndcard_info[] = {
#ifndef NOES1371
	&ES1371_sndcard_info,
#endif
#ifndef NOICH
	&ICH_sndcard_info,
#endif
#ifndef NOHDA
	&HDA_sndcard_info,
#endif
#ifndef NOSBLIVE
	&SBLIVE_sndcard_info,
#endif
#ifndef NOVIA82
	&VIA82XX_sndcard_info,
#endif
	NULL
};

/* scan for audio devices */

int FAREXP AU_init( const struct globalvars *gvars )
////////////////////////////////////////////////////
{
    struct audioout_info_s *aui;
	int i;

	dbgprintf(("AU_init\n"));
	if ( !( aui = (struct audioout_info_s *)calloc( 1, sizeof( struct audioout_info_s ) ) ) ) {
		dbgprintf(("AU_init: out of memory\n"));
		return(0);
	}
    /* 65535=maxbufsize, 4608=pagesize? 2=samplesize */
    //aui->card_dmasize = MDma_get_max_pcmoutbufsize( aui, 0x10000-1, 0x1200, 2, 0);
    /* v1.7: global settings can be accessed directly - card_select_ variables removed */
	//aui->card_select_devicenum = gvars->device;
	//aui->card_select_config = gvars->pin;
	aui->gvars = gvars;

	for ( i = 0; all_sndcard_info[i]; i++ ) {
		aui->card_handler = all_sndcard_info[i];
		if( aui->card_handler->card_detect ) {
			dbgprintf(("AU_init: checking card %s\n", aui->card_handler->shortname));
			aui->card_wave_id = WAVEID_PCM_SLE; // integer pcm
			aui->bits_card = 16;
			aui->bytespersample_card = aui->bits_card/8;
			aui->card_irq = 0xff;
			aui->freq_card = 44100;
			aui->chan_card = 2;
			if ( aui->card_handler->card_detect(aui) ) {
				aui->freq_card = aui->chan_card = aui->bits_card = 0;
				dbgprintf(("AU_init: found card %s\n", aui->card_handler->shortname));
				return((int)aui);
			}
		}
	}

	dbgprintf(("AU_init: no card found\n"));
	free( aui );
	return(0);

}

int FAREXP AU_getirq( struct audioout_info_s *aui )
///////////////////////////////////////////////////
{
    return( aui->card_irq );
}

char * FAREXP AU_getshortname( struct audioout_info_s *aui )
////////////////////////////////////////////////////////////
{
    return( aui->card_handler->shortname );
}

int FAREXP AU_getfreq( struct audioout_info_s *aui )
////////////////////////////////////////////////////
{
    return( aui->freq_card );
}

int FAREXP AU_isirq( struct audioout_info_s *aui )
//////////////////////////////////////////////////
{
    /* check if the irq belongs to the sound card */
    return( aui->card_handler->irq_routine && aui->card_handler->irq_routine(aui));
}

void FAREXP AU_setoutbytes( struct audioout_info_s *aui )
/////////////////////////////////////////////////////////
{
    aui->card_outbytes = aui->card_dmasize;
    return;
}

#if 0
void AU_setsamplenum( struct audioout_info_s *aui, int samples )
////////////////////////////////////////////////////////////////
{
    aui->samplenum = samples ;
    return;
}
#endif

#if 0
void FAREXP AU_prestart( struct audioout_info_s *aui )
//////////////////////////////////////////////////////
{
	if(aui->card_controlbits & AUINFOS_CARDCTRLBIT_DMACLEAR)
		AU_clearbuffs(aui);
}
#endif

void FAREXP AU_start( struct audioout_info_s *aui )
///////////////////////////////////////////////////
{
	if( !(aui->card_infobits & AUINFOS_CARDINFOBIT_PLAYING ) ) {
		if( aui->card_controlbits & AUINFOS_CARDCTRLBIT_DMACLEAR )
			AU_clearbuffs(aui);
		if( aui->card_handler->card_start )
			aui->card_handler->card_start( aui );
		aui->card_infobits |= AUINFOS_CARDINFOBIT_PLAYING;
		aui->card_infobits |= AUINFOS_CARDINFOBIT_DMAFULL;
	}
	if ( bOMode & 1 ) bOMode = 2;  /* no DOS output anymore */
}

void FAREXP AU_stop( struct audioout_info_s *aui )
//////////////////////////////////////////////////
{
	if( aui->card_infobits & AUINFOS_CARDINFOBIT_PLAYING ) {

		aui->card_infobits &= ~AUINFOS_CARDINFOBIT_PLAYING;

		if(aui->card_handler && aui->card_handler->card_stop)
			aui->card_handler->card_stop( aui );
		aui->card_dmafilled = aui->card_dmalastput;
		aui->card_dmaspace = aui->card_dmasize - aui->card_dmafilled;
		aui->card_infobits &= ~AUINFOS_CARDINFOBIT_DMAUNDERRUN;
	}
}

void FAREXP AU_close( struct audioout_info_s *aui )
///////////////////////////////////////////////////
{
	if(!aui)
		return;
	AU_stop(aui);
	if(aui->card_handler && aui->card_handler->card_close)
		aui->card_handler->card_close(aui);
}

static void AU_clearbuffs( struct audioout_info_s *aui )
////////////////////////////////////////////////////////
{
	if(aui->card_handler->cardbuf_clear)
		aui->card_handler->cardbuf_clear(aui);
	aui->card_controlbits &= ~AUINFOS_CARDCTRLBIT_DMACLEAR;
}

/* AU_setrate() is called by main(), not during interrupt time! */

int FAREXP AU_setrate( struct audioout_info_s *aui, int freq, int outchannels, int bits )
/////////////////////////////////////////////////////////////////////////////////////////

{
	unsigned int new_cardcontrolbits;
	dbgprintf(("AU_setrate(freq=%u, chan=%u, bits=%u) enter\n", freq, outchannels, bits ));

	new_cardcontrolbits = aui->card_controlbits;

	// Reconfigure the card.
	// The channel and bit differences are always handled by AU_MIXER

	if( 1 ) {

		if ( aui->card_infobits & AUINFOS_CARDINFOBIT_PLAYING )
			AU_stop(aui);

		dbgprintf(("AU_setrate: changing rate to %u\n", freq ));

		aui->freq_card = aui->freq_set = freq; /* may be modified below by card_setrate() */
		aui->chan_card = aui->chan_set = outchannels;
		aui->bits_card = aui->bits_set = bits;
		aui->card_wave_id = WAVEID_PCM_SLE; // integer pcm
		aui->bytespersample_card = 0;
		aui->card_controlbits = new_cardcontrolbits;

		if( aui->card_handler->card_setrate )
			aui->card_handler->card_setrate(aui);

		if(aui->card_wave_id == WAVEID_PCM_FLOAT)
			aui->bytespersample_card = 4;
		else
			if(!aui->bytespersample_card) // card haven't set it (not implemented in the au_mixer yet!: bits/8 !=bytespersample_card)
				aui->bytespersample_card = (aui->bits_card + 7) / 8;

		aui->card_controlbits |= AUINFOS_CARDCTRLBIT_DMACLEAR;
		//aui->card_controlbits &= ~AUINFOS_CARDCTRLBIT_UPDATEFREQ;

		aui->card_bytespersign = aui->chan_card * aui->bytespersample_card;
#ifdef SBEMU
		aui->card_outbytes = aui->card_dmasize;
#else
		aui->card_outbytes = aui->card_dmasize/4; // ??? for interrupt_decoder
#endif
	}
	return( aui->freq_card );
}

void FAREXP AU_setmixer_init( struct audioout_info_s *aui )
///////////////////////////////////////////////////////////
{
	unsigned int i;

	aui->card_master_volume = -1;

	for( i = 0; i < AU_MIXCHANS_NUM; i++ )
		aui->card_mixer_values[i] = -1;
}

static const struct aucards_mixerchan_s *AU_search_mixerchan( const struct aucards_mixerchan_s *mixeri[], unsigned int mixchannum )
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int i = 0;
	while(*mixeri){
		if((*mixeri)->mixchan == mixchannum)
			return (*mixeri);
		if(++i >= AU_MIXCHANS_NUM)
			break;
		mixeri++;
	}
	return NULL;
}

/* set mixer volume for "mixchannum" (0-10):
 * MASTER, PCM, HEADPHONE, SPDIF, SYNTH, MICIN, ...
 * defined by SC_xxx mixerset...
 */

void FAREXP AU_setmixer_one( struct audioout_info_s *aui, unsigned int mixchannum, unsigned int setmode, int newvalue )
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	const struct sndcard_info_s *cardi;
	const struct aucards_mixerchan_s *onechi; // one mixer channel infos (master,pcm,etc.)
	unsigned int subchannelnum, sch, channel, function;
	long newpercentval, maxpercentval;

	dbgprintf(("AU_setmixer_one( %u, %u, %u )\n", mixchannum, setmode, newvalue ));
	//mixer structure/values verifying

	/* there are 2 funcs: 0="set volume" or 1="set mute" */
	function = AU_MIXCHANFUNCS_GETFUNC(mixchannum);
	if( function >= AU_MIXCHANFUNCS_NUM )
		return;
	channel = AU_MIXCHANFUNCS_GETCHAN(mixchannum);
	if(channel > AU_MIXCHANS_NUM)
		return;
	cardi = aui->card_handler;
	if(!cardi)
		return;
	if(!cardi->card_writemixer || !cardi->card_readmixer || !cardi->card_mixerchans)
		return;
	onechi = AU_search_mixerchan( cardi->card_mixerchans, mixchannum );
	if(!onechi)
		return;
	subchannelnum = onechi->subchannelnum;
	if( !subchannelnum || (subchannelnum > AU_MIXERCHAN_MAX_SUBCHANNELS) )
		return;

	maxpercentval = AU_MIXCHAN_MAX_VALUE_VOLUME;

	//calculate new percent
	switch( setmode ) {
	case MIXER_SETMODE_ABSOLUTE:
		dbgprintf(("AU_setmixer_one( SETMODE_ABSOLUTE, %u %% )\n", newvalue ));
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
	if( newpercentval < 0 )
		newpercentval = 0;

	if(newpercentval > maxpercentval)
		newpercentval = maxpercentval;

	//read current register value, mix it with the new one, write it back
	for( sch = 0; sch < subchannelnum; sch++ ){
		const struct aucards_submixerchan_s *subchi = &(onechi->submixerchans[sch]); // one subchannel infos (left,right,etc.)
		unsigned long currchval,newchval;

		/* vsbhda: if card handles mixer setting alone don't check subchannel settings */
		if( !( subchi->submixch_infobits & SUBMIXCH_INFOBIT_CARD_SETVOL ) ) {
			/* invalid subchannel infos? */
			if((subchi->submixch_register > AU_MIXERCHAN_MAX_REGISTER) || !subchi->submixch_max || (subchi->submixch_shift > AU_MIXERCHAN_MAX_BITS)) {
				dbgprintf(("AU_setmixer_one: setting mixer ignored due to invalid subchannel info (reg=%u, max=%u, shift=%u)\n",
						   subchi->submixch_register, subchi->submixch_max, subchi->submixch_shift ));
				continue;
			}
		}

		/* vsbhda: if SUBMIXCH_INFOBIT_CARD_SETVOL==1, let the card handle the volume.
		 * Thus the values submixch_max and submixch_shift aren't used.
		 */
		if ( subchi->submixch_infobits & SUBMIXCH_INFOBIT_CARD_SETVOL ) {
			dbgprintf(("AU_setmixer_one: calling card_writemixer(%u)\n", newpercentval ));
			cardi->card_writemixer( aui, subchi->submixch_register, newpercentval );
		} else {
			currchval = cardi->card_readmixer( aui, subchi->submixch_register);// read current value
			dbgprintf(("AU_setmixer_one: called card_readmixer(%X)=%X (max=%X, shift=%X)\n", subchi->submixch_register, currchval, subchi->submixch_max, subchi->submixch_shift ));

			/* vsbhda: don't use floats here - function may be called during interrupt time */
			//newchval=(long)(((float)newpercentval * (float)subchi->submixch_max + ((float)((maxpercentval >> 1) - 1)))/(float)maxpercentval);   // percent to chval (rounding up)
			newchval = (((int64_t)newpercentval * subchi->submixch_max + (((maxpercentval >> 1) - 1))) / maxpercentval);   // percent to chval (rounding up)
			if( newchval > subchi->submixch_max)
				newchval = subchi->submixch_max;
			if(subchi->submixch_infobits & SUBMIXCH_INFOBIT_REVERSEDVALUE)   // reverse value if required
				newchval = subchi->submixch_max - newchval;

			newchval <<= subchi->submixch_shift;                             // shift to position

			currchval &= ~(subchi->submixch_max << subchi->submixch_shift);  // unmask
			newchval = (currchval | newchval);                               // add new value

			dbgprintf(("AU_setmixer_one: calling card_writemixer(%X, %X)\n", subchi->submixch_register, newchval ));
			cardi->card_writemixer( aui, subchi->submixch_register, newchval);  // write it back
		}
	}

	if( function == AU_MIXCHANFUNC_VOLUME )
		aui->card_mixer_values[channel] = newpercentval;
}

/*
 * called by AU_setmixer_all()
 * describe what's done here!
 */

static int AU_getmixer_one( struct audioout_info_s *aui, unsigned int mixchannum )
//////////////////////////////////////////////////////////////////////////////////
{
	const struct sndcard_info_s *cardi;
	const struct aucards_mixerchan_s *onechi;    // one mixer channel infos (master,pcm,etc.)
	const struct aucards_submixerchan_s *subchi; // one subchannel infos (left,right,etc.)
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
	subchannelnum = onechi->subchannelnum;
	if(!subchannelnum || (subchannelnum > AU_MIXERCHAN_MAX_SUBCHANNELS))
		return -1;

	maxpercentval = AU_MIXCHAN_MAX_VALUE_VOLUME;

	// we read one (the left at stereo) sub-channel only
	subchi = &(onechi->submixerchans[0]);
	if((subchi->submixch_register > AU_MIXERCHAN_MAX_REGISTER) || (subchi->submixch_shift > AU_MIXERCHAN_MAX_BITS)) { // invalid subchannel infos
		dbgprintf(("AU_getmixer_one(channel=%u, func=%u): don't like subchannel info (%u %u)!\n", channel, function, subchi->submixch_register, subchi->submixch_shift ));
		return -1;
	}

	value = cardi->card_readmixer( aui, subchi->submixch_register );

	/* vsbhda: if SUBMIXCH_INFOBIT_CARD_SETVOL==1, just return the plain value -
	 * it's a volume percentage already. Thus it's ensured thatthe values in submixch_shift/max
     * are never used.
	 */
	if ( !( subchi->submixch_infobits & SUBMIXCH_INFOBIT_CARD_SETVOL ) ) {
		value >>= subchi->submixch_shift;                             // shift
		value &= subchi->submixch_max;                                // mask

		if(subchi->submixch_infobits & SUBMIXCH_INFOBIT_REVERSEDVALUE)// reverse value if required
			value = subchi->submixch_max - value;

		/* vsbhda: no float usage here! */
		//value=(long)((float)value * (float)maxpercentval / (float)subchi->submixch_max); // chval to percent
		value = value * maxpercentval / subchi->submixch_max; // chval to percent
		if( value > maxpercentval )
			value = maxpercentval;
	}
	dbgprintf(("AU_getmixer_one(channel=%u, func=%u)=%d)\n", channel, function, value ));
	return value;
}

#define AU_MIXCHANS_OUTS 4

/* for AC97, there's a fifth out channel, AU_MIXCHAN_SYNTH */

static const unsigned int au_mixchan_outs[AU_MIXCHANS_OUTS] = {
	AU_MIXCHAN_MASTER, AU_MIXCHAN_PCM, AU_MIXCHAN_HEADPHONE, AU_MIXCHAN_SPDIFOUT };

/* set the volumes of "output channels:", defined in array above.
 */

void FAREXP AU_setmixer_outs( struct audioout_info_s *aui, unsigned int setmode, int newvalue )
///////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int i;

	for( i = 0; i < AU_MIXCHANS_OUTS; i++ )
		AU_setmixer_one( aui, AU_MIXCHANFUNCS_PACK(au_mixchan_outs[i], AU_MIXCHANFUNC_VOLUME ), setmode, newvalue );

	aui->card_master_volume = aui->card_mixer_values[AU_MIXCHAN_MASTER];
}

/* get/set the volumes of the output "channels"???? */

void FAREXP AU_setmixer_all( struct audioout_info_s *aui )
//////////////////////////////////////////////////////////
{
	unsigned int i;
	int vol = aui->card_master_volume;

	dbgprintf(("AU_setmixer_all: vol=%d)\n", vol ));
	/* set all output channels that haven't been set yet to the master volume */
	if( vol >= 0 )
		for( i = 0; i < AU_MIXCHANS_OUTS; i++)
			if( aui->card_mixer_values[au_mixchan_outs[i]] < 0 )
				aui->card_mixer_values[au_mixchan_outs[i]] = vol;

	for( i = 0; i < AU_MIXCHANS_NUM; i++ ){
		vol = aui->card_mixer_values[i];
		if( vol >= 0 ){
			AU_setmixer_one(aui,AU_MIXCHANFUNCS_PACK( i, AU_MIXCHANFUNC_VOLUME ), MIXER_SETMODE_ABSOLUTE, vol );
		} else {
			vol = AU_getmixer_one(aui,AU_MIXCHANFUNCS_PACK(i,AU_MIXCHANFUNC_VOLUME));
			if( vol >= 0 )
				aui->card_mixer_values[i] = vol;
		}
	}
}

#define SOUNDCARD_BUFFER_PROTECTION 32 // in bytes (required for PCI cards)

/* this function is static in mpxplay;
 * calculates and returns aui->card_dmaspace - in "sample"-units!
 */

unsigned int FAREXP AU_cardbuf_space( struct audioout_info_s *aui )
///////////////////////////////////////////////////////////////////
{
	unsigned long buffer_protection;

	/* this function is called from sound ISR!
	 * modifies:
	 * - card_dmalastput
	 * - card_dmaspace
	 * - card_dmafilled
	 * - card_infobits
	 * it's also the only function that calls cardbuf_getpos(),
     * that sets and returns card_dma_lastgoodpos.
	 */

	buffer_protection = SOUNDCARD_BUFFER_PROTECTION;     // rounding to bytespersign
	buffer_protection += aui->card_bytespersign - 1;
	buffer_protection -= (buffer_protection % aui->card_bytespersign);

	if( aui->card_dmalastput >= aui->card_dmasize ) // checking
		aui->card_dmalastput = 0;

	if( aui->card_handler->cardbuf_getpos ) {
		if( aui->card_handler->infobits & SNDCARD_CARDBUF_SPACE ) {
			if( aui->card_infobits & AUINFOS_CARDINFOBIT_PLAYING ) {
				aui->card_dmaspace = aui->card_handler->cardbuf_getpos(aui);
				aui->card_dmaspace -= (aui->card_dmaspace % aui->card_bytespersign); // round
			} else
				aui->card_dmaspace = (aui->card_dmaspace > aui->card_outbytes) ? (aui->card_dmaspace - aui->card_outbytes) : 0;
		} else {
			unsigned long bufpos;

			if( aui->card_infobits & AUINFOS_CARDINFOBIT_PLAYING ) {
				bufpos = aui->card_handler->cardbuf_getpos(aui);
				if( bufpos >= aui->card_dmasize )  // checking
					bufpos = 0;
				else
					bufpos -= (bufpos % aui->card_bytespersign); // round

				if( aui->card_infobits & AUINFOS_CARDINFOBIT_DMAUNDERRUN ){   // sets a new put-pointer in this case
					if( bufpos >= aui->card_outbytes )
						aui->card_dmalastput = bufpos - aui->card_outbytes;
					else
						aui->card_dmalastput = aui->card_dmasize + bufpos - aui->card_outbytes;
					aui->card_infobits &= ~AUINFOS_CARDINFOBIT_DMAUNDERRUN;
				}
			} else {
				bufpos = 0;
			}

			//if(aui->card_dmalastput >= aui->card_dmasize) // checking
			// aui->card_dmalastput = 0;

			if(bufpos > aui->card_dmalastput)
				aui->card_dmaspace = bufpos - aui->card_dmalastput;
			else
				aui->card_dmaspace = aui->card_dmasize - aui->card_dmalastput + bufpos;
		}
	} else {
		aui->card_dmaspace = aui->card_outbytes + buffer_protection;
		aui->card_infobits |= AUINFOS_CARDINFOBIT_DMAFULL;
	}

	if(aui->card_dmaspace > aui->card_dmasize) // checking
		aui->card_dmaspace = aui->card_dmasize;

	aui->card_dmafilled = aui->card_dmasize - aui->card_dmaspace;

	return (aui->card_dmaspace > buffer_protection) ? (aui->card_dmaspace - buffer_protection) : 0;
}

/* function called by AU_writedata() - during interrupt time! */

static int aucards_writedata_intsound( struct audioout_info_s *aui, unsigned long outbytes_left )
/////////////////////////////////////////////////////////////////////////////////////////////////
{
	char *pcm_outdata = (char *)aui->pcm_sample;
	unsigned long buffer_protection,space;

	/* example for bytes/sign = 4: 32 + 3 - 3 */
	buffer_protection = SOUNDCARD_BUFFER_PROTECTION;
	buffer_protection += aui->card_bytespersign - 1;
	buffer_protection -= (buffer_protection % aui->card_bytespersign);

	space = (aui->card_dmaspace > buffer_protection) ? (aui->card_dmaspace - buffer_protection) : 0;

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

int FAREXP AU_writedata( struct audioout_info_s *aui, int samples, void *pcm_sample )
/////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int outbytes_left;
	int left;

	/* this function is called during interrupt time!
     * samples are copied to the sound card's buffer.
	 */

	if( !samples )
		return 0;

	aui->samplenum = samples;
	aui->pcm_sample = pcm_sample;
	aui->samplenum -= (aui->samplenum % aui->chan_card); // if samplenum is buggy (round to chan_card)
	outbytes_left = aui->samplenum * aui->bytespersample_card;

#ifdef SBEMU
	aui->card_outbytes = min(outbytes_left,(aui->card_dmasize));
#else
	aui->card_outbytes = min(outbytes_left,(aui->card_dmasize/4));
#endif

	aui->card_outbytes -= (aui->card_outbytes % aui->card_bytespersign);

	left = aucards_writedata_intsound( aui, outbytes_left );

	aui->samplenum = 0;

	return left / aui->bytespersample_card; /* return value is ignored! */
}

#if 0
static int aucards_writedata_normal( struct audioout_info_s *aui, unsigned long outbytes_left )
///////////////////////////////////////////////////////////////////////////////////////////////
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
			if(aui->card_controlbits & AUINFOS_CARDCTRLBIT_DMADONTWAIT){
				aui->card_controlbits &= ~AUINFOS_CARDCTRLBIT_DMADONTWAIT;
				return aui->card_outbytes;
			}
		}
		if( space >= aui->card_bytespersign ){
			unsigned int outbytes_putblock = min( space, outbytes_left);

			aui->card_handler->cardbuf_writedata(aui,pcm_outdata,outbytes_putblock);
			pcm_outdata += outbytes_putblock;
			outbytes_left -= outbytes_putblock;

			aui->card_dmafilled += outbytes_putblock; // dma monitor needs this
		}
		if(!outbytes_left)
			break;
	} while(1);
	return 0;
}
#endif

