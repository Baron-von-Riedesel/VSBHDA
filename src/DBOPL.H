/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

//Use 8 handlers based on a small logarithmic wavetabe and an exponential table for volume
#define WAVE_HANDLER	10
//Use a logarithmic wavetable with an exponential table for volume
#define WAVE_TABLELOG	11
//Use a linear wavetable with a multiply table for volume
#define WAVE_TABLEMUL	12

//Select the type of wave generator routine
//#define DBOPL_WAVE WAVE_TABLEMUL
#define DBOPL_WAVE WAVE_TABLEMUL

#include <inttypes.h>

typedef uintptr_t Bitu;
typedef intptr_t Bits;

namespace DBOPL {

struct Chip;
struct Operator;
struct Channel;

#if (DBOPL_WAVE == WAVE_HANDLER)
#ifndef DB_FASTCALL
#define DB_FASTCALL
#endif
typedef Bits ( DB_FASTCALL *WaveHandler) ( Bitu i, Bitu volume );
#endif

typedef Bits ( DBOPL::Operator::*VolumeHandler) ( );
typedef Channel* ( DBOPL::Channel::*SynthHandler) ( Chip* chip, uint32_t samples, int16_t* output );

//Different synth modes that can generate blocks of data
typedef enum {
	sm2AM,
	sm2FM,
	sm3AM,
	sm3FM,
	sm4Start,
	sm3FMFM,
	sm3AMFM,
	sm3FMAM,
	sm3AMAM,
	sm6Start,
	sm2Percussion,
	sm3Percussion,
} SynthMode;

//Shifts for the values contained in chandata variable
enum {
	SHIFT_KSLBASE = 16,
	SHIFT_KEYCODE = 24,
};

struct Operator {
public:
	//Masks for operator 20 values
	enum {
		MASK_KSR = 0x10,
		MASK_SUSTAIN = 0x20,
		MASK_VIBRATO = 0x40,
		MASK_TREMOLO = 0x80,
	};

	typedef enum {
		OFF,
		RELEASE,
		SUSTAIN,
		DECAY,
		ATTACK,
	} State;

	VolumeHandler volHandler;

#if (DBOPL_WAVE == WAVE_HANDLER)
	WaveHandler waveHandler;	//Routine that generate a wave 
#else
	int16_t* waveBase;
	uint32_t waveMask;
	uint32_t waveStart;
#endif
	uint32_t waveIndex;			//WAVE_BITS shifted counter of the frequency index
	uint32_t waveAdd;				//The base frequency without vibrato
	uint32_t waveCurrent;			//waveAdd + vibratao

	uint32_t chanData;			//Frequency/octave and derived data coming from whatever channel controls this
	uint32_t freqMul;				//Scale channel frequency with this, TODO maybe remove?
	uint32_t vibrato;				//Scaled up vibrato strength
	int32_t sustainLevel;		//When stopping at sustain level stop here
	int32_t totalLevel;			//totalLevel is added to every generated volume
	uint32_t currentLevel;		//totalLevel + tremolo
	int32_t volume;				//The currently active volume
	
	uint32_t attackAdd;			//Timers for the different states of the envelope
	uint32_t decayAdd;
	uint32_t releaseAdd;
	uint32_t rateIndex;			//Current position of the evenlope

	uint8_t rateZero;				//Bits for the different states of the envelope having no changes
	uint8_t keyOn;				//Bitmask of different values that can generate keyon
    uint8_t bState;
	//Registers, also used to check for changes
	uint8_t reg20, reg40, reg60, reg80, regE0;
	//Active part of the envelope we're in
	uint8_t state;
	//0xff when tremolo is enabled
	uint8_t tremoloMask;
	//Strength of the vibrato
	uint8_t vibStrength;
	//Keep track of the calculated KSR so we can check for changes
	uint8_t ksr;
private:
	void SetState( uint8_t s );
	void UpdateAttack( const Chip* chip );
	void UpdateRelease( const Chip* chip );
	void UpdateDecay( const Chip* chip );
public:
	void UpdateAttenuation();
	void UpdateRates( const Chip* chip );
	void UpdateFrequency( );

