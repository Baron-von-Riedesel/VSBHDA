
/* SB-XFI: code specific for EMU20K1 */

#include <stdint.h>
#include <stddef.h>
#include <malloc.h>
#include <errno.h>
#ifndef DJGPP
#include <conio.h>
#endif

#include "CONFIG.H"
#include "AU_CARDS.H"
#include "TIMER.H"
#include "REG20K1.H"
#include "CTHW20KX.H"

/* the SB XFI also supports memory-mapped IO; it's not used currently. */

static inline uint32_t hw_read_20kx(struct hw *hw, uint32_t reg)
////////////////////////////////////////////////////////////////
{
	outpd(hw->io_base + 0x0,reg);
	return ((uint32_t)inpd(hw->io_base + 0x4));
}

static inline void hw_write_20kx(struct hw *hw, uint32_t reg, uint32_t data)
////////////////////////////////////////////////////////////////////////////
{
	outpd(hw->io_base + 0x0,reg);
	outpd(hw->io_base + 0x4,data);
}

static inline uint32_t hw_read_pci(struct hw *hw, uint32_t reg)
///////////////////////////////////////////////////////////////
{
	outpd(hw->io_base + 0x10,reg);
	return ((uint32_t)inpd(hw->io_base + 0x14));
}

static inline void hw_write_pci(struct hw *hw, uint32_t reg, uint32_t data)
///////////////////////////////////////////////////////////////////////////
{
	outpd(hw->io_base + 0x10,reg);
	outpd(hw->io_base + 0x14,data);
}

/* get a bit field in 32-bit data */

static unsigned int get_field(unsigned int data, unsigned int field)
////////////////////////////////////////////////////////////////////
{
	int i;
	for(i = 0; !(field & (1 << i)); )
		i++;
	return ((data & field) >> i);
}

/* set a bit field in 32-bit data */

static void set_field(unsigned int *data, unsigned int field, unsigned int value)
/////////////////////////////////////////////////////////////////////////////////
{
	int i;
	for (i = 0; !(field & (1 << i)); )
		i++;
	*data = (*data & (~field)) | ((value << i) & field);
}

/*------------------------------------------------------*/

/* SRC */

/* Mixer Parameter Ring ram Low and High register.
 * Fixed-point value in 8.24 format for parameter channel */
#define MPRLH_PITCH 0xFFFFFFFF

/* SRC resource control block */
#define SRCCTL_STATE 0x00000007
#define SRCCTL_BM   0x00000008
#define SRCCTL_RSR  0x00000030
#define SRCCTL_SF   0x000001C0
#define SRCCTL_WR   0x00000200
#define SRCCTL_PM   0x00000400
#define SRCCTL_ROM  0x00001800
#define SRCCTL_VO   0x00002000
#define SRCCTL_ST   0x00004000
#define SRCCTL_IE   0x00008000
#define SRCCTL_ILSZ 0x000F0000
#define SRCCTL_BP   0x00100000

#define SRCCCR_CISZ 0x000007FF
#define SRCCCR_CWA  0x001FF800
#define SRCCCR_D    0x00200000
#define SRCCCR_RS   0x01C00000
#define SRCCCR_NAL  0x3E000000
#define SRCCCR_RA   0xC0000000

#define SRCCA_CA    0x03FFFFFF
#define SRCCA_RS    0x1C000000
#define SRCCA_NAL   0xE0000000

#define SRCSA_SA    0x03FFFFFF

#define SRCLA_LA    0x03FFFFFF

#define AR_SLOT_SIZE        4096
#define AR_SLOT_BLOCK_SIZE    16
#define AR_PTS_PITCH           6
#define AR_PARAM_SRC_OFFSET 0x60

static unsigned int src_param_pitch_mixer(unsigned int src_idx)
{
	return ((src_idx << 4) + AR_PTS_PITCH + AR_SLOT_SIZE - AR_PARAM_SRC_OFFSET) % AR_SLOT_SIZE;
}

union src_dirty {
	struct {
		unsigned short ctl:1;
		unsigned short ccr:1;
		unsigned short sa:1;
		unsigned short la:1;
		unsigned short ca:1;
		unsigned short mpr:1;
		unsigned short czbfs:1;	/* Clear Z-Buffers */
		unsigned short rsv:9;
	} bf;
	unsigned short data;
};
struct src_rsc_ctrl_blk {
	unsigned int ctl;
	unsigned int ccr;
	unsigned int ca;
	unsigned int sa;
	unsigned int la;
	unsigned int mpr;
	union src_dirty dirty;
};

/* SRC manager control block */
union src_mgr_dirty {
	struct {
		unsigned short enb0:1;
		unsigned short enb1:1;
		unsigned short enb2:1;
		unsigned short enb3:1;
		unsigned short enb4:1;
		unsigned short enb5:1;
		unsigned short enb6:1;
		unsigned short enb7:1;
		unsigned short enbsa:1;
		unsigned short rsv:7;
	} bf;
	unsigned short data;
};

struct src_mgr_ctrl_blk {
	unsigned int enbsa;
	unsigned int enb[8];
	union src_mgr_dirty dirty;
};

/* SRCIMP manager control block */
#define SRCAIM_ARC	0x00000FFF
#define SRCAIM_NXT	0x00FF0000
#define SRCAIM_SRC	0xFF000000

struct srcimap {
	unsigned int srcaim;
	unsigned int idx;
};

/* SRCIMP manager register dirty flags */
union srcimp_mgr_dirty {
	struct {
		unsigned short srcimap:1;
		unsigned short rsv:15;
	} bf;
	unsigned short data;
};

struct srcimp_mgr_ctrl_blk {
	struct srcimap srcimap;
	union srcimp_mgr_dirty dirty;
};

static int src_rsc_get_ctrl_blk(void **rblk)
{
	struct src_rsc_ctrl_blk *blk;

	*rblk = NULL;
	blk = calloc(1, sizeof(struct src_rsc_ctrl_blk));
	if (!blk)
		return -ENOMEM;

	*rblk = blk;

	return 0;
}
static int src_set_state(void  *blk, unsigned int state)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->ctl, SRCCTL_STATE, state);
	ctl->dirty.bf.ctl = 1;
	return 0;
}

static int src_set_bm(void *blk, unsigned int bm)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->ctl, SRCCTL_BM, bm);
	ctl->dirty.bf.ctl = 1;
	return 0;
}

static int src_set_rsr(void *blk, unsigned int rsr)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->ctl, SRCCTL_RSR, rsr);
	ctl->dirty.bf.ctl = 1;
	return 0;
}

static int src_set_sf(void *blk, unsigned int sf)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->ctl, SRCCTL_SF, sf);
	ctl->dirty.bf.ctl = 1;
	return 0;
}

static int src_set_wr(void *blk, unsigned int wr)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->ctl, SRCCTL_WR, wr);
	ctl->dirty.bf.ctl = 1;
	return 0;
}

