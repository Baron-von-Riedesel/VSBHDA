
// for 16-bit only

#include <stdint.h>

uint32_t _linear_rmstack = 0;

uint8_t bOMode = 1; /* 1=DOS output, 2=low-level, 4=debugger */

int LibMain()
{
	return 1;
}
