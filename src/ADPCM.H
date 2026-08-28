
/* ADPCM decoding */

#if ADPCM

struct ADPCM_STATE {
    int scale;
    uint8_t ref;
    uint8_t useRef;
};

extern struct ADPCM_STATE adpcm_state;

extern int DecodeADPCM( uint8_t *pcm, int count );

#endif

