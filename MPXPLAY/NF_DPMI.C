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
//function: DPMI callings

#include <stdint.h>
#include <stdio.h>
#include <dpmi.h>
#include <sys/exceptn.h>

#include "CONFIG.H"
#include "NEWFUNC.H"

#define PHYSICAL_MAP_COUNT 64

static __dpmi_meminfo physicalmaps[PHYSICAL_MAP_COUNT];

unsigned long pds_dpmi_map_physical_memory( unsigned long phys_addr, unsigned long memsize)
///////////////////////////////////////////////////////////////////////////////////////////
{
	memsize = (memsize+4095)/4096*4096; //__dpmi_set_segment_limit need page aligned
	__dpmi_meminfo info = {0, memsize, phys_addr};

	int i = 0;
	for(; i < PHYSICAL_MAP_COUNT; ++i)
	{
		if(physicalmaps[i].handle == 0)
			break;
	}

	unsigned long base = 0;
	__dpmi_get_segment_base_address(_my_ds(), &base);
	unsigned long limit = __dpmi_get_segment_limit(_my_ds());

	if(i < PHYSICAL_MAP_COUNT && __dpmi_physical_address_mapping(&info) == 0)
	{
		if(info.address < base)
		{
			__dpmi_free_physical_address_mapping(&info);
			info.address = base + limit + 1;
			if(__dpmi_allocate_linear_memory(&info, 0) != 0)
			{
				printf("DPMI map physical memory failed.\n");
				return 0;
			}
			__dpmi_meminfo remap = info;
			remap.address = 0;
			remap.size = (memsize+4095)/4096;
			if(__dpmi_map_device_in_memory_block(&remap, phys_addr) != 0)
			{
				__dpmi_free_memory(info.handle);
				return 0;
			}
			info.size = 0;
		}
		info.address -= base;
		physicalmaps[i] = info;
		unsigned long newlimit = info.address + memsize - 1;
		__dpmi_set_segment_limit(_my_ds(), max(limit, newlimit));
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
	memset(&pds_xms_regs, 0, sizeof(pds_xms_regs));
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

static unsigned short pds_xms_alloc(unsigned short sizeKB, unsigned long* addr)
///////////////////////////////////////////////////////////////////////////////
{
	__dpmi_regs r;
	unsigned short handle = 0;
	*addr = 0;
   
	if(sizeKB == 0 || !pds_xms_init())
		return handle;
	r = pds_xms_regs;
	r.h.ah = 0x09;      //alloc XMS
	r.x.dx = sizeKB;    //size in kb
	__dpmi_simulate_real_mode_procedure_retf(&r);
	if (r.x.ax != 0x1)
		return handle;
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

static int pds_xms_free(unsigned short handle)
//////////////////////////////////////////////
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

/* djgpp style to alloc XMS memory.
 * it's done so the memory can be accessed with near pointers.
 * the djgpp DS segment limit is adjusted to cover the new mem block.
 */

int pds_dpmi_xms_allocmem( xmsmem_t * mem, unsigned int size)
/////////////////////////////////////////////////////////////
{
	unsigned long addr;
	size = (size+4095) / 1024 * 1024;
	if( ( mem->xms = pds_xms_alloc( size/1024, &addr)) ) {
		unsigned long base = 0;
		unsigned long limit = __dpmi_get_segment_limit(_my_ds());
		__dpmi_get_segment_base_address( _my_ds(), &base );

		__dpmi_meminfo info = {0, size, addr};
		mem->remap = 0;
		do {
			if( __dpmi_physical_address_mapping( &info ) == 0 ) {
				if( info.address < base ) {
					/*
					 * if mapped address is below DS base, try linear functions ( dpmi v1.0 )
					 */
					__dpmi_free_physical_address_mapping( &info );
					info.address = base + limit + 1;
					if(__dpmi_allocate_linear_memory( &info, 0 ) != 0) {
						printf("DPMI allocate linear memory failed.\n");
						break;
					}
					__dpmi_meminfo remap = info;
					remap.address = 0;
					remap.size = size / 4096;
					if(__dpmi_map_device_in_memory_block(&remap, addr) != 0)
						break;
					mem->remap = 1;
				}
				mem->handle = info.handle;
				mem->physicalptr = (char*)addr;
				mem->linearptr = (char*)(info.address - base);
				unsigned long newlimit = info.address + size - base - 1;
				newlimit = ((newlimit+1+0xFFF) & ~0xFFF) - 1; //__dpmi_set_segment_limit must be page aligned
				dbgprintf("pds_dpmi_xms_allocmem: linaddr=%x, limit=%x\n", mem->linearptr, newlimit);
				__dpmi_set_segment_limit( _my_ds(), max(limit, newlimit ));
				__dpmi_set_segment_limit( __djgpp_ds_alias, max(limit, newlimit ));
				return 1;
			}
		}while(0);
		pds_xms_free(mem->xms);
		mem->xms = 0;
	}
	printf("No XMS memory.\n");
	return 0;
}

void pds_dpmi_xms_freemem(xmsmem_t * mem)
/////////////////////////////////////////
{
	if(mem->remap)
		__dpmi_free_memory(mem->handle);
	else
	{
		__dpmi_meminfo info = {mem->handle, 0, 0};
		__dpmi_free_physical_address_mapping(&info);
	}
	pds_xms_free(mem->xms);
}