static int src_set_pm(void *blk, unsigned int pm)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->ctl, SRCCTL_PM, pm);
	ctl->dirty.bf.ctl = 1;
	return 0;
}

static int src_set_rom(void *blk, unsigned int rom)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->ctl, SRCCTL_ROM, rom);
	ctl->dirty.bf.ctl = 1;
	return 0;
}

static int src_set_vo(void *blk, unsigned int vo)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->ctl, SRCCTL_VO, vo);
	ctl->dirty.bf.ctl = 1;
	return 0;
}

static int src_set_st(void *blk, unsigned int st)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->ctl, SRCCTL_ST, st);
	ctl->dirty.bf.ctl = 1;
	return 0;
}

static int src_set_ie(void *blk, unsigned int ie)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->ctl, SRCCTL_IE, ie);
	ctl->dirty.bf.ctl = 1;
	return 0;
}

static int src_set_ilsz(void *blk, unsigned int ilsz)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->ctl, SRCCTL_ILSZ, ilsz);
	ctl->dirty.bf.ctl = 1;
	return 0;
}

static int src_set_bp(void *blk, unsigned int bp)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->ctl, SRCCTL_BP, bp);
	ctl->dirty.bf.ctl = 1;
	return 0;
}

static int src_set_cisz(void *blk, unsigned int cisz)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->ccr, SRCCCR_CISZ, cisz);
	ctl->dirty.bf.ccr = 1;
	return 0;
}

static int src_set_ca(void *blk, unsigned int ca)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->ca, SRCCA_CA, ca);
	ctl->dirty.bf.ca = 1;
	return 0;
}

static int src_set_sa(void *blk, unsigned int sa)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->sa, SRCSA_SA, sa);
	ctl->dirty.bf.sa = 1;
	return 0;
}

static int src_set_la(void *blk, unsigned int la)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->la, SRCLA_LA, la);
	ctl->dirty.bf.la = 1;
	return 0;
}

static int src_set_pitch(void *blk, unsigned int pitch)
{
	struct src_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->mpr, MPRLH_PITCH, pitch);
	ctl->dirty.bf.mpr = 1;
	return 0;
}

static int src_set_clear_zbufs(void *blk, unsigned int clear)
{
	((struct src_rsc_ctrl_blk *)blk)->dirty.bf.czbfs = (clear ? 1 : 0);
	return 0;
}

static int src_set_dirty(void *blk, unsigned int flags)
{
	((struct src_rsc_ctrl_blk *)blk)->dirty.data = (flags & 0xffff);
	return 0;
}

static int src_commit_write(struct hw *hw, unsigned int idx, void *blk)
///////////////////////////////////////////////////////////////////////
{
	struct src_rsc_ctrl_blk *ctl = blk;
	int i;

	if (ctl->dirty.bf.czbfs) {
		/* Clear Z-Buffer registers */
		for (i = 0; i < 8; i++)
			hw_write_20kx(hw, SRCUPZ + idx * 0x100 + i * 0x4, 0);

		for (i = 0; i < 4; i++)
			hw_write_20kx(hw, SRCDN0Z + idx * 0x100 + i * 0x4, 0);

		for (i = 0; i < 8; i++)
			hw_write_20kx(hw, SRCDN1Z + idx * 0x100 + i * 0x4, 0);

		ctl->dirty.bf.czbfs = 0;
	}
	if (ctl->dirty.bf.mpr) {
		/* Take the parameter mixer resource in the same group as that
		 * the idx src is in for simplicity. Unlike src, all conjugate
		 * parameter mixer resources must be programmed for
		 * corresponding conjugate src resources. */
		unsigned int pm_idx = src_param_pitch_mixer(idx);
		hw_write_20kx(hw, PRING_LO_HI + 4 * pm_idx, ctl->mpr);
		hw_write_20kx(hw, PMOPLO + 8 * pm_idx, 0x3);
		hw_write_20kx(hw, PMOPHI + 8 * pm_idx, 0x0);
		ctl->dirty.bf.mpr = 0;
	}
	if (ctl->dirty.bf.sa) {
		hw_write_20kx(hw, SRCSA + idx * 0x100, ctl->sa);
		ctl->dirty.bf.sa = 0;
	}
	if (ctl->dirty.bf.la) {
		hw_write_20kx(hw, SRCLA + idx * 0x100, ctl->la);
		ctl->dirty.bf.la = 0;
	}
	if (ctl->dirty.bf.ca) {
		hw_write_20kx(hw, SRCCA + idx * 0x100, ctl->ca);
		ctl->dirty.bf.ca = 0;
	}

	/* Write srccf register */
	hw_write_20kx(hw, SRCCF + idx * 0x100, 0x0);

	if (ctl->dirty.bf.ccr) {
		hw_write_20kx(hw, SRCCCR + idx * 0x100, ctl->ccr);
		ctl->dirty.bf.ccr = 0;
	}
	if (ctl->dirty.bf.ctl) {
		hw_write_20kx(hw, SRCCTL + idx * 0x100, ctl->ctl);
		ctl->dirty.bf.ctl = 0;
	}

	return 0;
}

static int src_get_ca(struct hw *hw, unsigned int idx, void *blk)
/////////////////////////////////////////////////////////////////
{
	struct src_rsc_ctrl_blk *ctl = blk;
	ctl->ca = hw_read_20kx(hw, SRCCA + idx * 0x100);
	ctl->dirty.bf.ca = 0;

	return get_field(ctl->ca, SRCCA_CA);
}

static unsigned int src_get_dirty(void *blk)
{
	return ((struct src_rsc_ctrl_blk *)blk)->dirty.data;
}

static unsigned int src_dirty_conj_mask(void)
{
	return 0x20;
}
#if 0
static int src_mgr_enbs_src(void *blk, unsigned int idx)
{
	((struct src_mgr_ctrl_blk *)blk)->enbsa = ~(0x0);
	((struct src_mgr_ctrl_blk *)blk)->dirty.bf.enbsa = 1;
	((struct src_mgr_ctrl_blk *)blk)->enb[idx/32] |= (0x1 << (idx%32));
	return 0;
}
#endif
static int src_mgr_enb_src(void *blk, unsigned int idx)
{
	((struct src_mgr_ctrl_blk *)blk)->enb[idx/32] |= (0x1 << (idx%32));
	((struct src_mgr_ctrl_blk *)blk)->dirty.data |= (0x1 << (idx/32));
	return 0;
}

static int src_mgr_dsb_src(void *blk, unsigned int idx)
{
	((struct src_mgr_ctrl_blk *)blk)->enb[idx/32] &= ~(0x1 << (idx%32));
	((struct src_mgr_ctrl_blk *)blk)->dirty.data |= (0x1 << (idx/32));
	return 0;
}

