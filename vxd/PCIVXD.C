/* PCIVXD.C: PCIBIOS.C replacement for VSBVXD - configuration mechanism #1
 * (ports 0CF8h/0CFCh) and a bus scan instead of the PCI BIOS (Int 1Ah),
 * which can't be called from ring 0. */

#include <stdint.h>
#include <conio.h>

#include "PCIBIOS.H"

#define PCI_ADDR   0x0CF8
#define PCI_DATA   0x0CFC
#define ENABLE_BIT 0x80000000

static uint32_t cfgaddr( uint8_t bus, uint8_t dev, uint8_t func, uint16_t reg )
{
	return ENABLE_BIT | ((uint32_t)bus << 16) | ((uint32_t)dev << 11) | ((uint32_t)func << 8) | (reg & 0xFC);
}

static uint32_t rd32( uint8_t bus, uint8_t dev, uint8_t func, uint16_t reg )
{
	outpd( PCI_ADDR, cfgaddr( bus, dev, func, reg ) );
	return inpd( PCI_DATA );
}

static void wr32( uint8_t bus, uint8_t dev, uint8_t func, uint16_t reg, uint32_t val )
{
	outpd( PCI_ADDR, cfgaddr( bus, dev, func, reg ) );
	outpd( PCI_DATA, val );
}

/* read/modify/write of size bytes (1, 2 or 4) within one dword */
static uint32_t rd( struct pci_config_s *p, uint16_t reg, int size )
{
	uint32_t v = rd32( p->bBus, p->bDev, p->bFunc, reg ) >> ((reg & 3) * 8);
	return size == 4 ? v : v & ((1UL << (size * 8)) - 1);
}

static void wr( struct pci_config_s *p, uint16_t reg, int size, uint32_t val )
{
	int shift = (reg & 3) * 8;
	uint32_t mask = size == 4 ? 0xFFFFFFFF : ((1UL << (size * 8)) - 1) << shift;
	uint32_t v = size == 4 ? val : (rd32( p->bBus, p->bDev, p->bFunc, reg ) & ~mask) | ((val << shift) & mask);
	wr32( p->bBus, p->bDev, p->bFunc, reg, v );
}

uint8_t  pcibios_ReadConfig_Byte ( struct pci_config_s *p, uint16_t a ) { return rd( p, a, 1 ); }
uint16_t pcibios_ReadConfig_Word ( struct pci_config_s *p, uint16_t a ) { return rd( p, a, 2 ); }
uint32_t pcibios_ReadConfig_Dword( struct pci_config_s *p, uint16_t a ) { return rd( p, a, 4 ); }
void pcibios_WriteConfig_Byte ( struct pci_config_s *p, uint16_t a, uint8_t v )  { wr( p, a, 1, v ); }
void pcibios_WriteConfig_Word ( struct pci_config_s *p, uint16_t a, uint16_t v ) { wr( p, a, 2, v ); }
void pcibios_WriteConfig_Dword( struct pci_config_s *p, uint16_t a, uint32_t v ) { wr( p, a, 4, v ); }

/* walk all functions of all devices; cb returns nonzero to stop */
static int scan( int (*cb)( struct pci_config_s *, void * ), void *ctx, struct pci_config_s *out )
{
	struct pci_config_s p;
	unsigned bus, dev, func, nfunc;

	for ( bus = 0; bus < 256; bus++ )
		for ( dev = 0; dev < 32; dev++ ) {
			p.bBus = bus; p.bDev = dev; p.bFunc = 0;
			if ( (uint16_t)rd32( bus, dev, 0, 0 ) == 0xFFFF )
				continue;
			nfunc = ( rd( &p, PCIR_HEADT, 1 ) & 0x80 ) ? 8 : 1;
			for ( func = 0; func < nfunc; func++ ) {
				uint32_t id = rd32( bus, dev, func, 0 );
				if ( (uint16_t)id == 0xFFFF )
					continue;
				p.bFunc = func;
				p.vendor_id = (uint16_t)id;
				p.device_id = id >> 16;
				p.bIrq = rd( &p, PCIR_INTR_LN, 1 );
				p.device_name = 0;
				p.device_type = 0;
				if ( cb( &p, ctx ) ) {
					if ( out )
						*out = p;
					return 1;
				}
			}
		}
	return 0;
}

