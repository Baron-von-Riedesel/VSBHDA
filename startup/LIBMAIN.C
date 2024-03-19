
// for 16-bit only

#include <stdint.h>

uint32_t _linear_rmstack;
uint32_t DSBase;
uint8_t bOMode = 1; /* 1=DOS output, 2=low-level, 4=debugger */
uint8_t _8087;

int LibMain()
{
	return 1;
}
