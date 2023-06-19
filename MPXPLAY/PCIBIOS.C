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
//function: PCI-BIOS handling
//based on a code of Taichi Sugiyama (YAMAHA)

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <go32.h>
#include <dpmi.h>

#include "NEWFUNC.H"
#include "PCIBIOS.H"

/* PCIBIOSACCESS 1 requires the go32 transfer buffer to be present; that's
 * not true after the program has become a TSR. Seems to work, at least for
 * HDA...
 */
#define PCIBIOSACCESS 0 /* 0=access PCI config space with I/O instructions */

#ifdef _DEBUG
extern void fatal_error( int );
#endif

#define PCIDEVNUM(bParam)      (bParam >> 3)
#define PCIFUNCNUM(bParam)     (bParam & 0x07)
#define PCIDEVFUNC(bDev,bFunc) ((((uint32_t)bDev) << 3) | bFunc)

#define pcibios_clear_regs(reg) pds_memset(&reg,0,sizeof(reg))

/* VSBHDA: DPMI function 0x300 is used instead of int386(); this allows to set
 * a real-mode stack that is >= 1kB, as required by PCI BIOS specs.
 * The djgpp "transfer buffer" is used as this stack - be aware that this buffer,
 * which is located just behind the PSP and is usually 16 kB,
 * isn't available anymore once vsbhda is installed as TSR!
 */

static uint8_t pcibios_GetBus(void)
///////////////////////////////////
{
	//union REGS reg;
	__dpmi_regs reg = {0}; /* use the "simulate int" function */

	//pcibios_clear_regs(reg);

	reg.h.ah = PCI_FUNCTION_ID;
	reg.h.al = PCI_BIOS_PRESENT;
	reg.x.flags = 0x203;
	reg.x.sp = (uint16_t)(_go32_info_block.size_of_transfer_buffer);
	reg.x.ss = _go32_info_block.linear_address_of_transfer_buffer >> 4;

	//int386(PCI_SERVICE, &reg, &reg);
	__dpmi_simulate_real_mode_interrupt( PCI_SERVICE, &reg );

	if(reg.x.flags & 1)
		return 0;

	return 1;
}

uint8_t    pcibios_FindDevice(uint16_t wVendor, uint16_t wDevice, struct pci_config_s *ppkey)
/////////////////////////////////////////////////////////////////////////////////////////////
{
	//union REGS reg;
	__dpmi_regs reg = {0}; /* use the "simulate int" function */

	//pcibios_clear_regs(reg);

	reg.h.ah = PCI_FUNCTION_ID;
	reg.h.al = PCI_FIND_DEVICE;
	reg.x.cx = wDevice;
	reg.x.dx = wVendor;
	reg.x.si = 0;  //bIndex;
	reg.x.flags = 0x202;
	reg.x.sp = (uint16_t)(_go32_info_block.size_of_transfer_buffer);
	reg.x.ss = _go32_info_block.linear_address_of_transfer_buffer >> 4;

	//int386(PCI_SERVICE, &reg, &reg);
	__dpmi_simulate_real_mode_interrupt( PCI_SERVICE, &reg );

	if(ppkey && (reg.h.ah == PCI_SUCCESSFUL)){
		ppkey->bBus  = reg.h.bh;
		ppkey->bDev  = PCIDEVNUM(reg.h.bl);
		ppkey->bFunc = PCIFUNCNUM(reg.h.bl);
		ppkey->vendor_id = wVendor;
		ppkey->device_id = wDevice;
	}

	return reg.h.ah;
}

/* search for a specific device class ( used by SC_HDA ), including index.
 * added for vsbhda.
 * if a device is found, scan the table ( vendor/device ) to see if a special
 * "device_type" is to be set.
 */

