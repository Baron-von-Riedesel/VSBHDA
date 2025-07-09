
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "CONFIG.H"
#include "LINEAR.H"
#include "VMPU.H"
#include "PLATFORM.H"

#if SOUNDFONT

static void* tsfimpl_malloc(size_t size)
{
    __dpmi_meminfo info;

    info.address = 0;
    info.size = (size + sizeof(size_t) + 4095) & ~4095;

    if (__dpmi_allocate_linear_memory( &info, 1 ) == -1 ) {
        dbgprintf(("tsfimpl_malloc(%X) failed\n", size ));
        return NULL;
    }

    dbgprintf(("tsfimpl_malloc(%X)=%X (hdl=%X)\n", size, info.address, info.handle ));
    (*(size_t*)NearPtr(info.address)) = info.handle;
    return NearPtr((unsigned int)(((uint8_t*)info.address) + sizeof(size_t) ));
}

static void tsfimpl_free(void* ptr)
{
    size_t handle;
    if (ptr == NULL)
        return;
    handle = *((size_t*)(ptr) - 1);
    __dpmi_free_memory(handle);
    //dbgprintf(("tsfimpl_free(%X)\n", handle ));
}

static void* tsfimpl_realloc(void *ptr, size_t size)
{
    __dpmi_meminfo info;
    if (ptr == 0)
        return tsfimpl_malloc(size);

    info.handle = *((size_t*)(ptr) - 1);
    info.size   = (size + sizeof(size_t) + 4095) & ~4095;

    if (__dpmi_resize_linear_memory( &info, 1 ) == -1 ) {
        dbgprintf(("tsfimpl_realloc(%X, %X) failed\n", ptr, size ));
        return NULL;
    }

    dbgprintf(("tsfimpl_realloc(%X)=%X (hdl=%X)\n", size, info.address, info.handle ));
    (*(size_t*)NearPtr(info.address)) = info.handle;
    return NearPtr((unsigned int)(((uint8_t*)info.address) + sizeof(size_t)));
}

#define TSF_MALLOC tsfimpl_malloc
#define TSF_FREE tsfimpl_free
#define TSF_REALLOC tsfimpl_realloc

#ifndef DJGPP
#define powf (float)pow
#define expf (float)exp
#define sqrtf (float)sqrt
#endif

/* don't use the stdio buffered file system */
void *fopen( const char *, void *);
void *fclose( void *);
void *fseek( void *, int, int);
void *fread( void *, int, int, void *);
#define SEEK_CUR 1

#define TSF_NO_STDIO

#define TSF_IMPLEMENTATION
#include "../tsf/TSF.H"

#endif
