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
//function: alloc & free physical memory (XMS)

#include <stdint.h>
#include <stdio.h>
//#include <dpmi.h>

#include "DJDPMI.H"
#include "CONFIG.H" /* for dbgprintf() */
#include "NEWFUNC.H"

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
static uint16_t xms_alloc( uint16_t sizeKB, uint32_t* addr )
////////////////////////////////////////////////////////////
{
	uint16_t handle;
	*addr = 0;
   
	if(sizeKB == 0 || !pds_xms_init())
		return 0;
	_xms_regs.h.ah = 0x09;    //alloc memory block
	_xms_regs.x.dx = sizeKB;
	__dpmi_simulate_real_mode_procedure_retf(&_xms_regs);
	if ( _xms_regs.x.ax != 1 )
		return 0;
	handle = _xms_regs.x.dx;
	_xms_regs.h.ah = 0x0C;    //lock memory block
	__dpmi_simulate_real_mode_procedure_retf(&_xms_regs);
	if( _xms_regs.x.ax != 1 ) {
		_xms_regs.h.ah = 0x0A; //free memory block
		__dpmi_simulate_real_mode_procedure_retf(&_xms_regs);
		return 0;
	}
	*addr = ((uint32_t)_xms_regs.x.dx << 16) | _xms_regs.x.bx;
	return handle;
}

static int xms_free( uint16_t handle )
//////////////////////////////////////
{
	if(!pds_xms_inited())
		return 0;
	_xms_regs.h.ah = 0x0D; /* unlock memory block */
	_xms_regs.x.dx = handle;
	__dpmi_simulate_real_mode_procedure_retf(&_xms_regs);
	//if(_xms_regs.x.ax != 1)
	//	return 0;
	_xms_regs.h.ah = 0x0A; /* free memory block */
	//_xms_regs.x.dx = handle;
	__dpmi_simulate_real_mode_procedure_retf(&_xms_regs);
	return _xms_regs.x.ax == 1;
}

/* alloc XMS memory and map it into linear address space */

int _alloc_physical_memory( struct xmsmem_s * mem, uint32_t size)
/////////////////////////////////////////////////////////////////
{
	if( ( mem->handle = xms_alloc( (size+1023)/1024, &mem->physicalptr ) ) ) {
		__dpmi_meminfo info;
		info.address = mem->physicalptr;
		info.size = size;
		if ( __dpmi_physical_address_mapping( &info ) == 0 ) {
			mem->dwLinear = info.address;
			return 1;
		}
		xms_free( mem->handle );
	}
	dbgprintf(("No XMS memory.\n"));
	return 0;
}

void _free_physical_memory( struct xmsmem_s * mem)
//////////////////////////////////////////////////
{
	__dpmi_meminfo info;
	info.address = mem->dwLinear;
	__dpmi_free_physical_address_mapping( &info );
	xms_free( mem->handle );
}
