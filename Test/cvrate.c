
/* test cv_rate() in sndisr.c */

#include <stdio.h>
#include <memory.h>
#include <sys\stat.h>

#define VARIANT1
#define DIFF 2048

short smplbuff[1024] = {0*DIFF,  1*DIFF,  2*DIFF,  3*DIFF,  4*DIFF,  5*DIFF,  6*DIFF,  7*DIFF,
                        8*DIFF,  9*DIFF, 10*DIFF, 11*DIFF, 12*DIFF, 13*DIFF, 14*DIFF, 15*DIFF,
                       14*DIFF, 13*DIFF, 12*DIFF, 11*DIFF, 10*DIFF,  9*DIFF,  8*DIFF,  7*DIFF,
                        6*DIFF,  5*DIFF,  4*DIFF,  3*DIFF,  2*DIFF,  1*DIFF,  0*DIFF, -1*DIFF,
                       -2*DIFF, -3*DIFF, -4*DIFF, -5*DIFF, -6*DIFF, -7*DIFF, -8*DIFF, -9*DIFF,
                      -10*DIFF,-11*DIFF,-12*DIFF,-13*DIFF,-14*DIFF,-15*DIFF,-14*DIFF,-13*DIFF,
                      -12*DIFF,-11*DIFF,-10*DIFF, -9*DIFF, -8*DIFF, -7*DIFF, -6*DIFF, -5*DIFF,
                       -4*DIFF, -3*DIFF, -2*DIFF, -1*DIFF };
;
short tmpbuff[1024];

//#define TOTAL

unsigned int cv_rate( short *pcmsrc, const unsigned int samplenum, const unsigned int channels, unsigned int srcrate, unsigned int dstrate)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    //const unsigned int instep = ((srcrate / dstrate) << 12) | (((4096 * (srcrate % dstrate) - 1) / (dstrate - 1)) & 0xFFF);
#ifdef VARIANT1
    const unsigned int instep = ((srcrate / dstrate) << 12) | (((4096 * (srcrate % dstrate) + dstrate - 1 ) / dstrate) & 0xFFF);
#else
    //const unsigned int instep = ((srcrate / dstrate) << 12) | (((4096 * (srcrate % dstrate) ) / dstrate + 1) & 0xFFF);
#endif

	//const unsigned int inend = (samplenum / channels) << 12;
	const unsigned int inend = (samplenum >> (channels - 1 )) << 12;
	unsigned int ipi;
	unsigned int inpos;//(srcrate < dstrate) ? instep / 2 : 0;
	int i;
#ifdef TOTAL
	unsigned int total;
#endif
	short *buff = tmpbuff;
	short *pcmdst = tmpbuff;
	memset( tmpbuff, 0, sizeof( tmpbuff) );

#ifdef TOTAL
	total = samplenum >> ( channels - 1);
	if ( total >= channels )
		total -= channels;
#endif

	printf("cv_rate(src/dst rates=%u/%u chn=%u smpl=%u): instep=%x inend=%x\n", srcrate, dstrate, channels, samplenum, instep, inend );
	for ( inpos = 0; inpos < inend; inpos += instep, pcmdst += channels ) {

		unsigned int m1,m2;
		unsigned int ipi;
		short *intmp1,*intmp2;

		ipi = (inpos >> 12 ) << ( channels - 1);
		m2 = inpos & 0xFFF;
		m1 = 4096 - m2;
		intmp1 = pcmsrc + ipi;
#ifdef TOTAL
		intmp2 = (ipi < total) ? intmp1 + channels : intmp1;
#else
		intmp2 = intmp1 + channels;
#endif
		//printf("ipi=%4u m1=%4u, m2=%4u, intmp1=%X, intmp2=%X (src=%X)\n", ipi, m1, m2, intmp1 - pcmsrc, intmp2 - pcmsrc, pcmsrc );
		*pcmdst = ( *intmp1 * m1 + *intmp2 * m2) >> 12;
		if ( channels > 1 )
			*(pcmdst+1) = ( *(intmp1+1) * m1 + *(intmp2+1) * m2) >> 12;
	}

	printf("cv_rate: samples=%u\n", (pcmdst - tmpbuff) >> (channels - 1) );
#if 1
	for ( i = 0; buff < pcmdst; buff += 8, i += 8 )
		printf("samples[%4u]: %6d %6d %6d %6d %6d %6d %6d %6d\n",
			i, *(buff+0), *(buff+1), *(buff+2), *(buff+3), *(buff+4), *(buff+5),*(buff+6),*(buff+7));
#endif
	return 1;
}

int main(int argc, const char * argv[])
{
	int i;
	for ( i = 0; i < 60; i += 8 )
		printf("%4u: %6d %6d %6d %6d %6d %6d %6d %6d\n",
			i, smplbuff[i],smplbuff[i+1],smplbuff[i+2],smplbuff[i+3],smplbuff[i+4],smplbuff[i+5],smplbuff[i+6],smplbuff[i+7] );

	//cv_rate( smplbuff, 15, 1, 11025, 44100 );
	//cv_rate( smplbuff, 16, 1, 11025, 44100 );
	//cv_rate( smplbuff, 17, 1, 11025, 44100 );
	cv_rate( smplbuff, 60, 1, 11025, 44100 );
	//cv_rate( smplbuff,128, 1, 11025, 44100 );
	cv_rate( smplbuff, 30, 2, 11025, 44100 );
	/* for ADPCM */
	cv_rate( smplbuff, 12, 1, 4000, 44100 );
	return 0;
}

