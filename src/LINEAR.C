
/* linear memory access */

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#ifdef DJGPP
#include <crt0.h>
#endif

#include "LINEAR.H"

#ifdef DJGPP
int _crt0_startup_flags = _CRT0_FLAG_PRESERVE_FILENAME_CASE | _CRT0_FLAG_KEEP_QUOTES | _CRT0_FLAG_NEARPTR;
#endif

#if defined(DJGPP) || defined(NOTFLAT)
uint32_t DSBase = 0;
#else
uint8_t _8087; /* define these so no fpu exc handling is linked in */
uint8_t _real87;
#endif