uint8_t    pcibios_FindDeviceClass(uint8_t bClass, uint8_t bSubClass, uint8_t bInterface, uint16_t wIndex, const struct pci_device_s devices[], struct pci_config_s *ppkey)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	//union REGS reg;
	__dpmi_regs reg = {0}; /* use the "simulate int" function */

	//pcibios_clear_regs(reg);

	reg.h.ah = PCI_FUNCTION_ID;
	reg.h.al = PCI_FIND_CLASS;
	reg.d.ecx = bClass << 16 | bSubClass << 8 | bInterface;
	reg.x.si = wIndex;
	reg.x.flags = 0x202;
	reg.x.sp = (uint16_t)(_go32_info_block.size_of_transfer_buffer);
	reg.x.ss = _go32_info_block.linear_address_of_transfer_buffer >> 4;

	//int386(PCI_SERVICE, &reg, &reg);
	__dpmi_simulate_real_mode_interrupt( PCI_SERVICE, &reg );

	if(ppkey && (reg.h.ah == PCI_SUCCESSFUL)){
		ppkey->bBus  = reg.h.bh;
		ppkey->bDev  = PCIDEVNUM(reg.h.bl);
		ppkey->bFunc = PCIFUNCNUM(reg.h.bl);
		ppkey->vendor_id = pcibios_ReadConfig_Word( ppkey, PCIR_VID );
		ppkey->device_id = pcibios_ReadConfig_Word( ppkey, PCIR_DID );
		for( int i = 0; devices[i].vendor_id; i++ ){
			if ( devices[i].vendor_id == ppkey->vendor_id && devices[i].device_id == ppkey->device_id ) {
				ppkey->device_name = devices[i].device_name;
				ppkey->device_type = devices[i].device_type;
				break;
			}
		}
		return( PCI_SUCCESSFUL );
	}
	return PCI_DEVICE_NOTFOUND;
}

/* search a device by scanning a table of vendor-ids & device-ids */

uint8_t pcibios_search_devices(const struct pci_device_s devices[], struct pci_config_s *ppkey)
///////////////////////////////////////////////////////////////////////////////////////////////
{
	if(pcibios_GetBus()){
		unsigned int i = 0;
		while(devices[i].vendor_id){
			if(pcibios_FindDevice( devices[i].vendor_id,devices[i].device_id,ppkey) == PCI_SUCCESSFUL ){
				if(ppkey){
					ppkey->device_name=devices[i].device_name;
					ppkey->device_type=devices[i].device_type;
				}
				return PCI_SUCCESSFUL;
			}
			i++;
		}
	}
	return PCI_DEVICE_NOTFOUND;
}

#if PCIBIOSACCESS

uint8_t    pcibios_ReadConfig_Byte( struct pci_config_s * ppkey, uint16_t wAdr)
///////////////////////////////////////////////////////////////////////////////
{
	__dpmi_regs reg = {0}; /* use the "simulate int" function */

#ifdef _DEBUG
	if ( _go32_info_block.size_of_transfer_buffer == 0 )
        fatal_error( 1 );
#endif
	reg.h.ah = PCI_FUNCTION_ID;
	reg.h.al = PCI_READ_BYTE;
	reg.h.bh = ppkey->bBus;
	reg.h.bl = PCIDEVFUNC(ppkey->bDev, ppkey->bFunc);
	reg.x.di = wAdr;
	reg.x.sp = (uint16_t)(_go32_info_block.size_of_transfer_buffer);
	reg.x.ss = _go32_info_block.linear_address_of_transfer_buffer >> 4;
	//int386(PCI_SERVICE, &reg, &reg);
	__dpmi_simulate_real_mode_interrupt( PCI_SERVICE, &reg );

	return reg.h.cl;
}

uint16_t pcibios_ReadConfig_Word( struct pci_config_s * ppkey, uint16_t wAdr)
/////////////////////////////////////////////////////////////////////////////
{
	__dpmi_regs reg = {0}; /* use the "simulate int" function */

#ifdef _DEBUG
	if ( _go32_info_block.size_of_transfer_buffer == 0 )
        fatal_error( 1 );
#endif
	reg.h.ah = PCI_FUNCTION_ID;
	reg.h.al = PCI_READ_WORD;
	reg.h.bh = ppkey->bBus;
	reg.h.bl = PCIDEVFUNC(ppkey->bDev, ppkey->bFunc);
	reg.x.di = wAdr;
	reg.x.sp = (uint16_t)(_go32_info_block.size_of_transfer_buffer);
	reg.x.ss = _go32_info_block.linear_address_of_transfer_buffer >> 4;
	//int386(PCI_SERVICE, &reg, &reg);
	__dpmi_simulate_real_mode_interrupt( PCI_SERVICE, &reg );

	return reg.x.cx;
}

uint32_t pcibios_ReadConfig_Dword( struct pci_config_s * ppkey, uint16_t wAdr)
//////////////////////////////////////////////////////////////////////////////
{
	uint32_t dwData;

	dwData  = (uint32_t)pcibios_ReadConfig_Word(ppkey, wAdr + 2) << 16;
	dwData |= (uint32_t)pcibios_ReadConfig_Word(ppkey, wAdr);

	return dwData;
}