static int src_mgr_commit_write(struct hw *hw, void *blk)
/////////////////////////////////////////////////////////
{
	struct src_mgr_ctrl_blk *ctl = blk;
	int i;
	unsigned int ret;

	if (ctl->dirty.bf.enbsa) {
		do {
			ret = hw_read_20kx(hw, SRCENBSTAT);
		} while (ret & 0x1);
		hw_write_20kx(hw, SRCENBS, ctl->enbsa);
		ctl->dirty.bf.enbsa = 0;
	}
	for (i = 0; i < 8; i++) {
		if ((ctl->dirty.data & (0x1 << i))) {
			hw_write_20kx(hw, SRCENB + i * 0x100, ctl->enb[i]);
			ctl->dirty.data &= ~(0x1 << i);
		}
	}

	return 0;
}

static int src_mgr_get_ctrl_blk(void **rblk)
////////////////////////////////////////////
{
	struct src_mgr_ctrl_blk *blk;

	*rblk = NULL;
	blk = calloc(1, sizeof( struct src_mgr_ctrl_blk));
	if (!blk)
		return -ENOMEM;

	*rblk = blk;

	return 0;
}

static int srcimp_mgr_get_ctrl_blk(void **rblk)
{
	struct srcimp_mgr_ctrl_blk *blk;

	*rblk = NULL;
	blk = calloc(1, sizeof( struct srcimp_mgr_ctrl_blk));
	if (!blk)
		return -ENOMEM;

	*rblk = blk;

	return 0;
}

#if ADC_SUPP
static int srcimp_mgr_set_imaparc(void *blk, unsigned int slot)
{
	struct srcimp_mgr_ctrl_blk *ctl = blk;

	set_field(&ctl->srcimap.srcaim, SRCAIM_ARC, slot);
	ctl->dirty.bf.srcimap = 1;
	return 0;
}

static int srcimp_mgr_set_imapuser(void *blk, unsigned int user)
{
	struct srcimp_mgr_ctrl_blk *ctl = blk;

	set_field(&ctl->srcimap.srcaim, SRCAIM_SRC, user);
	ctl->dirty.bf.srcimap = 1;
	return 0;
}

static int srcimp_mgr_set_imapnxt(void *blk, unsigned int next)
{
	struct srcimp_mgr_ctrl_blk *ctl = blk;

	set_field(&ctl->srcimap.srcaim, SRCAIM_NXT, next);
	ctl->dirty.bf.srcimap = 1;
	return 0;
}

static int srcimp_mgr_set_imapaddr(void *blk, unsigned int addr)
{
	struct srcimp_mgr_ctrl_blk *ctl = blk;

	ctl->srcimap.idx = addr;
	ctl->dirty.bf.srcimap = 1;
	return 0;
}

static int srcimp_mgr_commit_write(struct hw *hw, void *blk)
{
	struct srcimp_mgr_ctrl_blk *ctl = blk;

	if (ctl->dirty.bf.srcimap) {
		hw_write_20kx(hw, SRCIMAP + ctl->srcimap.idx * 0x100, ctl->srcimap.srcaim);
		ctl->dirty.bf.srcimap = 0;
	}

	return 0;
}
#endif
/*------------------------------------------------------*/

/* AMIXER */

/* AMIXER resource register dirty flags */
union amixer_dirty {
	struct {
		unsigned short amoplo:1;
		unsigned short amophi:1;
		unsigned short rsv:14;
	} bf;
	unsigned short data;
};

/* AMIXER resource control block */
struct amixer_rsc_ctrl_blk {
	unsigned int amoplo;
	unsigned int amophi;
	union amixer_dirty dirty;
};

#define AMOPLO_M    0x00000003 // mode mask
#define AMOPLO_X    0x0003FFF0
#define AMOPLO_Y    0xFFFC0000

#define AMOPHI_SADR  0x000000FF
#define AMOPHI_SE    0x80000000

static int amixer_set_mode(void *blk, unsigned int mode)
{
	struct amixer_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->amoplo, AMOPLO_M, mode);
	ctl->dirty.bf.amoplo = 1;
	return 0;
}

static int amixer_set_iv(void *blk, unsigned int iv)
{
	/* 20k1 amixer does not have this field */
	return 0;
}

static int amixer_set_x(void *blk, unsigned int x)
{
	struct amixer_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->amoplo, AMOPLO_X, x);
	ctl->dirty.bf.amoplo = 1;
	return 0;
}

static int amixer_set_y(void *blk, unsigned int y)
{
	struct amixer_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->amoplo, AMOPLO_Y, y);
	ctl->dirty.bf.amoplo = 1;
	return 0;
}

static int amixer_set_sadr(void *blk, unsigned int sadr)
{
	struct amixer_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->amophi, AMOPHI_SADR, sadr);
	ctl->dirty.bf.amophi = 1;
	return 0;
}

static int amixer_set_se(void *blk, unsigned int se)
{
	struct amixer_rsc_ctrl_blk *ctl = blk;
	set_field(&ctl->amophi, AMOPHI_SE, se);
	ctl->dirty.bf.amophi = 1;
	return 0;
}

static int amixer_set_dirty(void *blk, unsigned int flags)
{
	struct amixer_rsc_ctrl_blk *ctl = blk;
	ctl->dirty.data = (flags & 0xffff);
	return 0;
}

static int amixer_set_dirty_all(void *blk)
{
	struct amixer_rsc_ctrl_blk *ctl = blk;
	ctl->dirty.data = ~(0x0);
	return 0;
}

static int amixer_commit_write(struct hw *hw, unsigned int idx, void *blk)
{
	struct amixer_rsc_ctrl_blk *ctl = blk;
	if (ctl->dirty.bf.amoplo || ctl->dirty.bf.amophi) {
		hw_write_20kx(hw, AMOPLO+idx*8, ctl->amoplo);
		ctl->dirty.bf.amoplo = 0;
		hw_write_20kx(hw, AMOPHI+idx*8, ctl->amophi);
		ctl->dirty.bf.amophi = 0;
	}
	return 0;
}

static int amixer_get_y(void *blk)
{
	struct amixer_rsc_ctrl_blk *ctl = blk;
	return get_field(ctl->amoplo, AMOPLO_Y);
}

static unsigned int amixer_get_dirty(void *blk)
{
	struct amixer_rsc_ctrl_blk *ctl = blk;
	return ctl->dirty.data;
}

static int amixer_rsc_get_ctrl_blk(void **rblk)
{
	struct amixer_rsc_ctrl_blk *blk;

	*rblk = NULL;
	blk = calloc(1, sizeof(struct amixer_rsc_ctrl_blk));
	if (!blk)
		return -ENOMEM;

	*rblk = blk;

	return 0;
}

static int amixer_mgr_get_ctrl_blk(void **rblk)
///////////////////////////////////////////////
{
	/*amixer_mgr_ctrl_blk_t *blk;*/

	*rblk = NULL;
	/*blk = kzalloc(sizeof(*blk), GFP_KERNEL);
	if (!blk)
		return -ENOMEM;

	*rblk = blk;*/

	return 0;
}

/*------------------------------------------------------*/

/* DAIO */

/* I2S Transmitter/Receiver Control register */
#define I2SCTL_EA	0x00000004
#define I2SCTL_EI	0x00000010

