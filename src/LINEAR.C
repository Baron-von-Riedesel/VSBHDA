
/* linear memory access */

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "LINEAR.H"

#if defined(DJGPP) || defined(NOTFLAT)
uint32_t DSBase = 0;
#else
uint8_t _8087; /* define these so no fpu exc handling is linked in */
uint8_t _real87;
#endif