void pcibios_WriteConfig_Byte( struct pci_config_s * ppkey, uint16_t wAdr, uint8_t bData)
/////////////////////////////////////////////////////////////////////////////////////////
{
	__dpmi_regs reg = {0}; /* use the "simulate int" function */

#ifdef _DEBUG
	if ( _go32_info_block.size_of_transfer_buffer == 0 )
        fatal_error( 1 );
#endif

	reg.h.ah = PCI_FUNCTION_ID;
	reg.h.al = PCI_WRITE_BYTE;
	reg.h.bh = ppkey->bBus;
	reg.h.bl = PCIDEVFUNC(ppkey->bDev, ppkey->bFunc);
	reg.h.cl = bData;
	reg.x.di = wAdr;
	reg.x.sp = (uint16_t)(_go32_info_block.size_of_transfer_buffer);
	reg.x.ss = _go32_info_block.linear_address_of_transfer_buffer >> 4;

	//int386(PCI_SERVICE, &reg, &reg);
	__dpmi_simulate_real_mode_interrupt( PCI_SERVICE, &reg );
}

void pcibios_WriteConfig_Word( struct pci_config_s * ppkey, uint16_t wAdr, uint16_t wData)
//////////////////////////////////////////////////////////////////////////////////////////
{
	__dpmi_regs reg = {0}; /* use the "simulate int" function */

#ifdef _DEBUG
	if ( _go32_info_block.size_of_transfer_buffer == 0 )
        fatal_error( 1 );
#endif
	reg.h.ah = PCI_FUNCTION_ID;
	reg.h.al = PCI_WRITE_WORD;
	reg.h.bh = ppkey->bBus;
	reg.h.bl = PCIDEVFUNC(ppkey->bDev, ppkey->bFunc);
	reg.x.cx = wData;
	reg.x.di = wAdr;
	reg.x.sp = (uint16_t)(_go32_info_block.size_of_transfer_buffer);
	reg.x.ss = _go32_info_block.linear_address_of_transfer_buffer >> 4;

	//int386(PCI_SERVICE, &reg, &reg);
	__dpmi_simulate_real_mode_interrupt( PCI_SERVICE, &reg );
}

void pcibios_WriteConfig_Dword( struct pci_config_s * ppkey, uint16_t wAdr, uint32_t dwData)
////////////////////////////////////////////////////////////////////////////////////////////
{
	pcibios_WriteConfig_Word(ppkey, wAdr, LoW(dwData ));
	pcibios_WriteConfig_Word(ppkey, wAdr + 2, HiW(dwData));
}

#else

#define PCI_ADDR  0x0CF8
#define PCI_DATA  0x0CFC
#define ENABLE_BIT 0x80000000
#define inpd inportl
#define outpd outportl

static uint8_t PCI_ReadByte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg)
////////////////////////////////////////////////////////////////////////////////
{
	int shift = ((reg & 3) * 8);
	uint32_t val = ENABLE_BIT |
		((uint32_t)bus << 16) |
		((uint32_t)dev << 11) |
		((uint32_t)func << 8) |
		((uint32_t)reg & 0xFC);
	outpd(PCI_ADDR, val);
	return (inpd(PCI_DATA) >> shift) & 0xFF;
}

static uint16_t PCI_ReadWord(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg)
/////////////////////////////////////////////////////////////////////////////////
{
	if ((reg & 3) <= 2)
	{
		const int shift = ((reg & 3) * 8);
		const uint32_t val = ENABLE_BIT |
			((uint32_t)bus << 16) |
			((uint32_t)dev << 11) |
			((uint32_t)func << 8) |
			((uint32_t)reg & 0xFC);
		outpd(PCI_ADDR, val);
		return (inpd(PCI_DATA) >> shift) & 0xFFFF;
	}
	else
		return (uint16_t)((PCI_ReadByte(bus, dev, func, (uint8_t)(reg + 1)) << 8) | PCI_ReadByte(bus, dev, func, reg));
}

static uint32_t PCI_ReadDWord(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg)
//////////////////////////////////////////////////////////////////////////////////
{
	if ((reg & 3) == 0)
	{
		uint32_t val = ENABLE_BIT |
			((uint32_t)bus << 16) |
			((uint32_t)dev << 11) |
			((uint32_t)func << 8) |
			((uint32_t)reg & 0xFC);
		outpd(PCI_ADDR, val);
		return inpd(PCI_DATA);
	}
	else
		return ((uint32_t)PCI_ReadWord(bus, dev, func, (uint8_t)(reg + 2)) << 16L) | PCI_ReadWord(bus, dev, func, reg);
}

static void PCI_WriteByte(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint8_t value)
/////////////////////////////////////////////////////////////////////////////////////////////
{
	int shift = ((reg & 3) * 8);
	uint32_t val = ENABLE_BIT |
		((uint32_t)bus << 16) |
		((uint32_t)dev << 11) |
		((uint32_t)func << 8) |
		((uint32_t)reg & 0xFC);
	outpd(PCI_ADDR, val);
	outpd(PCI_DATA, (uint32_t)(inpd(PCI_DATA) & ~(0xFFU << shift)) | ((uint32_t)value << shift));
}