/* S/PDIF Transmitter Control register */
#define SPOCTL_OE	0x00000001
#define SPOCTL_OS	0x0000000E
#define SPOCTL_RIV	0x00000010
#define SPOCTL_LIV	0x00000020
#define SPOCTL_SR	0x000000C0

/* S/PDIF Transmitter register dirty flags */
union dao_dirty {
	struct {
		unsigned short spos:1;
		unsigned short rsv:15;
	} bf;
	unsigned short data;
};

/* S/PDIF Transmitter control block */
struct dao_ctrl_blk {
	unsigned int spos; /* S/PDIF Output Channel Status Register */
	union dao_dirty dirty;
};

static int dao_commit_write(struct hw *hw, unsigned int idx, void *blk)
{
	struct dao_ctrl_blk *ctl = blk;

	if (ctl->dirty.bf.spos) {
		if (idx < 4) {
			/* S/PDIF SPOSx */
			hw_write_20kx(hw, SPOS+0x4*idx, ctl->spos);
		}
		ctl->dirty.bf.spos = 0;
	}

	return 0;
}

static int dao_get_ctrl_blk(void **rblk)
{
	struct dao_ctrl_blk *blk;

	*rblk = NULL;
	blk = calloc(1, sizeof(struct dao_ctrl_blk));
	if (!blk)
		return -ENOMEM;

	*rblk = blk;

	return 0;
}

/* Audio Input Mapper RAM */
#define AIM_ARC		0x00000FFF
#define AIM_NXT		0x007F0000

struct daoimap {
	unsigned int aim;
	unsigned int idx;
};

/* DAIO manager register dirty flags */
union daio_mgr_dirty {
	struct {
		unsigned int i2soctl:4;
		unsigned int i2sictl:4;
		unsigned int spoctl:4;
		unsigned int spictl:4;
		unsigned int daoimap:1;
		unsigned int rsv:15;
	} bf;
	unsigned int data;
};

/* DAIO manager control block */
struct daio_mgr_ctrl_blk {
	unsigned int i2sctl;
	unsigned int spoctl;
	unsigned int spictl;
	struct daoimap daoimap;
	union daio_mgr_dirty dirty;
};

static int daio_mgr_enb_dao(void *blk, unsigned int idx)
{
	struct daio_mgr_ctrl_blk *ctl = blk;

	if (idx < 4) {
		/* S/PDIF output */
		set_field(&ctl->spoctl, SPOCTL_OE << (idx*8), 1);
		ctl->dirty.bf.spoctl |= (0x1 << idx);
	} else {
		/* I2S output */
		idx %= 4;
		set_field(&ctl->i2sctl, I2SCTL_EA << (idx*8), 1);
		ctl->dirty.bf.i2soctl |= (0x1 << idx);
	}
	return 0;
}

static int daio_mgr_dsb_dao(void *blk, unsigned int idx)
{
	struct daio_mgr_ctrl_blk *ctl = blk;

	if (idx < 4) {
		/* S/PDIF output */
		set_field(&ctl->spoctl, SPOCTL_OE << (idx*8), 0);
		ctl->dirty.bf.spoctl |= (0x1 << idx);
	} else {
		/* I2S output */
		idx %= 4;
		set_field(&ctl->i2sctl, I2SCTL_EA << (idx*8), 0);
		ctl->dirty.bf.i2soctl |= (0x1 << idx);
	}
	return 0;
}

static int daio_mgr_dao_init(struct hw *hw, void *blk, unsigned int idx, unsigned int conf)
{
	struct daio_mgr_ctrl_blk *ctl = blk;

	if (idx < 4) {
		/* S/PDIF output */
		switch ((conf & 0x7)) {
		case 0:
			set_field(&ctl->spoctl, SPOCTL_SR << (idx*8), 3);
			break; /* CDIF */
		case 1:
			set_field(&ctl->spoctl, SPOCTL_SR << (idx*8), 0);
			break;
		case 2:
			set_field(&ctl->spoctl, SPOCTL_SR << (idx*8), 1);
			break;
		case 4:
			set_field(&ctl->spoctl, SPOCTL_SR << (idx*8), 2);
			break;
		default:
			break;
		}
		set_field(&ctl->spoctl, SPOCTL_LIV << (idx*8),
			  (conf >> 4) & 0x1); /* Non-audio */
		set_field(&ctl->spoctl, SPOCTL_RIV << (idx*8),
			  (conf >> 4) & 0x1); /* Non-audio */
		set_field(&ctl->spoctl, SPOCTL_OS << (idx*8),
			  ((conf >> 3) & 0x1) ? 2 : 2); /* Raw */

		ctl->dirty.bf.spoctl |= (0x1 << idx);
	} else {
		/* I2S output */
		/*idx %= 4; */
	}
	return 0;
}

static int daio_mgr_set_imaparc(void *blk, unsigned int slot)
{
	struct daio_mgr_ctrl_blk *ctl = blk;

	set_field(&ctl->daoimap.aim, AIM_ARC, slot);
	ctl->dirty.bf.daoimap = 1;
	return 0;
}

static int daio_mgr_set_imapnxt(void *blk, unsigned int next)
{
	struct daio_mgr_ctrl_blk *ctl = blk;

	set_field(&ctl->daoimap.aim, AIM_NXT, next);
	ctl->dirty.bf.daoimap = 1;
	return 0;
}

static int daio_mgr_set_imapaddr(void *blk, unsigned int addr)
{
	struct daio_mgr_ctrl_blk *ctl = blk;

	ctl->daoimap.idx = addr;
	ctl->dirty.bf.daoimap = 1;
	return 0;
}

static int daio_mgr_get_ctrl_blk(struct hw *hw, void **rblk)
////////////////////////////////////////////////////////////
{
	struct daio_mgr_ctrl_blk *blk;

	*rblk = NULL;
	blk = calloc(1, sizeof(struct daio_mgr_ctrl_blk));
	if (!blk)
		return -ENOMEM;

	blk->i2sctl = hw_read_20kx(hw, I2SCTL);
	blk->spoctl = hw_read_20kx(hw, SPOCTL);
	blk->spictl = hw_read_20kx(hw, SPICTL);

	*rblk = blk;

	return 0;
}

