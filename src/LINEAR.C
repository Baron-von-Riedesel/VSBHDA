
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

uint32_t DSBase = 0;
#endif

