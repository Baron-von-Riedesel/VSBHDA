#ifndef au_mixer_h
#define au_mixer_h

#include "MPXPLAY.H"

#ifdef __cplusplus
extern "C" {
#endif

typedef mpxp_int32_t  PCM_CV_TYPE_I;  // max. 32 bit input/output pcm format (integer)
typedef mpxp_uint32_t PCM_CV_TYPE_UI; //
typedef mpxp_float_t  PCM_CV_TYPE_F;  // 32-bit float part of mixer
typedef mpxp_int16_t  PCM_CV_TYPE_S;  // 16-bit integer part of mixer
typedef mpxp_int8_t   PCM_CV_TYPE_C;  // 8-bit signed
typedef mpxp_uint8_t  PCM_CV_TYPE_UC; // 8-bit unsigned
typedef PCM_CV_TYPE_F PCM_CV_TYPE_MAX;// largest datatype (currently)

#define MIXER_SCALE_BITS    16 // used bits in integer based (lq) functions and default scale bits in hq functions
#define MIXER_SCALE_VAL  65536
#define MIXER_SCALE_MIN -32768
#define MIXER_SCALE_MAX  32767

#define MIXER_MAX_FUNCTIONS 31 // built-in + dll
#define MIXER_BUILTIN_FUNCTIONS  13 // !!!
#define MIXER_BUILTIN_CFPREPROCESSNUM 2 // last is speed/freq control
#define MIXER_DLLFUNC_INSERTPOINT   3 // after crossfader

#define MIXER_SETMODE_RELATIVE 0
#define MIXER_SETMODE_ABSOLUTE 1
#define MIXER_SETMODE_RESET    2

// one_mixerfunc_info->infobits
#define MIXER_INFOBIT_RESETDONE            1 // mixer value is set to default (reset done)
#define MIXER_INFOBIT_ENABLED              2 // mixer function is enabled/used
#define MIXER_INFOBIT_BUSY                 4 // mixer function is under control/modification (to avoid short circuit (endless cycle) in the mixer)
#define MIXER_INFOBIT_SWITCH               8 // function variable a switch (flip-flop)(its value can be 0 or 1 only)
#define MIXER_INFOBIT_EXTERNAL_DEPENDENCY 16 // function has an external dependency (controlled by another/external variable too)(ie:freq conversion,swapchan,surround)
#define MIXER_INFOBIT_PARALLEL_DEPENDENCY 32 // dependant/enabled by another mixer function too

#define MPXPLAY_CFGFUNCNUM_AUMIXER_MAJOR 0x08000000
#define MPXPLAY_CFGFUNCNUM_AUMIXER_GET_CARD_CHAN 0x08000126 // ccb_data:void*mpi argp1:mpxp_uint32_t*ret_value
#define MPXPLAY_AUMIXER_MUTE_SWITCH_SIGN 65535  // on/off switch instead of 'push' (ctrl-'M')

struct mpxp_aumixer_main_info_s;

typedef struct mpxp_aumixer_passinfo_s{
 // fixed structure -----------
 mpxp_int32_t (*control_cb)(void *ccb_data,mpxp_uint32_t cfgnum,void *argp1,void *argp2);
 void *ccb_data;
 void *private_data;
 short *pcm_sample;
 unsigned int samplenum;
 // ---------------------------
 // only built-in functions can use these variables (external/dll must use mpi->control_cb to get informations)
 unsigned int mixer_function_flags; // enabled functions
 int  bytespersample_mixer; // in
 int  freq_song;
 int  freq_card;
 int  chan_song;
 int  chan_card;
 int  bits_song;
 int  bits_card;
 int  pcmout_savedsamples;
 struct mpxp_aumixer_main_info_s *mmi;
 struct mpxplay_audioout_info_s *aui;
 //struct mpxpframe_s *frp;
 void *mutex_aumixer;
 void *mixerfuncs_private_datas[MIXER_MAX_FUNCTIONS];
}mpxp_aumixer_passinfo_s;

typedef struct mpxp_aumixer_main_info_s{
 short *pcm16;                  // at mixer end
 short *pcm_sample;             // at mixer end
 unsigned long samplenum;       // at mixer end
 PCM_CV_TYPE_S *MIXER_int16pcm_buffer;
 unsigned long MIXER_int16pcm_bufsize;
 unsigned long mixer_infobits;  // MIXERINFOBIT_
}mpxp_aumixer_main_info_s;

typedef struct one_mixerfunc_info{
 char *name;
 char *cmdlineopt;
 int  *variablep;
 int  infobits;
 int  var_min;     // minimum value (must be set if variable is not a SWITCH)
 int  var_max;     // maximum value (must be set if variable is not a SWITCH)
 int  var_center;  // center (zero) value (must be set, if center value is not 0)
 int  var_step;    // step
 int  (*function_init)(struct mpxp_aumixer_passinfo_s *,int inittype); // see inittype flags
 long (*function_config)(struct mpxp_aumixer_passinfo_s *,unsigned long cfgnum,void *argp1,void *argp2); // pre-config without argument
 void (*process_routine)(struct mpxp_aumixer_passinfo_s *); // float (32 bit) based audio modification/transformation
 int  (*own_checkvar_routine)(struct mpxp_aumixer_passinfo_s *);  // if(*variablep!=var_center && check_var())
 void (*own_setvar_routine)(struct mpxp_aumixer_passinfo_s *,unsigned int setmode,int value);
}one_mixerfunc_info;

extern mpxp_int32_t mpxplay_aumixer_control_cb(void *cb_data,mpxp_uint32_t funcnum,void *argp1,void *argp2);

#ifdef __cplusplus
}
#endif

#endif