static int daio_mgr_commit_write(struct hw *hw, void *blk)
{
	struct daio_mgr_ctrl_blk *ctl = blk;
	int i;

	if (ctl->dirty.bf.i2sictl || ctl->dirty.bf.i2soctl) {
		for (i = 0; i < 4; i++) {
			if ((ctl->dirty.bf.i2sictl & (0x1 << i)))
				ctl->dirty.bf.i2sictl &= ~(0x1 << i);

			if ((ctl->dirty.bf.i2soctl & (0x1 << i)))
				ctl->dirty.bf.i2soctl &= ~(0x1 << i);
		}
		hw_write_20kx(hw, I2SCTL, ctl->i2sctl);
		pds_delay_10us(1*100);
	}
	if (ctl->dirty.bf.spoctl) {
		for (i = 0; i < 4; i++) {
			if ((ctl->dirty.bf.spoctl & (0x1 << i)))
				ctl->dirty.bf.spoctl &= ~(0x1 << i);
		}
		hw_write_20kx(hw, SPOCTL, ctl->spoctl);
		pds_delay_10us(1*100);
	}
	if (ctl->dirty.bf.spictl) {
		for (i = 0; i < 4; i++) {
			if ((ctl->dirty.bf.spictl & (0x1 << i)))
				ctl->dirty.bf.spictl &= ~(0x1 << i);
		}
		hw_write_20kx(hw, SPICTL, ctl->spictl);
		pds_delay_10us(1*100);
	}
	if (ctl->dirty.bf.daoimap) {
		hw_write_20kx(hw, DAOIMAP+ctl->daoimap.idx*4,
					ctl->daoimap.aim);
		ctl->dirty.bf.daoimap = 0;
	}

	return 0;
}

/*------------------------------------------------------*/

static int set_timer_irq(struct hw *hw, int enable)
///////////////////////////////////////////////////
{
	dbgprintf(("set_timer_irq(%u): hw->io_base=%X\n", enable, hw->io_base));
	hw_write_20kx(hw, GIE, enable ? IT_INT : 0);
	return 0;
}

static int set_timer_tick(struct hw *hw, unsigned int ticks)
////////////////////////////////////////////////////////////
{
	if (ticks)
		ticks |= TIMR_IE | TIMR_IP; /* these bits aren't documented */
	hw_write_20kx(hw, TIMR, ticks);
	return 0;
}

/* next 2 functions are specific for VSBHDA since irq mechanism is different */

static unsigned int get_timer_interrupt_pending(struct hw *hw)
//////////////////////////////////////////////////////////////
{
	return hw_read_20kx(hw, GIP) & IT_INT;
}

static void ack_interrupt(struct hw *hw, unsigned int status)
/////////////////////////////////////////////////////////////
{
	//dbgprintf(("ack_interrupt(%X)\n", status));
	hw_write_20kx(hw, GIP, status);
	return;
}

static unsigned int get_wc(struct hw *hw)
/////////////////////////////////////////
{
	return hw_read_20kx(hw, WC);
}

struct dac_conf {
	unsigned int msr; /* master sample rate in rsrs */
};

struct adc_conf {
	unsigned int msr; 	/* master sample rate in rsrs */
	unsigned char input; 	/* the input source of ADC */
	unsigned char mic20db; 	/* boost mic by 20db if input is microphone */
};

struct daio_conf {
	unsigned int msr; /* master sample rate in rsrs */
};

struct trn_conf {
	unsigned long vm_pgt_phys;
};

static int hw_auto_init(struct hw *hw)
//////////////////////////////////////
{
	unsigned int gctl;
	int i;

	gctl = hw_read_20kx(hw, GCTL);
	set_field(&gctl, GCTL_EAI, 0);
	hw_write_20kx(hw, GCTL, gctl);
	set_field(&gctl, GCTL_EAI, 1);
	hw_write_20kx(hw, GCTL, gctl);
	pds_delay_10us(1000);
	for (i = 0; i < 400000; i++) {
		gctl = hw_read_20kx(hw, GCTL);
		if (get_field(gctl, GCTL_AID))
			break;
	}
	if (!get_field(gctl, GCTL_AID)) {
		dbgprintf(("hw_auto_init: Card Auto-init failed!!!\n"));
		return -EBUSY;
	}

	return 0;
}

static int hw_pll_init(struct hw *hw, unsigned int rsr)
///////////////////////////////////////////////////////
{
	unsigned int i,pllctl;
	pllctl = (rsr == 48000) ? 0x1480a001 : 0x1480a731;
	for( i = 0; i < 3; i++ ) {
		if(hw_read_20kx(hw, PLLCTL) == pllctl)
			break;
		hw_write_20kx(hw, PLLCTL, pllctl);
		pds_delay_10us(40*100);
	}
	if( i >= 3) {
		dbgprintf(("hw_pll_init(%u): init failed\n", rsr));
		return -1;
	}
	return 0;
}

static int hw_daio_init(struct hw *hw, const struct daio_conf *info)
////////////////////////////////////////////////////////////////////
{
	uint32_t i2sorg,spdorg;

	dbgprintf(("hw_daio_init: info.msr=%u\n", info->msr));
	/* Read I2S CTL.  Keep original value. */
	/*i2sorg = hw_read_20kx(hw, I2SCTL);*/
	i2sorg = 0x94040404; /* enable all audio out and I2S-D input */
	/* Program I2S with proper master sample rate and enable
	 * the correct I2S channel. */
	i2sorg &= 0xfffffffc;

	/* Enable S/PDIF-out-A in fixed 24-bit data
	 * format and default to 48kHz. */
	/* Disable all before doing any changes. */
	hw_write_20kx(hw, SPOCTL, 0x0);
	spdorg = 0x05;

	switch(info->msr) {
	case 1:
		i2sorg |= 1;
		spdorg |= (0x0 << 6);
		break;
	case 2:
		i2sorg |= 2;
		spdorg |= (0x1 << 6);
		break;
	case 4:
		i2sorg |= 3;
		spdorg |= (0x2 << 6);
		break;
	default:
		i2sorg |= 1;
		break;
	}

	hw_write_20kx(hw, I2SCTL, i2sorg);
	hw_write_20kx(hw, SPOCTL, spdorg);

	/* Enable S/PDIF-in-A in fixed 24-bit data format. */
	/* Disable all before doing any changes. */
	hw_write_20kx(hw, SPICTL, 0x0);
	pds_delay_10us(1*100);
	spdorg = 0x0a0a0a0a;
	hw_write_20kx(hw, SPICTL, spdorg);
	pds_delay_10us(1*100);

	return 0;
}

static int hw_trn_init(struct hw *hw, const struct trn_conf *info)
//////////////////////////////////////////////////////////////////
{
	unsigned int trnctl;
	unsigned int ptp_phys_low, ptp_phys_high;

	dbgprintf(("hw_trn_init: info.vm_pgt_phys=%X!\n", info->vm_pgt_phys));
	/* Set up device page table */
	if ((~0UL) == info->vm_pgt_phys) {
		dbgprintf(("hw_trn_init: Wrong device page table page address!\n"));
		return -1;
	}

	trnctl = 0x13;  /* 32-bit, 4k-size page */
	ptp_phys_low = (unsigned int)info->vm_pgt_phys;
	//ptp_phys_high = upper_32_bits(info->vm_pgt_phys);
	ptp_phys_high = 0;
	if (sizeof(void *) == 8) /* 64bit address */
		trnctl |= (1 << 2);
#if 0 /* Only 4k h/w pages for simplicitiy */
#if PAGE_SIZE == 8192
	trnctl |= (1<<5);
#endif
#endif
	hw_write_20kx(hw, PTPALX, ptp_phys_low);
	hw_write_20kx(hw, PTPAHX, ptp_phys_high);
	hw_write_20kx(hw, TRNCTL, trnctl);
	hw_write_20kx(hw, TRNIS, 0x200c01); /* really needed? */

	return 0;
}

