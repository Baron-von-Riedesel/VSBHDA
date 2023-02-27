
#include "DPMI_.H"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/exceptn.h>
#include <go32.h>

#define dbgprintf

BOOL DPMI_TSR()
{
    __djgpp_exception_toggle();

    DPMI_REG r = {0};

    dbgprintf("Transfer buffer: %08lx, PSP: %08lx, transfer buffer size: %08lx\n", _go32_info_block.linear_address_of_transfer_buffer,
        _go32_info_block.linear_address_of_original_psp, _go32_info_block.size_of_transfer_buffer);
        
    r.w.dx = (uint16_t)((_go32_info_block.linear_address_of_transfer_buffer
        - _go32_info_block.linear_address_of_original_psp
        + _go32_info_block.size_of_transfer_buffer) >> 4);

    r.w.dx= 256>>4; //only psp
    dbgprintf("TSR size: %d\n", r.w.dx<<4);
    r.w.ax = 0x3100;
    return DPMI_CallRealModeINT(0x21, &r) == 0; //won't return on success
}


