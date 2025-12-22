
// for 16-bit only

#include <stdint.h>

enum {
    OM_DOS = 1,
    OM_DIRECT = 2,
    OM_DEBUGGER = 4,
};

uint32_t _linear_rmstack;
uint32_t DSBase;
uint8_t _8087;
uint8_t bOMode = OM_DOS;

int IsDebuggerPresent( void );

int LibMain()
{
	if ( IsDebuggerPresent() )
		bOMode = OM_DEBUGGER;
	return 1;
}
