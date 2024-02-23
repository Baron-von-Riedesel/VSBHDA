//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2008 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: alloc/free/map/unmap physical memory (XMS)

#include <stdint.h>
#include <stdio.h>
#include <dpmi.h>
#include <sys/exceptn.h>

#include "CONFIG.H"
#include "NEWFUNC.H"

unsigned long _dpmi_map_physical_memory( unsigned long phys_addr, unsigned long memsize)
////////////////////////////////////////////////////////////////////////////////////////
{
	__dpmi_meminfo info = {0, memsize, phys_addr};
	if( __dpmi_physical_address_mapping(&info) == 0) {
		return info.address;
	}
	return 0;
}

void _dpmi_unmap_physical_memory(unsigned long linear_addr)
///////////////////////////////////////////////////////////
{
	__dpmi_meminfo info;
	info.address = linear_addr;
	__dpmi_free_physical_address_mapping( &info );
	return;
}

//copied from USBDDOS
static __dpmi_regs _xms_regs = {0};

#define pds_xms_inited() (_xms_regs.x.cs != 0 || _xms_regs.x.ip != 0)

static int pds_xms_init(void)
/////////////////////////////
{
	if(pds_xms_inited())
		return 1;
	//memset( &_xms_regs, 0, sizeof(_xms_regs) );
	_xms_regs.x.ax = 0x4300;
	__dpmi_simulate_real_mode_interrupt(0x2F, &_xms_regs);
	if(_xms_regs.h.al != 0x80)
		return  0;
	_xms_regs.x.ax = 0x4310;
	__dpmi_simulate_real_mode_interrupt(0x2F, &_xms_regs);    //control function in es:bx
	_xms_regs.x.cs = _xms_regs.x.es;
	_xms_regs.x.ip = _xms_regs.x.bx;
	//_xms_regs.x.ss = _xms_regs.x.sp = 0;
	return 1;
}

/* alloc xms memory.
 * returns xms handle and physical address in addr.
 */
static unsigned short xms_alloc(unsigned short sizeKB, unsigned long* addr)
///////////////////////////////////////////////////////////////////////////
{
	unsigned short handle;
	*addr = 0;
   
	if(sizeKB == 0 || !pds_xms_init())
		return 0;
	//r = _xms_regs;
	_xms_regs.h.ah = 0x09;      //alloc XMS
	_xms_regs.x.dx = sizeKB;    //size in kb
	__dpmi_simulate_real_mode_procedure_retf(&_xms_regs);
	if (_xms_regs.x.ax != 0x1)
		return 0;
	handle = _xms_regs.x.dx;

	_xms_regs.x.dx = handle;
	_xms_regs.h.ah = 0x0C;    //lock XMS
	__dpmi_simulate_real_mode_procedure_retf(&_xms_regs);
	if(_xms_regs.x.ax != 0x1) {
		_xms_regs.h.ah = 0x0A; //free XMS
		__dpmi_simulate_real_mode_procedure_retf(&_xms_regs);
		return 0;
	}
	*addr = ((unsigned long)_xms_regs.x.dx << 16L) | (unsigned long)_xms_regs.x.bx;
	return handle;
}

static int xms_free(unsigned short handle)
//////////////////////////////////////////
{
	if(!pds_xms_inited())
		return 0;
	_xms_regs.h.ah = 0x0D;
	_xms_regs.x.dx = handle;
	__dpmi_simulate_real_mode_procedure_retf(&_xms_regs);
	if(_xms_regs.x.ax != 1)
		return 0;
	_xms_regs.h.ah = 0x0A;
	_xms_regs.x.dx = handle;
	__dpmi_simulate_real_mode_procedure_retf(&_xms_regs);
	return _xms_regs.x.ax == 1;
}

/* alloc XMS memory and map it into linear address space */

int _alloc_physical_memory( struct xmsmem_s * mem, unsigned int size)
/////////////////////////////////////////////////////////////////////
{
	if( ( mem->handle = xms_alloc( (size+1023)/1024, &mem->physicalptr) ) ) {
		if ( mem->dwLinear = _dpmi_map_physical_memory( mem->physicalptr, size ) ) {
			return 1;
		}
		xms_free( mem->handle );
	}
	dbgprintf("No XMS memory.\n");
	return 0;
}

void _free_physical_memory( struct xmsmem_s * mem)
//////////////////////////////////////////////////
{
	_dpmi_unmap_physical_memory( mem->dwLinear );
	xms_free( mem->handle );
}
