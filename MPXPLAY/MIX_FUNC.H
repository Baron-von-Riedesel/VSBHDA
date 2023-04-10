#ifndef mix_func_h
#define mix_func_h

#include "AU_MIXER.H"

#ifdef __cplusplus
extern "C" {
#endif

//cv_bits.c
extern void cv_bits_n_to_m(PCM_CV_TYPE_S *pcm,unsigned int samplenum,unsigned int in_bytespersample,unsigned int out_bytespersample);
//extern void aumixer_cvbits_float64le_to_float32le(PCM_CV_TYPE_S *pcm,unsigned int samplenum);

//cv_freq.c
extern unsigned int mixer_speed_lq(PCM_CV_TYPE_S *pcm16, unsigned int samplenum, unsigned int channels, unsigned int samplerate, unsigned int newrate);

//cv_chan.c
extern unsigned int cv_channels_1_to_n(PCM_CV_TYPE_S *pcm_sample,unsigned int samplenum,unsigned int newchannels,unsigned int bytespersample);
extern unsigned int cv_channels_n_to_1(PCM_CV_TYPE_S *pcm_sample,unsigned int samplenum,unsigned int oldchannels,unsigned int bytespersample);
extern unsigned int cv_channels_n_to_2(PCM_CV_TYPE_S *pcm_sample,unsigned int samplenum,unsigned int oldchannels,unsigned int bytespersample);

#ifdef __cplusplus
}
#endif

#endif