static int i2c_unlock(struct hw *hw)
////////////////////////////////////
{
	if((hw_read_pci(hw, 0xcc) & 0xff) == 0xaa)
		return 0;

	hw_write_pci(hw, 0xcc, 0x8c);
	hw_write_pci(hw, 0xcc, 0x0e);
	if((hw_read_pci(hw, 0xcc) & 0xff) == 0xaa)
		return 0;

	hw_write_pci(hw, 0xcc, 0xee);
	hw_write_pci(hw, 0xcc, 0xaa);
	if((hw_read_pci(hw, 0xcc) & 0xff) == 0xaa)
		return 0;

	return -1;
}

static void i2c_lock(struct hw *hw)
///////////////////////////////////
{
	if((hw_read_pci(hw, 0xcc) & 0xff) == 0xaa)
		hw_write_pci(hw, 0xcc, 0x00);
}

static void i2c_write(struct hw *hw, uint32_t device, uint32_t addr, uint32_t data)
///////////////////////////////////////////////////////////////////////////////////
{
	unsigned int ret = 0;

	do{
		ret = hw_read_pci(hw, 0xEC);
	}while(!(ret & 0x800000));
	hw_write_pci(hw, 0xE0, device);
	hw_write_pci(hw, 0xE4, (data << 8) | (addr & 0xff));
}

/* DAC operations */

static int hw_reset_dac(struct hw *hw)
//////////////////////////////////////
{
	uint32_t i = 0;
	uint16_t gpioorg = 0;
	unsigned int ret = 0;

	dbgprintf(("hw_reset_dac\n"));
	if(i2c_unlock(hw)) {
		dbgprintf(("hw_reset_dac: i2c_unlock() returned != 0\n"));
		return -1;
	}

	do {
		ret = hw_read_pci(hw, 0xEC);
	} while (!(ret & 0x800000));
	hw_write_pci(hw, 0xEC, 0x05);  /* write to i2c status control */

	/* To be effective, need to reset the DAC twice. */
	for(i = 0; i < 2;  i++){
		/* set gpio */
		pds_delay_10us(100*100);
		gpioorg = (uint16_t)hw_read_20kx(hw, GPIO);
		gpioorg &= 0xfffd;
		hw_write_20kx(hw, GPIO, gpioorg);
		pds_delay_10us(100);
		hw_write_20kx(hw, GPIO, gpioorg | 0x2);
	}

	i2c_write(hw, 0x00180080, 0x01, 0x80);
	i2c_write(hw, 0x00180080, 0x02, 0x10);

	i2c_lock(hw);

	return 0;
}

static int hw_dac_init(struct hw *hw, const struct dac_conf *info)
//////////////////////////////////////////////////////////////////
{
	uint32_t data = 0;
	uint16_t gpioorg = 0;
	unsigned int ret = 0;

	dbgprintf(("hw_dac_init: info.msr=%u\n", info->msr));
	if(hw->model == CTSB055X ) {
		/* SB055x, unmute outputs */
		gpioorg = (uint16_t)hw_read_20kx(hw, GPIO);
		gpioorg &= 0xffbf;    /* set GPIO6 to low */
		gpioorg |= 2;        /* set GPIO1 to high */
		hw_write_20kx(hw, GPIO, gpioorg);
		return 0;
	}

	/* mute outputs */
	gpioorg = (uint16_t)hw_read_20kx(hw, GPIO);
	gpioorg &= 0xffbf;
	hw_write_20kx(hw, GPIO, gpioorg);

	hw_reset_dac(hw);

	if(i2c_unlock(hw)) {
		dbgprintf(("hw_dac_init: i2c_unlock() failed!\n"));
		return -1;
	}

	hw_write_pci(hw, 0xEC, 0x05);  /* write to i2c status control */
	do {
		ret = hw_read_pci(hw, 0xEC);
	} while (!(ret & 0x800000));

	switch ( info->msr ) {
	case 1:data = 0x24;break;
	case 2:data = 0x25;break;
	case 4:data = 0x26;break;
	default:data = 0x24;break;
	}

	i2c_write(hw, 0x00180080, 0x06, data);
	i2c_write(hw, 0x00180080, 0x09, data);
	i2c_write(hw, 0x00180080, 0x0c, data);
	i2c_write(hw, 0x00180080, 0x0f, data);

	i2c_lock(hw);

	/* unmute outputs */
	gpioorg = (uint16_t)hw_read_20kx(hw, GPIO);
	gpioorg = gpioorg | 0x40;
	hw_write_20kx(hw, GPIO, gpioorg);

	return 0;
}

#if ADC_SUPP

/* ADC operations */

static int is_adc_input_selected_SB055x(struct hw *hw, enum ADCSRC type)
{
	return 0;
}

static int is_adc_input_selected_SBx(struct hw *hw, enum ADCSRC type)
{
	unsigned int data;

	data = hw_read_20kx(hw, GPIO);
	switch (type) {
	case ADC_MICIN:
		data = ((data & (0x1<<7)) && (data & (0x1<<8)));
		break;
	case ADC_LINEIN:
		data = (!(data & (0x1<<7)) && (data & (0x1<<8)));
		break;
	case ADC_NONE: /* Digital I/O */
		data = (!(data & (0x1<<8)));
		break;
	default:
		data = 0;
	}
	return data;
}

static int is_adc_input_selected_hendrix(struct hw *hw, enum ADCSRC type)
{
	unsigned int data;

	data = hw_read_20kx(hw, GPIO);
	switch (type) {
	case ADC_MICIN:
		data = (data & (0x1 << 7)) ? 1 : 0;
		break;
	case ADC_LINEIN:
		data = (data & (0x1 << 7)) ? 0 : 1;
		break;
	default:
		data = 0;
	}
	return data;
}

static int hw_is_adc_input_selected(struct hw *hw, enum ADCSRC type)
{
	switch (hw->model) {
	case CTSB055X:
		return is_adc_input_selected_SB055x(hw, type);
	case CTSB073X:
		return is_adc_input_selected_hendrix(hw, type);
	case CTUAA:
		return is_adc_input_selected_hendrix(hw, type);
	default:
		return is_adc_input_selected_SBx(hw, type);
	}
}

static int adc_input_select_SB055x(struct hw *hw, enum ADCSRC type, unsigned char boost)
{
	unsigned int data;

	/*
	 * check and set the following GPIO bits accordingly
	 * ADC_Gain = GPIO2
	 * DRM_off = GPIO3
	 * Mic_Pwr_on = GPIO7
	 * Digital_IO_Sel = GPIO8
	 * Mic_Sw = GPIO9
	 * Aux/MicLine_Sw = GPIO12
	 */
	data = hw_read_20kx(hw, GPIO);
	data &= 0xec73;
	switch (type) {
	case ADC_MICIN:
		data |= (0x1<<7) | (0x1<<8) | (0x1<<9) ;
		data |= boost ? (0x1<<2) : 0;
		break;
	case ADC_LINEIN:
		data |= (0x1<<8);
		break;
	case ADC_AUX:
		data |= (0x1<<8) | (0x1<<12);
		break;
	case ADC_NONE:
		data |= (0x1<<12);  /* set to digital */
		break;
	default:
		return -1;
	}

	hw_write_20kx(hw, GPIO, data);

	return 0;
}

