
/* test cv_rate() variants (contained in sndisr.c) */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <ctype.h>
#include <sys\stat.h>

#define MAXCHUNK 512

#define V20        1 /* v2.0 variant */
#define V17        0 /* v1.7 variant */
#define ALTERNATE  0 /* alternate variant */

static int freq_in = 11025;
static int freq_out = 44100;
static int chunksize = 16;
static int bits = 8;
static char *infile = NULL;
static char *outfile = NULL;

short inbuff[MAXCHUNK+1];
short outbuff[32768];

static void cv_bits_8_to_16( short *pcm, unsigned int nSamples )
////////////////////////////////////////////////////////////////
{
    unsigned char *srcu = (unsigned char *)pcm + nSamples - 1;
    short *dst = pcm + nSamples - 1;

    for ( ; nSamples; nSamples--, srcu--, dst-- ) {
        *dst = (short)((*srcu ^ 0x80) << 8);
    }
}

#if V20

/* the v2.0 variant */

static unsigned int cv_rate( short *pcmout, short *pcmsrc, unsigned int nSamples, unsigned int channels, unsigned int srcrate, unsigned int dstrate)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    //const unsigned int instep = ((srcrate / dstrate) << 12) | (((4096 * (srcrate % dstrate) + dstrate - 1 ) / dstrate) & 0xFFF);
    const unsigned int instep = ((srcrate / dstrate) << 12) | ((((srcrate % dstrate) << 12 ) / dstrate + 1 ) & 0xFFF);

    //const unsigned int inend = (nSamples / channels) << 12;
    const unsigned int inend = (nSamples >> (channels - 1 )) << 12;
    unsigned int inpos;//(srcrate < dstrate) ? instep / 2 : 0;
    unsigned int idx;
    short *pcmdst = pcmout;
    //printf("cv_rate( samples=%u, channels=%u, src/dst rates=%u/%u): instep=%x inend=%x\n",
    //       nSamples, channels, srcrate, dstrate, instep, inend );

    for ( inpos = 0; inpos < inend; inpos += instep ) {

        unsigned int m1,m2;
        short *intmp1,*intmp2;

        idx = (inpos >> 12 ) << ( channels - 1);
        m2 = inpos & 0xFFF;
        m1 = 4096 - m2;
        intmp1 = pcmsrc + idx;
        intmp2 = pcmsrc + idx + channels;
        *pcmdst++ = ( *intmp1 * m1 + *intmp2 * m2) >> 12;
        if ( channels > 1 )
            *pcmdst++ = ( *(intmp1+1) * m1 + *(intmp2+1) * m2) >> 12;
    }
#ifdef _DEBUG
    printf("cv_rate: dst samples=%u idx=%u\n", pcmdst - pcmout, idx );
#endif
    return pcmdst - pcmout;
}

#elif V17

static unsigned int cv_rate( short *pcmout, short *pcmsrc, unsigned int samplenum, unsigned int channels, unsigned int srcrate, unsigned int dstrate)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	/* todo: what algorithm for instep is best? */
	//const unsigned int instep = ((srcrate / dstrate) << 12) | (((4096 * (srcrate % dstrate) - 1) / (dstrate - 1)) & 0xFFF);
	const unsigned int instep = ((srcrate / dstrate) << 12) | (((4096 * (srcrate % dstrate) + dstrate - 1 ) / dstrate) & 0xFFF);

	const unsigned int inend = (samplenum / channels) << 12;
	short *pcmdst;
	unsigned int ipi;
	unsigned int inpos = 0;//(srcrate < dstrate) ? instep/2 : 0;
	int total;
	short* buff = pcmsrc;

	if(!samplenum)
		return 0;

	pcmdst = pcmout;
	total = samplenum / channels;

	do{
		int m1,m2;
		unsigned int ch;
		short *intmp1,*intmp2;
		ipi = inpos >> 12;
		m2 = inpos & 0xFFF;
		m1 = 4096 - m2;
		ch = channels;
		ipi *= ch;
		intmp1 = buff + ipi;
		intmp2 = ipi < total - ch ? intmp1 + ch : intmp1;
		do{
			*pcmdst++= ((*intmp1++) * m1 + (*intmp2++) * m2) / 4096;// >> 12; //don't use shift, signed right shift impl defined, maybe logical shift
		}while (--ch);
		inpos += instep;
	}while( inpos < inend );

#ifdef _DEBUG
    printf("cv_rate: dst samples=%u ipi=%u\n", pcmdst - pcmout, ipi );
