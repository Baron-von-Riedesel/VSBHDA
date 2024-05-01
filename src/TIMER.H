
#define PIT_DIVISOR_DEFAULT 65536

#ifdef DJGPP

#define _disableint() asm("mov $0x900, %%ax \n\t" "int $0x31 \n\t" "mov %%ax, %0\n\t" : "=m"(oldstate) :: "eax" )
#define _restoreint() asm("mov %0, %%ax \n\t" "int $0x31 \n\t" :: "m"(oldstate) : "eax" )

static inline uint16_t gettimercnt(void)
{
    uint16_t tsc;
	_disableint();
	outp( 0x43, 0x04 );
	tsc = inp(0x40);
	tsc += inp(0x40) << 8;
	_restoreint();
    return tsc;
}

#else

static  uint16_t gettimercnt(void);
#pragma aux gettimercnt = \
    "mov ax, 0900h" \
    "int 31h"      \
    "push ax"      \
    "mov al, 4"    \
    "out 43h, al"  \
    "in al, 40h"   \
    "mov ah, al"   \
    "in al, 40h"   \
    "xchg al,ah"   \
    "mov dx, ax"   \
    "pop ax"       \
    "int 31h"      \
    "mov ax, dx"   \
    parm[]         \
    modify exact[ax dx]
#endif

void delay_10us(unsigned int ticks) //each tick is 10us
///////////////////////////////////
{
	unsigned int divisor = PIT_DIVISOR_DEFAULT; // is 65536
	unsigned int i;
	unsigned short oldtsc, tsctemp, tscdif;
	unsigned short oldstate;

	for( i = 0; i < ticks; i++ ){
		oldtsc = gettimercnt();
		do{
			tsctemp = gettimercnt();
			if(tsctemp <= oldtsc)
				tscdif = oldtsc - tsctemp; // handle overflow
			else
				tscdif = divisor + oldtsc - tsctemp;
		} while ( tscdif < 12); //wait for 10us  (12/(65536*18) sec)
	}
}