static int adc_input_select_SBx(struct hw *hw, enum ADCSRC type, unsigned char boost)
{
	unsigned int data;
	unsigned int i2c_data;
	unsigned int ret;

	if (i2c_unlock(hw))
		return -1;

	do {
		ret = hw_read_pci(hw, 0xEC);
	} while (!(ret & 0x800000)); /* i2c ready poll */
	/* set i2c access mode as Direct Control */
	hw_write_pci(hw, 0xEC, 0x05);

	data = hw_read_20kx(hw, GPIO);
	switch (type) {
	case ADC_MICIN:
		data |= ((0x1 << 7) | (0x1 << 8));
		i2c_data = 0x1;  /* Mic-in */
		break;
	case ADC_LINEIN:
		data &= ~(0x1 << 7);
		data |= (0x1 << 8);
		i2c_data = 0x2; /* Line-in */
		break;
	case ADC_NONE:
		data &= ~(0x1 << 8);
		i2c_data = 0x0; /* set to Digital */
		break;
	default:
		i2c_lock(hw);
		return -1;
	}
	hw_write_20kx(hw, GPIO, data);
	i2c_write(hw, 0x001a0080, 0x2a, i2c_data);
	if (boost) {
		i2c_write(hw, 0x001a0080, 0x1c, 0xe7); /* +12dB boost */
		i2c_write(hw, 0x001a0080, 0x1e, 0xe7); /* +12dB boost */
	} else {
		i2c_write(hw, 0x001a0080, 0x1c, 0xcf); /* No boost */
		i2c_write(hw, 0x001a0080, 0x1e, 0xcf); /* No boost */
	}

	i2c_lock(hw);

	return 0;
}

static int adc_input_select_hendrix(struct hw *hw, enum ADCSRC type, unsigned char boost)
{
	unsigned int data;
	unsigned int i2c_data;
	unsigned int ret;

	if (i2c_unlock(hw))
		return -1;

	do {
		ret = hw_read_pci(hw, 0xEC);
	} while (!(ret & 0x800000)); /* i2c ready poll */
	/* set i2c access mode as Direct Control */
	hw_write_pci(hw, 0xEC, 0x05);

	data = hw_read_20kx(hw, GPIO);
	switch (type) {
	case ADC_MICIN:
		data |= (0x1 << 7);
		i2c_data = 0x1;  /* Mic-in */
		break;
	case ADC_LINEIN:
		data &= ~(0x1 << 7);
		i2c_data = 0x2; /* Line-in */
		break;
	default:
		i2c_lock(hw);
		return -1;
	}
	hw_write_20kx(hw, GPIO, data);
	i2c_write(hw, 0x001a0080, 0x2a, i2c_data);
	if (boost) {
		i2c_write(hw, 0x001a0080, 0x1c, 0xe7); /* +12dB boost */
		i2c_write(hw, 0x001a0080, 0x1e, 0xe7); /* +12dB boost */
	} else {
		i2c_write(hw, 0x001a0080, 0x1c, 0xcf); /* No boost */
		i2c_write(hw, 0x001a0080, 0x1e, 0xcf); /* No boost */
	}

	i2c_lock(hw);

	return 0;
}

static int hw_adc_input_select(struct hw *hw, enum ADCSRC type)
{
	int state = type == ADC_MICIN;

	switch (hw->model) {
	case CTSB055X:
		return adc_input_select_SB055x(hw, type, state);
	case CTSB073X:
		return adc_input_select_hendrix(hw, type, state);
	case CTUAA:
		return adc_input_select_hendrix(hw, type, state);
	default:
		return adc_input_select_SBx(hw, type, state);
	}
}

static int adc_init_SB055x(struct hw *hw, int input, int mic20db)
{
	return adc_input_select_SB055x(hw, input, mic20db);
}

static int adc_init_SBx(struct hw *hw, int input, int mic20db)
{
	unsigned short gpioorg;
	unsigned short input_source;
	unsigned int adcdata;
	unsigned int ret;

	input_source = 0x100;  /* default to analog */
	switch (input) {
	case ADC_MICIN:
		adcdata = 0x1;
		input_source = 0x180;  /* set GPIO7 to select Mic */
		break;
	case ADC_LINEIN:
		adcdata = 0x2;
		break;
	case ADC_VIDEO:
		adcdata = 0x4;
		break;
	case ADC_AUX:
		adcdata = 0x8;
		break;
	case ADC_NONE:
		adcdata = 0x0;
		input_source = 0x0;  /* set to Digital */
		break;
	default:
		adcdata = 0x0;
		break;
	}

	if (i2c_unlock(hw))
		return -1;

	do {
		ret = hw_read_pci(hw, 0xEC);
	} while (!(ret & 0x800000)); /* i2c ready poll */
	hw_write_pci(hw, 0xEC, 0x05);  /* write to i2c status control */

	i2c_write(hw, 0x001a0080, 0x0e, 0x08);
	i2c_write(hw, 0x001a0080, 0x18, 0x0a);
	i2c_write(hw, 0x001a0080, 0x28, 0x86);
	i2c_write(hw, 0x001a0080, 0x2a, adcdata);

	if (mic20db) {
		i2c_write(hw, 0x001a0080, 0x1c, 0xf7);
		i2c_write(hw, 0x001a0080, 0x1e, 0xf7);
	} else {
		i2c_write(hw, 0x001a0080, 0x1c, 0xcf);
		i2c_write(hw, 0x001a0080, 0x1e, 0xcf);
	}

	if (!(hw_read_20kx(hw, ID0) & 0x100))
		i2c_write(hw, 0x001a0080, 0x16, 0x26);

	i2c_lock(hw);

	gpioorg = (unsigned short)hw_read_20kx(hw,  GPIO);
	gpioorg &= 0xfe7f;
	gpioorg |= input_source;
	hw_write_20kx(hw, GPIO, gpioorg);

	return 0;
}

static int hw_adc_init(struct hw *hw, const struct adc_conf *info)
{
	if (hw->model == CTSB055X)
		return adc_init_SB055x(hw, info->input, info->mic20db);
	else
		return adc_init_SBx(hw, info->input, info->mic20db);
}
#endif