#endif

	return pcmdst - pcmout;
}

#elif ALTERNATE

/* a variant that doesn't avoid divisions; probably slower, but more comprehensible;
 * requires srcrate <= dstrate.
 */

static unsigned int cv_rate( short *pcmout, short *pcmsrc, unsigned int nSamples, unsigned int channels, unsigned int srcrate, unsigned int dstrate)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
    int i;
    short *pcmdst;
    short *pcmend = pcmsrc + nSamples * channels;
    static int freqsum = 0;
    int addfreq = min( srcrate, dstrate );
    int maxfreq = max( srcrate, dstrate );
#ifdef _DEBUG
    short *orgsrc = pcmsrc;
#endif
# if 0
    addfreq += (maxfreq % addfreq) / ( 1 + (maxfreq / addfreq ) );
# endif
    for ( pcmdst = pcmout; pcmsrc < pcmend; ) {

        int diff;

        diff = *(pcmsrc + channels) - *pcmsrc;
        diff = ( diff * freqsum ) / maxfreq;
        *pcmdst++ = *pcmsrc + diff;

        if ( channels > 1 ) {
            diff = *(pcmsrc + channels+1) - *(pcmsrc+1);
            diff = ( diff * freqsum ) / maxfreq;
            *pcmdst++ = *(pcmsrc+1) + diff;
        }
        freqsum += addfreq;
        if ( freqsum >= maxfreq ) {
            freqsum -= maxfreq;
            pcmsrc += channels;
        }
    }

#ifdef _DEBUG
    printf("cv_rate: dst samples=%u src samples=%u\n", pcmdst - pcmout, pcmsrc - orgsrc );
#endif

    return pcmdst - pcmout;
}

#endif

int getnum(char *p) {
    int result = 0;
    for ( result = 0; *p; p++) {
        if (*p < '0' || *p > '9')
            break;
        result = result * 10 + (*p - '0');

    }
    return result;
}

void disphelp( void ) {
    printf("cvrate - resample PCM data to format 44100 Hz, 16-bit, mono\n");
    printf("usage: cvrate [options] infile outfile\n");
    printf("options:\n");
    printf(" -r<n> : rate of input PCM data (def 11025)\n");
    printf(" -b<n> : bits of input samples (def 8)\n");
    printf(" -c<n> : chunk size (def 16)\n");
}

int main(int argc, char *argv[])
{
    int i;
    char *p;
    short *inp;
    short *outp;
    char c;
    int insamples;
    FILE *file1;
    FILE *file2;

    for (i = 1; i < argc; i++ ) {
        p = argv[i];
        if (*p == '-' || *p == '/') {
            p++;
            c = tolower(*p);
            p++;
            switch (c) {
            case 'r':
                freq_in = getnum(p);
                break;
            case 'c':
                chunksize = getnum(p);
                if ( chunksize > MAXCHUNK ) {
                    printf("max chunk size is %u\n", MAXCHUNK);
                    return(1);
                }
                break;
            case 'b':
                bits = getnum(p);
                if ( bits != 8 && bits != 16 ) {
                    printf("bits must be 8 or 16\n");
                    return(1);
                }
                break;
            case '?':
                disphelp();
                return(0);
            default:
                disphelp();
                return(1);
            }
        } else if ( infile == NULL ) {
            infile = p;
        } else {
            outfile = p;
        }
    }
    if ( infile == NULL || outfile == NULL ) {
        disphelp();
        return(1);
    }
    if ( !(file1 = fopen(infile,"rb"))) {
        printf("file %s cannot be opened\n", infile);
        return(1);
    }
    if ( !(file2 = fopen(outfile,"wb"))) {
        printf("file %s cannot be opened\n", outfile);
        return(1);
    }
    while (1) {
        insamples = fread( inbuff, 1, MAXCHUNK, file1 );
        if ( bits == 8 )
            cv_bits_8_to_16( inbuff, insamples );

        /* ensure there's a "terminating next" byte */
        if (insamples)
            *(inbuff+insamples) = *(inbuff+insamples-1);

        for ( inp = inbuff, outp = outbuff; insamples; insamples -= min( insamples, chunksize) ) {
            outp += cv_rate( outp, inp, min(insamples,chunksize), 1, freq_in, freq_out );
            inp += min( insamples, chunksize );
        }
        if (!(outp - outbuff))
            break;
        fwrite( outbuff, 2, outp - outbuff, file2 );
    }

    fclose( file1 );
    fclose( file2 );

    return 0;
}