static void PCI_WriteWord(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint16_t value)
//////////////////////////////////////////////////////////////////////////////////////////////
{
	if ((reg & 3) <= 2)
	{
		int shift = ((reg & 3) * 8);
		uint32_t val = ENABLE_BIT |
			((uint32_t)bus << 16) |
			((uint32_t)dev << 11) |
			((uint32_t)func << 8) |
			((uint32_t)reg & 0xFC);
		outpd(PCI_ADDR, val);
		outpd(PCI_DATA, (inpd(PCI_DATA) & ~(0xFFFFU << shift)) | ((uint32_t)value << shift));
	}
	else
	{
		PCI_WriteByte(bus, dev, func, (uint8_t)(reg + 0), (uint8_t)(value & 0xFF));
		PCI_WriteByte(bus, dev, func, (uint8_t)(reg + 1), (uint8_t)(value >> 8));
	}
}


static void PCI_WriteDWord(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint32_t value)
///////////////////////////////////////////////////////////////////////////////////////////////
{
	if ((reg & 3) == 0)
	{
		uint32_t val = ENABLE_BIT |
			((uint32_t)bus << 16) |
			((uint32_t)dev << 11) |
			((uint32_t)func << 8) |
			((uint32_t)reg & 0xFC);
		outpd(PCI_ADDR, val);
		outpd(PCI_DATA, value);
	}
	else
	{
		PCI_WriteWord(bus, dev, func, (uint8_t)(reg + 0), (uint16_t)(value & 0xFFFF));
		PCI_WriteWord(bus, dev, func, (uint8_t)(reg + 2), (uint16_t)(value >> 16));
	}
}

uint8_t    pcibios_ReadConfig_Byte( struct pci_config_s * ppkey, uint16_t wAdr)
///////////////////////////////////////////////////////////////////////////////
{
	return PCI_ReadByte(ppkey->bBus, ppkey->bDev, ppkey->bFunc, wAdr);
}

uint16_t pcibios_ReadConfig_Word( struct pci_config_s * ppkey, uint16_t wAdr)
/////////////////////////////////////////////////////////////////////////////
{
	return PCI_ReadWord(ppkey->bBus, ppkey->bDev, ppkey->bFunc, wAdr);
}

uint32_t pcibios_ReadConfig_Dword( struct pci_config_s * ppkey, uint16_t wAdr)
//////////////////////////////////////////////////////////////////////////////
{
	return PCI_ReadDWord(ppkey->bBus, ppkey->bDev, ppkey->bFunc, wAdr);
}

void pcibios_WriteConfig_Byte( struct pci_config_s * ppkey, uint16_t wAdr, uint8_t bData)
/////////////////////////////////////////////////////////////////////////////////////////
{
	PCI_WriteByte(ppkey->bBus, ppkey->bDev, ppkey->bFunc, wAdr, bData);
}

void pcibios_WriteConfig_Word( struct pci_config_s * ppkey, uint16_t wAdr, uint16_t wData)
//////////////////////////////////////////////////////////////////////////////////////////
{
	PCI_WriteWord(ppkey->bBus, ppkey->bDev, ppkey->bFunc, wAdr, wData);
}

void pcibios_WriteConfig_Dword( struct pci_config_s * ppkey, uint16_t wAdr, uint32_t dwData)
////////////////////////////////////////////////////////////////////////////////////////////
{
	PCI_WriteDWord(ppkey->bBus, ppkey->bDev, ppkey->bFunc, wAdr, dwData);
}
#endif

void pcibios_set_master( struct pci_config_s *ppkey)
////////////////////////////////////////////////////
{
	unsigned int cmd;
	cmd = pcibios_ReadConfig_Byte(ppkey, PCIR_PCICMD); /* read cmd register ( offset 4 ) */
	cmd |= 0x01 | 0x04;  /* bit0: IO space, bit2: busmaster */
	pcibios_WriteConfig_Byte(ppkey, PCIR_PCICMD, cmd);
}

void pcibios_enable_memmap_set_master( struct pci_config_s *ppkey)
//////////////////////////////////////////////////////////////////
{
	unsigned int cmd;
	cmd = pcibios_ReadConfig_Byte(ppkey, PCIR_PCICMD);
	cmd &= ~0x01;     /* disable io-port mapping */
	cmd |= 0x02 | 0x04; /* enable memory mapping and set master */
	pcibios_WriteConfig_Byte(ppkey, PCIR_PCICMD, cmd);
}