static int hw_card_init(struct hw *hw, struct card_conf *info)
//////////////////////////////////////////////////////////////
{
	int err;
	unsigned int gctl;
	unsigned int data;
	struct dac_conf dac_info = {0};
	struct adc_conf adc_info = {0};
	struct daio_conf daio_info = {0};
	struct trn_conf trn_info = {0};

	hw->io_base = info->iobase; /* vsbhda */
#if 0
	/* Get PCI io port base address and do Hendrix switch if needed.
	 * vsbhda: io port is known already.
	 */
	err = hw_card_start(hw);
	if (err) {
		dbgprintf(("hw_card_init: hw_card_start() failed, err=%d\n", err));
		return err;
	}
#endif
	/* PLL init */
	err = hw_pll_init(hw, info->rsr);
	if (err < 0) {
		dbgprintf(("hw_card_init: hw_pll_init() failed, err=%d\n", err));
		return err;
	}

	/* kick off auto-init */
	err = hw_auto_init(hw);
	if (err < 0) {
		dbgprintf(("hw_card_init: hw_auto_init() failed, err=%d\n", err));
		return err;
	}

	/* Enable audio ring */
	gctl = hw_read_20kx(hw, GCTL);
	set_field(&gctl, GCTL_EAC, 1);
	set_field(&gctl, GCTL_DBP, 1);
	set_field(&gctl, GCTL_TBP, 1);
	set_field(&gctl, GCTL_FBP, 1);
	set_field(&gctl, GCTL_ET, 1);
	hw_write_20kx(hw, GCTL, gctl);
	pds_delay_10us(10*100);

	/* Reset all global pending interrupts */
	hw_write_20kx(hw, GIE, 0);
	/* Reset all SRC pending interrupts */
	hw_write_20kx(hw, SRCIP, 0);
	//msleep(30);
	pds_delay_10us(30*100);

	/* Detect the card ID and configure GPIO accordingly. */
	switch (hw->model) {
	case CTSB055X:
		hw_write_20kx(hw, GPIOCTL, 0x13fe);
		break;
	case CTSB073X:
		hw_write_20kx(hw, GPIOCTL, 0x00e6);
		break;
	case CTUAA:
		hw_write_20kx(hw, GPIOCTL, 0x00c2);
		break;
	default:
		hw_write_20kx(hw, GPIOCTL, 0x01e6);
		break;
	}

	trn_info.vm_pgt_phys = info->vm_pgt_phys;
	err = hw_trn_init(hw, &trn_info);
	if (err < 0) {
		dbgprintf(("hw_card_init: hw_trn_init() failed, err=%d\n", err));
		return err;
	}

	daio_info.msr = info->msr;
	err = hw_daio_init(hw, &daio_info);
	if (err < 0) {
		dbgprintf(("hw_card_init: hw_daio_init() failed, err=%d\n", err));
		return err;
	}

	dac_info.msr = info->msr;
	err = hw_dac_init(hw, &dac_info);
	if (err < 0) {
		dbgprintf(("hw_card_init: hw_dac_init() failed, err=%d\n", err));
		return err;
	}
#if ADC_SUPP
	adc_info.msr = info->msr;
	//adc_info.input = ADC_LINEIN;
	adc_info.input = ADC_AUX;
	adc_info.mic20db = 0;
	err = hw_adc_init(hw, &adc_info);
	if (err < 0) {
		dbgprintf(("hw_card_init: hw_adc_init() failed, err=%d\n", err));
		return err;
	}
#endif

	data = hw_read_20kx(hw, SRCMCTL);
	data |= 0x1; /* Enables input from the audio ring */
	hw_write_20kx(hw, SRCMCTL, data);

	return 0;
}

static const struct hw ct20k1_preset = {
	hw_card_init,
	hw_pll_init,
#if ADC_SUPP
	hw_is_adc_input_selected,
	hw_adc_input_select,
#endif
	//hw_capabilities,
#if 0 //def CONFIG_PM_SLEEP
	hw_suspend,
	hw_resume,
#endif
	src_rsc_get_ctrl_blk,
	//src_put_rsc_ctrl_blk,
	src_set_state,
	src_set_bm,
	src_set_rsr,
	src_set_sf,
	src_set_wr,
	src_set_pm,
	src_set_rom,
	src_set_vo,
	src_set_st,
	src_set_ie,
	src_set_ilsz,
	src_set_bp,
	src_set_cisz,
	src_set_ca,
	src_set_sa,
	src_set_la,
	src_set_pitch,
	src_set_dirty,
	src_set_clear_zbufs,
	//src_set_dirty_all,
	src_commit_write,
	src_get_ca,
	src_get_dirty,
	src_dirty_conj_mask,
	src_mgr_get_ctrl_blk,
	//src_mgr_put_ctrl_blk,
	//src_mgr_enbs_src,
	src_mgr_enb_src,
	src_mgr_dsb_src,
	src_mgr_commit_write,

	srcimp_mgr_get_ctrl_blk,
	//srcimp_mgr_put_ctrl_blk,
#if ADC_SUPP
	srcimp_mgr_set_imaparc,
	srcimp_mgr_set_imapuser,
	srcimp_mgr_set_imapnxt,
	srcimp_mgr_set_imapaddr,
	srcimp_mgr_commit_write,
#endif
	amixer_rsc_get_ctrl_blk,
	//amixer_rsc_put_ctrl_blk,
	amixer_mgr_get_ctrl_blk,
	//amixer_mgr_put_ctrl_blk,
	amixer_set_mode,
	amixer_set_iv,
	amixer_set_x,
	amixer_set_y,
	amixer_set_sadr,
	amixer_set_se,
	amixer_set_dirty,
	amixer_set_dirty_all,
	amixer_commit_write,
	amixer_get_y,
	amixer_get_dirty,
#if 0
	dai_get_ctrl_blk,
	dai_put_ctrl_blk,
	dai_srt_set_srcr,
	dai_srt_set_srcl,
	dai_srt_set_rsr,
	dai_srt_set_drat,
	dai_srt_set_ec,
	dai_srt_set_et,
	dai_commit_write,
#endif
	dao_get_ctrl_blk,
	//dao_put_ctrl_blk,
	//dao_set_spos,
	dao_commit_write,
	//dao_get_spos,

	daio_mgr_get_ctrl_blk,
	//daio_mgr_put_ctrl_blk,
	//daio_mgr_enb_dai,
	//daio_mgr_dsb_dai,
	daio_mgr_enb_dao,
	daio_mgr_dsb_dao,
	daio_mgr_dao_init,
	daio_mgr_set_imaparc,
	daio_mgr_set_imapnxt,
	daio_mgr_set_imapaddr,
	daio_mgr_commit_write,

	set_timer_irq,
	set_timer_tick,
	get_wc,
#if 1 /* vsbhda specific */
	get_timer_interrupt_pending,
	ack_interrupt,
#endif
	//io_base,
	//mem_base,
	//chip_type,
	//model,
};

/* vsbhda: this function is a bit different than the origin*/

struct hw *create_20k1_hw_obj( void )
/////////////////////////////////////
{
	struct hw *hw;

	hw = calloc(1, sizeof(struct hw));
	if (!hw)
		return 0;

	*hw = ct20k1_preset;

	return hw;
}

