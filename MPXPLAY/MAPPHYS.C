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
#include <sys/segments.h>  /* for _my_ds() */

#include "CONFIG.H"
#include "NEWFUNC.H"

#define PHYSICAL_MAP_COUNT 16

static __dpmi_meminfo physicalmaps[PHYSICAL_MAP_COUNT];

unsigned long pds_dpmi_map_physical_memory( unsigned long phys_addr, unsigned long memsize)
///////////////////////////////////////////////////////////////////////////////////////////
{
	memsize = (memsize+4095)/4096*4096; //__dpmi_set_segment_limit need page aligned
	__dpmi_meminfo info = {0, memsize, phys_addr};

	int i;
	for( i = 0; i < PHYSICAL_MAP_COUNT; ++i) {
		if(physicalmaps[i].handle == 0)
			break;
	}

	unsigned long base = 0;
	__dpmi_get_segment_base_address(_my_ds(), &base);
	unsigned long limit = __dpmi_get_segment_limit(_my_ds());

	if(i < PHYSICAL_MAP_COUNT && __dpmi_physical_address_mapping(&info) == 0) {
		if( info.address < base ) {
			__dpmi_free_physical_address_mapping(&info);
			info.address = base + limit + 1;
			if(__dpmi_allocate_linear_memory(&info, 0) != 0) {
				printf("DPMI map physical memory failed.\n");
				return 0;
			}
			__dpmi_meminfo remap = info;
			remap.address = 0;
			remap.size = (memsize + 4095) / 4096;
			if(__dpmi_map_device_in_memory_block(&remap, phys_addr) != 0) {
				__dpmi_free_memory(info.handle);
				return 0;
			}
			info.size = 0;
		}
		info.address -= base;
		physicalmaps[i] = info;
		unsigned long newlimit = info.address + memsize - 1;
		__dpmi_set_segment_limit(_my_ds(), max(limit, newlimit));
		//__dpmi_set_segment_limit( __djgpp_ds_alias, max(limit, newlimit));
		return info.address;
	}
	return 0;
}

void pds_dpmi_unmap_physical_memory(unsigned long linear_addr)
//////////////////////////////////////////////////////////////
{
	int i = 0;
	for(; i < PHYSICAL_MAP_COUNT; ++i)
	{
		if(physicalmaps[i].handle != 0 && physicalmaps[i].address == linear_addr)
			break;
	}
	if(i >= PHYSICAL_MAP_COUNT)
		return;
	if(physicalmaps[i].size != 0)
		__dpmi_free_physical_address_mapping(&physicalmaps[i]);
	else
		__dpmi_free_memory(physicalmaps[i].handle);
	physicalmaps[i].handle = 0;
}

//copied from USBDDOS
static __dpmi_regs pds_xms_regs;

#define pds_xms_inited() (pds_xms_regs.x.cs != 0 || pds_xms_regs.x.ip != 0)

static int pds_xms_init(void)
/////////////////////////////
{
	if(pds_xms_inited())
		return 1;
	memset( &pds_xms_regs, 0, sizeof(pds_xms_regs) );
	pds_xms_regs.x.ax = 0x4300;
	__dpmi_simulate_real_mode_interrupt(0x2F, &pds_xms_regs);
	if(pds_xms_regs.h.al != 0x80)
		return  0;
	pds_xms_regs.x.ax = 0x4310;
	__dpmi_simulate_real_mode_interrupt(0x2F, &pds_xms_regs);    //control function in es:bx
	pds_xms_regs.x.cs = pds_xms_regs.x.es;
	pds_xms_regs.x.ip = pds_xms_regs.x.bx;
	pds_xms_regs.x.ss = pds_xms_regs.x.sp = 0;
	return 1;
}

static unsigned short xms_alloc(unsigned short sizeKB, unsigned long* addr)
///////////////////////////////////////////////////////////////////////////
{
	__dpmi_regs r;
	unsigned short handle;
	*addr = 0;
   
	if(sizeKB == 0 || !pds_xms_init())
		return 0;
	r = pds_xms_regs;
	r.h.ah = 0x09;      //alloc XMS
	r.x.dx = sizeKB;    //size in kb
	__dpmi_simulate_real_mode_procedure_retf(&r);
	if (r.x.ax != 0x1)
		return 0;
	handle = r.x.dx;

	r = pds_xms_regs;
	r.x.dx = handle;
	r.h.ah = 0x0C;    //lock XMS
	__dpmi_simulate_real_mode_procedure_retf(&r);
	if(r.x.ax != 0x1)
	{
		r = pds_xms_regs;
		r.h.ah = 0x0A; //free XMS
		__dpmi_simulate_real_mode_procedure_retf(&r);
		return 0;
	}
	*addr = ((unsigned long)r.x.dx << 16L) | (unsigned long)r.x.bx;
	return handle;
}

static int xms_free(unsigned short handle)
//////////////////////////////////////////
{
	__dpmi_regs r = pds_xms_regs;

	if(!pds_xms_inited())
		return 0;
	r.h.ah = 0x0D;
	r.x.dx = handle;
	__dpmi_simulate_real_mode_procedure_retf(&r);
	if(r.x.ax != 1)
		return 0;
	r = pds_xms_regs;
	r.h.ah = 0x0A;
	r.x.dx = handle;
	__dpmi_simulate_real_mode_procedure_retf(&r);
	return r.x.ax == 1;
}

/* alloc XMS memory and map it so the memory can be accessed with near pointers. */

int pds_dpmi_alloc_physical_memory( xmsmem_t * mem, unsigned int size)
//////////////////////////////////////////////////////////////////////
{
	unsigned long addr;
	size = ( size + 4095) / 1024 * 1024;
	if( ( mem->handle = xms_alloc( size/1024, &addr)) ) {
		mem->linearptr = (char *)pds_dpmi_map_physical_memory( addr, size );
		if ( mem->linearptr ) {
			mem->physicalptr = addr;
			return 1;
		}
		xms_free( mem->handle );
		mem->handle = 0;
	}
	printf("No XMS memory.\n");
	return 0;
}

void pds_dpmi_free_physical_memory(xmsmem_t * mem)
//////////////////////////////////////////////////
{
	pds_dpmi_unmap_physical_memory( (long unsigned int)(mem->linearptr) );
	xms_free( mem->handle );
}
