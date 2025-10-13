
// for 16-bit only

#include <stdint.h>

uint32_t _linear_rmstack;
uint32_t DSBase;
uint8_t _8087;
uint8_t bOMode = 1;

int IsDebuggerPresent( void );

int LibMain()
{
	if ( IsDebuggerPresent() )
		bOMode = 4;
	return 1;
}
