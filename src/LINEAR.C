
/* linear memory access */

#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <crt0.h>

#include "LINEAR.H"

int _crt0_startup_flags = _CRT0_FLAG_PRESERVE_FILENAME_CASE | _CRT0_FLAG_KEEP_QUOTES;

uint32_t DSBase = 0;