struct findctx { uint16_t vendor, device; uint32_t class; unsigned index; };

static int match_id( struct pci_config_s *p, void *v )
{
	struct findctx *c = v;
	return p->vendor_id == c->vendor && p->device_id == c->device;
}

static int match_class( struct pci_config_s *p, void *v )
{
	struct findctx *c = v;
	if ( ( rd( p, 0x08, 4 ) >> 8 ) != c->class )
		return 0;
	return c->index-- == 0;
}

uint8_t pcibios_FindDevice( uint16_t wVendor, uint16_t wDevice, struct pci_config_s *ppkey )
{
	struct findctx c;
	c.vendor = wVendor;
	c.device = wDevice;
	return scan( match_id, &c, ppkey ) ? PCI_SUCCESSFUL : PCI_DEVICE_NOTFOUND;
}

uint8_t pcibios_FindDeviceClass( uint8_t bClass, uint8_t bSubClass, uint8_t bInterface, uint16_t wIndex,
                                 const struct pci_device_s devices[], struct pci_config_s *ppkey )
{
	struct findctx c;
	int i;
	c.class = (uint32_t)bClass << 16 | bSubClass << 8 | bInterface;
	c.index = wIndex;
	if ( !scan( match_class, &c, ppkey ) )
		return PCI_DEVICE_NOTFOUND;
	for ( i = 0; devices[i].vendor_id; i++ )
		if ( devices[i].vendor_id == ppkey->vendor_id && devices[i].device_id == ppkey->device_id ) {
			ppkey->device_name = devices[i].device_name;
			ppkey->device_type = devices[i].device_type;
			break;
		}
	return PCI_SUCCESSFUL;
}

uint8_t pcibios_search_devices( const struct pci_device_s devices[], struct pci_config_s *ppkey )
{
	unsigned i;
	for ( i = 0; devices[i].vendor_id; i++ )
		if ( pcibios_FindDevice( devices[i].vendor_id, devices[i].device_id, ppkey ) == PCI_SUCCESSFUL ) {
			if ( ppkey ) {
				ppkey->device_name = devices[i].device_name;
				ppkey->device_type = devices[i].device_type;
			}
			return PCI_SUCCESSFUL;
		}
	return PCI_DEVICE_NOTFOUND;
}

void pcibios_enable_BM_IO( struct pci_config_s *p )
{
	unsigned cmd = pcibios_ReadConfig_Word( p, PCIR_PCICMD );
	cmd |= 0x01 | 0x04;
	cmd &= ~0x400;
	pcibios_WriteConfig_Word( p, PCIR_PCICMD, cmd );
}

void pcibios_enable_BM_MM( struct pci_config_s *p )
{
	unsigned cmd = pcibios_ReadConfig_Word( p, PCIR_PCICMD );
	cmd &= ~0x01;
	cmd |= 0x02 | 0x04;
	cmd &= ~0x400;
	pcibios_WriteConfig_Word( p, PCIR_PCICMD, cmd );
}

/* log all multimedia devices (class 04h) */
static int show( struct pci_config_s *p, void *v )
{
	extern void vlog( const char *, ... );
	uint32_t cls = rd( p, 0x08, 4 ) >> 8;
	(void)v;
	if ( ( cls >> 16 ) == 0x04 )
		vlog( "PCI %02x:%02x.%u %04x:%04x class %06x irq %u\n", p->bBus, p->bDev, p->bFunc,
			  p->vendor_id, p->device_id, cls, p->bIrq );
	return 0;
}

void pci_list_audio( void )
{
	scan( show, 0, 0 );
}