	void Write20( const Chip* chip, uint8_t val );
	void Write40( const Chip* chip, uint8_t val );
	void Write60( const Chip* chip, uint8_t val );
	void Write80( const Chip* chip, uint8_t val );
	void WriteE0( const Chip* chip, uint8_t val );

	bool Silent() const;
	void Prepare( const Chip* chip );

	void KeyOn( uint8_t mask);
	void KeyOff( uint8_t mask);

//	template< State state>
	Bits TemplateVolume( );

	int32_t RateForward( uint32_t add );
	Bitu ForwardWave();
	Bitu ForwardVolume();

	Bits GetSample( Bits modulation );
	Bits GetWave( Bitu index, Bitu vol );
public:
	Operator();
};

struct Channel {
	Operator op[2]; //Leave on top of struct for simpler pointer math.
	inline Operator* Op( Bitu index ) {
		return &( ( this + (index >> 1) )->op[ index & 1 ]);
	}
	SynthHandler synthHandler;
	uint32_t chanData;		//Frequency/octave and derived values
	int32_t old[2];			//Old data for feedback

	uint8_t feedback;			//Feedback shift
	uint8_t regB0;			//Register values to check for changes
	uint8_t regC0;
	//This should correspond with reg104, bit 6 indicates a Percussion channel, bit 7 indicates a silent channel
	uint8_t fourMask;
	int8_t maskLeft;		//Sign extended values for both channel's panning
	int8_t maskRight;
    SynthMode mode;

	//Forward the channel data to the operators of the channel
	void SetChanData( const Chip* chip, uint32_t data );
	//Change in the chandata, check for new values and if we have to forward to operators
	void UpdateFrequency( const Chip* chip, uint8_t fourOp );
	void UpdateSynth(const Chip* chip);
	void WriteA0( const Chip* chip, uint8_t val );
	void WriteB0( const Chip* chip, uint8_t val );
	void WriteC0( const Chip* chip, uint8_t val );

	//call this for the first channel
	template< bool opl3Mode >
	void GeneratePercussion( Chip* chip, int16_t* output );

	//Generate blocks of data in specific modes
	//template<SynthMode mode>
	Channel* BlockTemplate( Chip* chip, uint32_t samples, int16_t* output );
	Channel();
};

struct Chip {
	//18 channels with 2 operators each. Leave on top of struct for simpler pointer math.
	Channel chan[18];

	//This is used as the base counter for vibrato and tremolo
	uint32_t lfoCounter;
	uint32_t lfoAdd;

	uint32_t noiseCounter;
	uint32_t noiseAdd;
	uint32_t noiseValue;

	//Frequency scales for the different multiplications
	uint32_t freqMul[16]; // = {};
	//Rates for decay and release for rate of this chip
    uint32_t linearRates[76]; //= {};
	//Best match attack rates for the rate of this chip
    uint32_t attackRates[76]; // = {};

	uint8_t reg104;
	uint8_t reg08;
	uint8_t reg04;
	uint8_t regBD;
	uint8_t vibratoIndex;
	uint8_t tremoloIndex;
	int8_t vibratoSign;
	uint8_t vibratoShift;
	uint8_t tremoloValue;
	uint8_t vibratoStrength;
	uint8_t tremoloStrength;
	//Mask for allowed wave forms
	uint8_t waveFormMask;
	//0 or -1 when enabled
	int8_t opl3Active;
	//Running in opl3 mode
	const bool opl3Mode;
	//Return the maximum amount of samples before and LFO change
	uint32_t ForwardLFO( uint32_t samples );
	uint32_t ForwardNoise();

	void WriteBD( uint8_t val );
	void WriteReg(uint32_t reg, uint8_t val );

	uint32_t WriteAddr( uint32_t port, uint8_t val );

	int GenerateBlock2( Bitu total, int16_t* output );
	int GenerateBlock3( Bitu total, int16_t* output );

	//Update the synth handlers in all channels
	void UpdateSynths();
	int Generate(int16_t* buffer, uint32_t samples);
	void Setup( uint32_t rate );

	Chip( bool opl3Mode );
};

}		//Namespace
