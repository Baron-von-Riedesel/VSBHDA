/**************************************************************************
 * This source is based on
 * a) the Linux ALSA driver for SB X-Fi (09.2026), files
 *    ctatc.c, ctmixer.c, ctamixer.c, ctsrc.c, ctamixer.c,
 *    ctdaio.c, ctimap.c, ctresource.c, cthardware.c [+ header files]
 * b) the fragmentary MpxPlay driver code for SB X-Fi; this itself
 *    was based on ALSA driver code, probably from ~2005.
 **************************************************************************/

// currently restricted to EMU20K1 (non-titanium cards)!

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#ifndef DJGPP
#include <conio.h>
#endif
#include <errno.h>

#include "CONFIG.H"
#include "AU_CARDS.H"
#include "DMABUFF.H"
#include "PCIBIOS.H"
#include "CTHW20KX.H"

#define EMU20KX_PAGESIZE     4096
#define EMU20KX_MAXPAGES     1024

//#define EMU20KX_MAX_CHANNELS     8
//#define EMU20KX_MAX_BYTES        4

#define snd_card emu20kx_card_s

/*-----------------------------------------------------------*/
/* cthw20k1.h */

extern struct hw *create_20k1_hw_obj( void );

#if CT20K2
/* cthw20k2.h */

extern struct hw *create_20k2_hw_obj( void );
#endif

/*-----------------------------------------------------------*/
/* ctresource.h */

enum RSCTYP {
	SRC,
	SRCIMP,
	AMIXER,
	SUM,
	DAIO,
	NUM_RSCTYP	/* This must be the last one and less than 16 */
};

struct rsc_ops;

struct rsc {
	unsigned int idx:12;  /* The index of a resource */
	unsigned int type:4;  /* The type (RSCTYP) of a resource */
	unsigned int conj:12; /* Current conjugate index */
	unsigned int msr:4;   /* The Master Sample Rate a resource working on */
	void *ctrl_blk;       /* Chip specific control info block for a resource */
	struct hw *hw;        /* Chip specific object for hardware access means */
	const struct rsc_ops *ops; /* Generic resource operations */
};

struct rsc_ops {
	void (*master)(struct rsc *rsc); /* Move to master resource */
	void (*next_conj)(struct rsc *rsc); /* Move to next conjugate resource */
	int (*index)(const struct rsc *rsc); /* Return the index of resource */
	int (*output_slot)(const struct rsc *rsc); /* Return the output slot number */
};

struct rsc_mgr {
	enum RSCTYP type; /* The type (RSCTYP) of resource to manage */
	unsigned int amount; /* The total amount of a kind of resource */
	unsigned int avail; /* The amount of currently available resources */
	unsigned char *rscs; /* The bit-map for resource allocation */
	void *ctrl_blk; /* Chip specific control info block */
	struct hw *hw; /* Chip specific object for hardware access */
};

/*-----------------------------------------------------------*/
/* ctamixer.h */

/* descriptor of a summation node resource */
struct sum {
	struct rsc rsc; /* Basic resource info */
	unsigned char idx[8];
};

/* Define sum resource request description info */
struct sum_desc {
	unsigned int msr;
};

struct sum_mgr {
	struct rsc_mgr mgr;	/* Basic resource manager info */
	struct snd_card *card;	/* pointer to this card */
	//spinlock_t mgr_lock;

	 /* request one sum resource */
	int (*get_sum)(struct sum_mgr *mgr, const struct sum_desc *desc, struct sum **rsum);
	/* return one sum resource */
	//int (*put_sum)(struct sum_mgr *mgr, struct sum *sum);
};

struct amixer_rsc_ops;

struct amixer {
	struct rsc rsc; /* Basic resource info */
	unsigned char idx[8];
	struct rsc *input; /* pointer to a resource acting as source */
	struct sum *sum;   /* Put amixer output to this summation node */
	const struct amixer_rsc_ops *ops; /* AMixer specific operations */
};

struct amixer_rsc_ops {
	int (*set_input)(struct amixer *amixer, struct rsc *rsc);
	int (*set_scale)(struct amixer *amixer, unsigned int scale);
	int (*set_invalid_squash)(struct amixer *amixer, unsigned int iv);
	int (*set_sum)(struct amixer *amixer, struct sum *sum);
	int (*commit_write)(struct amixer *amixer);
	/* Only for interleaved recording */
	//int (*commit_raw_write)(struct amixer *amixer);
	int (*setup)(struct amixer *amixer, struct rsc *input, unsigned int scale, struct sum *sum);
	int (*get_scale)(struct amixer *amixer);
};

/* amixer resource request description info */
struct amixer_desc {
	unsigned int msr;
};

struct amixer_mgr {
	struct rsc_mgr mgr;	/* Basic resource manager info */
	struct snd_card *card;	/* pointer to this card */
	//spinlock_t mgr_lock;

	 /* request one amixer resource */
	int (*get_amixer)(struct amixer_mgr *mgr, const struct amixer_desc *desc, struct amixer **ramixer);
	/* return one amixer resource */
	//int (*put_amixer)(struct amixer_mgr *mgr, struct amixer *amixer);
};

/*-----------------------------------------------------------*/

/* defined in linux/list.h */

struct list_head {
	void *prev;
	void *next;
};

/*-----------------------------------------------------------*/
/* ctimap.h */

struct imapper {
	unsigned short slot; /* the id of the slot containing input data */
	unsigned short user; /* the id of the user resource consuming data */
	unsigned short addr; /* the input mapper ram id */
	unsigned short next; /* the next input mapper ram id */
	struct list_head list;
};

/*-----------------------------------------------------------*/
/* ctsrc.h */

/* SRCCTL_STATE */
#define SRC_STATE_OFF    0x0
#define SRC_STATE_INIT   0x4
#define SRC_STATE_RUN    0x5

/* sample formats */
#define SRC_SF_U8     0x0
#define SRC_SF_S16    0x1
#define SRC_SF_S24    0x2
#define SRC_SF_S32    0x3
#define SRC_SF_F32    0x4

/* descriptor of a src resource */
enum SRCMODE {
	MEMRD,		/* Read data from host memory */
	MEMWR,		/* Write data to host memory */
	ARCRW,		/* Read from and write to audio ring channel */
	NUM_SRCMODES
};

struct src_rsc_ops;

struct src {
	struct rsc rsc; /* Basic resource info */
	struct src *intlv; /* Pointer to next interleaved SRC in a series */
	const struct src_rsc_ops *ops; /* SRC specific operations */
	/* Number of contiguous srcs for interleaved usage */
	unsigned char multi;
	unsigned char mode; /* Working mode of this SRC resource */
};

struct src_rsc_ops {
	int (*set_state)(struct src *src, unsigned int state);
	int (*set_bm)(struct src *src, unsigned int bm);
	int (*set_sf)(struct src *src, unsigned int sf);
	int (*set_pm)(struct src *src, unsigned int pm);
	int (*set_rom)(struct src *src, unsigned int rom);
	int (*set_vo)(struct src *src, unsigned int vo);
	int (*set_st)(struct src *src, unsigned int st);
	int (*set_bp)(struct src *src, unsigned int bp);
	int (*set_cisz)(struct src *src, unsigned int cisz);
	int (*set_ca)(struct src *src, unsigned int ca);
	int (*set_sa)(struct src *src, unsigned int sa);
	int (*set_la)(struct src *src, unsigned int la);
	int (*set_pitch)(struct src *src, unsigned int pitch);
	int (*set_clr_zbufs)(struct src *src);
	int (*commit_write)(struct src *src);
	int (*get_ca)(struct src *src);
	int (*init)(struct src *src);
	struct src* (*next_interleave)(struct src *src);
};
/* src resource request description info */
struct src_desc {
	/* Number of contiguous master srcs for interleaved usage */
	unsigned char multi;
	unsigned char msr;
	unsigned char mode; /* Working mode of the requested srcs */
};

/* Define src manager object */
struct src_mgr {
	struct rsc_mgr mgr;	/* Basic resource manager info */
	struct snd_card *card;	/* pointer to this card */
	//spinlock_t mgr_lock;

	 /* request src resource */
	int (*get_src)(struct src_mgr *mgr, const struct src_desc *desc, struct src **rsrc);
	/* return src resource */
	//int (*put_src)(struct src_mgr *mgr, struct src *src);
	int (*src_enable_s)(struct src_mgr *mgr, struct src *src);
	int (*src_enable)(struct src_mgr *mgr, struct src *src);
	int (*src_disable)(struct src_mgr *mgr, struct src *src);
	int (*commit_write)(struct src_mgr *mgr);
};

#if ADC_SUPP

/* Define the descriptor of a SRC Input Mapper resource */

struct srcimp_mgr;
struct srcimp_rsc_ops;

struct srcimp {
	struct rsc rsc;
	unsigned char idx[8];
	unsigned int mapped; /* A bit-map indicating which conj rsc is mapped */
	struct srcimp_mgr *mgr;
	const struct srcimp_rsc_ops *ops;
	struct imapper imappers[];
};

struct srcimp_rsc_ops {
	int (*map)(struct srcimp *srcimp, struct src *user, struct rsc *input);
	int (*unmap)(struct srcimp *srcimp);
};

/* Define SRCIMP resource request description info */
struct srcimp_desc {
	unsigned int msr;
};

struct srcimp_mgr {
	struct rsc_mgr mgr;	/* Basic resource manager info */
	struct snd_card *card;	/* pointer to this card */
	//spinlock_t mgr_lock;
	//spinlock_t imap_lock;
	struct list_head imappers;
	struct imapper *init_imap;
	unsigned int init_imap_added;

	 /* request srcimp resource */
	int (*get_srcimp)(struct srcimp_mgr *mgr, const struct srcimp_desc *desc, struct srcimp **rsrcimp);
	/* return srcimp resource */
	//int (*put_srcimp)(struct srcimp_mgr *mgr, struct srcimp *srcimp);
	int (*imap_add)(struct srcimp_mgr *mgr, struct imapper *entry);
	int (*imap_delete)(struct srcimp_mgr *mgr, struct imapper *entry);
};

#endif

/*-----------------------------------------------------------*/
/* ctdaio.h */

/* Define the descriptor of a daio resource */
enum DAIOTYP {
	LINEO1,
	//LINEO2,
	//LINEO3,
	//LINEO4,
	//SPDIFOO,	/* S/PDIF Out (Flexijack/Optical) */
#if ADC_SUPP
	LINEIM,
#endif
	//SPDIFIO,	/* S/PDIF In (Flexijack/Optical) on the card */
	//MIC,		/* Dedicated mic on Titanium HD */
	//RCA,		/* Dedicated RCA on SE-300PCIE */
	//SPDIFI_BAY,	/* S/PDIF In on internal drive bay */
	NUM_DAIOTYP
};

struct daio {
	struct rsc rscl;	/* Basic resource info for left TX/RX */
	struct rsc rscr;	/* Basic resource info for right TX/RX */
	enum DAIOTYP type;
	unsigned char output;
};

struct dao_rsc_ops;
struct daio_mgr;

struct dao {
	struct daio daio;
	const struct dao_rsc_ops *ops;	/* DAO specific operations */
	struct imapper **imappers;
	struct daio_mgr *mgr;
	struct hw *hw;
	void *ctrl_blk;
};

#if ADC_SUPP
struct dai {
	struct daio daio;
	const struct dai_rsc_ops *ops;	/* DAI specific operations */
	struct hw *hw;
	void *ctrl_blk;
};
#endif

struct dao_desc {
	unsigned int msr:4;
	unsigned int passthru:1;
};

struct dao_rsc_ops {
	int (*set_spos)(struct dao *dao, unsigned int spos);
	int (*commit_write)(struct dao *dao);
	int (*get_spos)(struct dao *dao, unsigned int *spos);
	//int (*reinit)(struct dao *dao, const struct dao_desc *desc);
	int (*set_left_input)(struct dao *dao, struct rsc *input);
	int (*set_right_input)(struct dao *dao, struct rsc *input);
	//int (*clear_left_input)(struct dao *dao);
	//int (*clear_right_input)(struct dao *dao);
};

#if ADC_SUPP
struct dai_rsc_ops {
	int (*set_srt_srcl)(struct dai *dai, struct rsc *src);
	int (*set_srt_srcr)(struct dai *dai, struct rsc *src);
	int (*set_srt_msr)(struct dai *dai, unsigned int msr);
	int (*set_enb_src)(struct dai *dai, unsigned int enb);
	int (*set_enb_srt)(struct dai *dai, unsigned int enb);
	int (*commit_write)(struct dai *dai);
};
#endif

/* Define daio resource request description info */
struct daio_desc {
	unsigned int type:4;
	unsigned int msr:4;
	unsigned int passthru:1;
	unsigned int output:1;
};

struct daio_mgr {
	struct rsc_mgr mgr;	/* Basic resource manager info */
	struct snd_card *card;	/* pointer to this card */
	//spinlock_t mgr_lock;
	//spinlock_t imap_lock;
	struct list_head imappers;
	struct imapper *init_imap;
	unsigned int init_imap_added;

	 /* request one daio resource */
	int (*get_daio)(struct daio_mgr *mgr, const struct daio_desc *desc, struct daio **rdaio);
	/* return one daio resource */
	//int (*put_daio)(struct daio_mgr *mgr, struct daio *daio);
	int (*daio_enable)(struct daio_mgr *mgr, struct daio *daio);
	//int (*daio_disable)(struct daio_mgr *mgr, struct daio *daio);
	int (*imap_add)(struct daio_mgr *mgr, struct imapper *entry);
	int (*imap_delete)(struct daio_mgr *mgr, struct imapper *entry);
	int (*commit_write)(struct daio_mgr *mgr);
};

/*-----------------------------------------------------------*/
/* ctmixer.h */
#define INIT_VOL    0x1c00

enum MIXER_PORT_T {
 MIX_WAVE_FRONT,
 //MIX_WAVE_REAR,
 //MIX_WAVE_CENTLFE,
 //MIX_WAVE_SURROUND,
 //MIX_SPDIF_OUT,
 MIX_PCMO_FRONT,
#if ADC_SUPP
 MIX_MIC_IN,
 MIX_LINE_IN,
#endif
 //MIX_SPDIF_IN,
 MIX_PCMI_FRONT,
 //MIX_PCMI_REAR,
 //MIX_PCMI_CENTLFE,
 //MIX_PCMI_SURROUND,
 NUM_MIX_PORTS
};

struct emu20kx_card_s;

struct ct_mixer {
	struct emu20kx_card_s *card;

	struct sum **sums;		/* sum resources for signal collection */
	//struct snd_kcontrol *line_mic_kctls[2]; /* line/mic capture switch controls */
	unsigned int switch_state; /* A bit-map to indicate state of switches */

	int (*get_output_ports)(struct ct_mixer *mixer, enum MIXER_PORT_T type, struct rsc **rleft, struct rsc **rright);
	int (*set_input_left)(struct ct_mixer *mixer, enum MIXER_PORT_T type, struct rsc *rsc);
	int (*set_input_right)(struct ct_mixer *mixer, enum MIXER_PORT_T type, struct rsc *rsc);
	struct amixer *amixers[];		/* amixer resources for volume control */
};

/* ----------------------------------------------------- */
/* ctatc.h */

enum CTALSADEVS {		/* Types of alsa devices */
	FRONT,
	//SURROUND,
	//CLFE,
	//SIDE,
	//IEC958,
	//MIXER,
	NUM_CTALSADEVS		/* This should always be the last */
};

/* ----------------------------------------------------- */

/* no of SRC in atc_get_resources */
//#define NUM_ATC_SRCS 6
/* index 0+1: SPDIF_IN
 * index 2+3: LINEIN
 * index 4+5: MIC
 */
#if ADC_SUPP
#define NUM_ATC_SRCS 6 /* support LINE_IN and MIC */
#else
#define NUM_ATC_SRCS 2 /* todo: check if those 2 are needed */
#endif
/* no of SUM in atc_get_resources */
//#define NUM_ATC_PCM (2 * 4) /* the 4 were for F/R/S/C */
#define NUM_ATC_PCM (2 * 1) /* just F(ront) is used currently */

/* ----------------------------------------------------- */

struct emu20kx_card_s
{
 //unsigned int    iobase;
 unsigned int    subsys_id;
 struct pci_config_s  pci_dev;

 struct cardmem_s dm;
 char   *pcmout_buffer;
 int    pcmout_bufsize;
 uint32_t *virtualpagetable;
 void   *silentpage;
 struct hw *hw;
 struct rsc_mgr    *rsc_mgrs[NUM_RSCTYP];
 struct daio       *daios[NUM_DAIOTYP];
#if NUM_ATC_SRCS
 struct src        *srcs[NUM_ATC_SRCS];
#endif
#if ADC_SUPP
 struct srcimp     *srcimps[NUM_ATC_SRCS];
#endif
 struct sum        *pcm[NUM_ATC_PCM];
 struct ct_mixer   *mixer;
 struct src        *apcm_src;
 struct amixer     *apcm_amixer[2];
 unsigned int rsr; /* reference sampling rate (44100 or 48000) - argument for card_init() */
 unsigned int msr; /* multiply of rsr (1,2,4) - argument for card_init() */
};

/*-----------------------------------------------------------*/

/* ctresource.c */

#define AUDIO_SLOT_BLOCK_NUM 256

static const unsigned char offset_in_audio_slot_block[NUM_RSCTYP] = {
	/* SRC channel is at Audio Ring slot 1 every 16 slots. */
	[SRC]     = 0x1,
	[AMIXER]  = 0x4,
	[SUM]     = 0xc,
};

/* Resource allocation based on bit-map management mechanism */

static int get_resource(unsigned char *rscs, unsigned int amount, unsigned int multi, unsigned int *ridx)
/////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	int i, j, k, n;

	/* Check whether there are sufficient resources to meet request. */
	for (i = 0, n = multi; i < amount; i++) {
		j = i / 8;
		k = i % 8;
		if (rscs[j] & ((unsigned char)1 << k)) {
			n = multi;
			continue;
		}
		if (!(--n))
			break; /* found sufficient contiguous resources */
	}

	if (i >= amount) {
		/* Can not find sufficient contiguous resources */
		return -ENOENT;
	}

	/* Mark the contiguous bits in resource bit-map as used */
	for (n = multi; n > 0; n--) {
		j = i / 8;
		k = i % 8;
		rscs[j] |= ((unsigned char)1 << k);
		i--;
	}

	*ridx = i + 1;

	return 0;
}

static int mgr_get_resource(struct rsc_mgr *mgr, unsigned int n, unsigned int *ridx)
////////////////////////////////////////////////////////////////////////////////////
{
	int err;

	if (n > mgr->avail)
		return -ENOENT;

	err = get_resource(mgr->rscs, mgr->amount, n, ridx);
	if (!err)
		mgr->avail -= n;

	return err;
}

static int rsc_index(const struct rsc *rsc)
{
    return rsc->conj;
}

static int audio_ring_slot(const struct rsc *rsc)
{
    return (rsc->conj << 4) + offset_in_audio_slot_block[rsc->type];
}

static void rsc_next_conj(struct rsc *rsc)
{
	unsigned int i;
	for (i = 0; (i < 8) && (!(rsc->msr & (0x1 << i))); )
		i++;
	rsc->conj += (AUDIO_SLOT_BLOCK_NUM >> i);
}

static void rsc_master(struct rsc *rsc)
{
	rsc->conj = rsc->idx;
}

static const struct rsc_ops rsc_generic_ops = {
	rsc_master,
	rsc_next_conj,
	rsc_index,
	audio_ring_slot,
};

#ifdef _DEBUG
char *getrsctype( enum RSCTYP type )
{
	static char *rscn[] = { "SRC", "SRCIMP","AMIXER","SUM","DAIO"};
	return rscn[type];
}

#include <string.h>

char *getrsctypeX( struct rsc *rsc )
{
	static char rscstr[32];
	if ( rsc ) {
		int i;
		strcpy( rscstr, getrsctype(rsc->type));
		i = strlen( rscstr );
		*(rscstr + i++) = '(';
		if ( rsc->idx > 9 )
			*(rscstr + i++) = (char)(rsc->idx/10) + '0';
		*(rscstr + i++) = (char)(rsc->idx%10) + '0';
		*(rscstr + i++) = ')';
		*(rscstr + i) = '\0';
		return rscstr;
	}
	return "NULL";
}
#endif

static int rsc_init(struct rsc *rsc, unsigned idx, enum RSCTYP type, unsigned msr, struct hw *hw)
/////////////////////////////////////////////////////////////////////////////////////////////////
{
	int err = 0;

	dbgprintf(("rsc_init(%X, idx=%u, type=%u(%s), msr=%u)\n", rsc, idx, type, getrsctype(type), msr));
	rsc->idx = idx;
	rsc->conj = idx;
	rsc->type = type;
	rsc->msr = msr;
	rsc->hw = hw;
	rsc->ops = &rsc_generic_ops;
	if (!hw) {
		rsc->ctrl_blk = NULL;
		return 0;
	}

	switch (type) {
	case SRC:
		err = hw->src_rsc_get_ctrl_blk(&rsc->ctrl_blk);
		break;
	case AMIXER:
		err = hw->amixer_rsc_get_ctrl_blk(&rsc->ctrl_blk);
		break;
	case SRCIMP:
	case SUM:
	case DAIO:
		break;
	default:
		dbgprintf(( "rsc_init: invalid resource type value %d!\n", type));
		return -1;
	}

	if (err) {
		dbgprintf(("rsc_init: failed to get resource control block!\n"));
		return err;
	}

	return 0;
}

static int rsc_mgr_init(struct rsc_mgr *mgr, enum RSCTYP type, unsigned int amount, struct hw *hw)
//////////////////////////////////////////////////////////////////////////////////////////////////
{
	int err = 0;

	mgr->type = NUM_RSCTYP;

	mgr->rscs = calloc(1, (amount + 7) / 8);  /* bitmap */
	if (!mgr->rscs)
		return -ENOMEM;

	switch (type) {
	case SRC:
		err = hw->src_mgr_get_ctrl_blk(&mgr->ctrl_blk);
		break;
	case SRCIMP:
		err = hw->srcimp_mgr_get_ctrl_blk(&mgr->ctrl_blk);
		break;
	case AMIXER:
		err = hw->amixer_mgr_get_ctrl_blk(&mgr->ctrl_blk);
		break;
	case DAIO:
		err = hw->daio_mgr_get_ctrl_blk(hw, &mgr->ctrl_blk);
		break;
	case SUM:
		break;
	default:
		dbgprintf(("rsc_mgr_init: Invalid resource type value %d!\n", type));
		err = -EINVAL;
		goto error;
	}

	if (err) {
		dbgprintf(("rsc_mgr: Failed to get manager control block!\n"));
		goto error;
	}

	mgr->type = type;
	mgr->avail = mgr->amount = amount;
	mgr->hw = hw;

	return 0;

error:
	//kfree(mgr->rscs);
	return err;
}

/*-----------------------------------------------------------*/
/* list_head functions - defined in linux headers */

#if 0 /* OW has problems with gcc container_of() macro */
#define container_of(ptr, type, member) ({ \
	typeof( ((type*)0)->member ) \
	* __mptr = ((void*)(ptr)); \
	(type*)( (char*)__mptr - \
	offsetof(type, member) ); \
	})
#else
static inline void *get_container( void *pv, int ofs ) { return (void *)((char *)pv - ofs); }
#define container_of(ptr, type, member) ((type *)get_container( ptr, offsetof( type, member ) ) )
#endif

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
	struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

static inline void __list_add(struct list_head *new, struct list_head *prev, struct list_head *next)
{
	//if (!__list_add_valid(new, prev, next))
	//	return;

	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

static inline int list_empty(const struct list_head *head)
{
	return (head->next == head );
}

static inline int list_is_head(const struct list_head *list, const struct list_head *head)
{
	return list == head;
}

#define list_for_each(pos, head) \
	for (pos = (head)->next; !list_is_head(pos, (head)); pos = pos->next)

#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

/*-----------------------------------------------------------*/
/* ctimap.c */

static int input_mapper_add(struct list_head *mappers, struct imapper *entry, int (*map_op)(void *, struct imapper *), void *data)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	struct list_head *pos, *pre, *head;
	struct imapper *pre_ent, *pos_ent;

	head = mappers;

	dbgprintf(("input_mapper_add(%X, entry=%X, map_op=%X, data=%X)\n", mappers, entry, map_op, data));
	if (list_empty(head)) {
		entry->next = entry->addr;
		map_op(data, entry);
		list_add(&entry->list, head);
		return 0;
	}

	list_for_each(pos, head) {
		pos_ent = list_entry(pos, struct imapper, list);
		if (pos_ent->slot > entry->slot) {
			/* found a position in list */
			break;
		}
	}

	if (pos != head) {
		pre = pos->prev;
		if (pre == head)
			pre = head->prev;

		__list_add(&entry->list, pos->prev, pos);
	} else {
		pre = head->prev;
		pos = head->next;
		list_add_tail(&entry->list, head);
	}

	pre_ent = list_entry(pre, struct imapper, list);
	pos_ent = list_entry(pos, struct imapper, list);

	entry->next = pos_ent->addr;
	map_op(data, entry);
	pre_ent->next = entry->addr;
	map_op(data, pre_ent);

	return 0;
}

static int input_mapper_delete(struct list_head *mappers, struct imapper *entry, int (*map_op)(void *, struct imapper *), void *data)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	struct list_head *next, *pre, *head;
	struct imapper *pre_ent, *next_ent;

	head = mappers;

	dbgprintf(("input_mapper_delete(%X, %X, %X, %X) head.prev/next=%X/%X\n", mappers, entry, map_op, data, head->prev, head->next));
	if (list_empty(head))
		return 0;

	pre = (entry->list.prev == head) ? head->prev : entry->list.prev;
	next = (entry->list.next == head) ? head->next : entry->list.next;

	if (pre == &entry->list) {
		/* entry is the only one node in mappers list */
		entry->next = entry->addr = entry->user = entry->slot = 0;
		map_op(data, entry);
		list_del(&entry->list);
		return 0;
	}

	pre_ent = list_entry(pre, struct imapper, list);
	next_ent = list_entry(next, struct imapper, list);

	pre_ent->next = next_ent->addr;
	map_op(data, pre_ent);
	list_del(&entry->list);

	return 0;
}

/*-----------------------------------------------------------*/
/* ctdaio.c */

struct daio_usage {
	unsigned short data;
};

struct daio_rsc_idx {
	unsigned short left;
	unsigned short right;
};

static const struct daio_rsc_idx idx_20k1[NUM_DAIOTYP] = {
	[LINEO1] = {.left = 0x00, .right = 0x01},
	//[LINEO2] = {.left = 0x18, .right = 0x19},
	//[LINEO3] = {.left = 0x08, .right = 0x09},
	//[LINEO4] = {.left = 0x10, .right = 0x11},
#if ADC_SUPP
	[LINEIM]   = {.left = 0x1b5, .right = 0x1bd},
#endif
	//[SPDIFOO] = {.left = 0x20, .right = 0x21},
	//[SPDIFIO] = {.left = 0x15, .right = 0x1d},
	//[SPDIFI_BAY] = {.left = 0x95, .right = 0x9d},
};

#if CT20K2
static const struct daio_rsc_idx idx_20k2[NUM_DAIOTYP] = {
	[LINEO1] = {.left = 0x40, .right = 0x41},
	//[LINEO2] = {.left = 0x60, .right = 0x61},
	//[LINEO3] = {.left = 0x50, .right = 0x51},
	//[LINEO4] = {.left = 0x70, .right = 0x71},
# if ADC_SUPP
	[LINEIM] = {.left = 0x45, .right = 0xc5},
	[MIC]    = {.left = 0x55, .right = 0xd5},
# endif
	//[RCA]    = {.left = 0x30, .right = 0x31},
	//[SPDIFOO] = {.left = 0x00, .right = 0x01},
	//[SPDIFIO] = {.left = 0x05, .right = 0x85},
};

#endif

static void daio_master(struct rsc *rsc)
{
	/* Actually, this is not the resource index of DAIO.
	 * For DAO, it is the input mapper index. And, for DAI,
	 * it is the output time-slot index. */
	rsc->conj = rsc->idx;
}

static int daio_index(const struct rsc *rsc)
{
	return rsc->conj;
}

static void daio_out_next_conj(struct rsc *rsc)
{
	rsc->conj += 2;
}

static const struct rsc_ops daio_out_rsc_ops = {
	.master = daio_master,
	.next_conj = daio_out_next_conj,
	.index = daio_index,
	.output_slot = NULL,
};

#if ADC_SUPP

static void daio_in_next_conj_20k1(struct rsc *rsc)
{
	rsc->conj += 0x200;
}

static const struct rsc_ops daio_in_rsc_ops_20k1 = {
	.master = daio_master,
	.next_conj = daio_in_next_conj_20k1,
	.index = NULL,
	.output_slot = daio_index,
};
#endif

/* translate DAIO type to index;
 * the index is used as an argument for hw!
 * seems that index 0-3 are SPDIF, while 4-7 are I2S...
 */

static int daio_device_index(enum DAIOTYP type, struct hw *hw)
{
	switch (hw->chip_type) {
	case ATC20K1:
		switch (type) {
		//case SPDIFOO: return 0;
		//case SPDIFIO: return 0;
		//case SPDIFI_BAY: return 1;
		case LINEO1: return 4;
		//case LINEO2: return 7;
		//case LINEO3: return 5;
		//case LINEO4: return 6;
#if ADC_SUPP
		case LINEIM: return 7;
#endif
		default:
			dbgprintf(("daio_device_index: Invalid type %d for CA20K1\n", type));
			return -EINVAL;
		}
#if CT20K2
	case ATC20K2:
		switch (type) {
		//case SPDIFOO: return 0;
		//case SPDIFIO: return 0;
		case LINEO1: return 4;
		//case LINEO2: return 7;
		//case LINEO3: return 5;
		//case LINEO4: return 6;
#if ADC_SUPP
		case LINEIM: return 4;
		case MIC: return 5;
#endif
		//case RCA: return 3;
		default:
			dbgprintf(("daio_device_index: Invalid type %d for CA20K2\n", type));
			return -EINVAL;
		}
#endif
	default:
		dbgprintf(("daio_device_index: Invalid chip type %d\n", hw->chip_type));
		return -EINVAL;
	}
}

static int dao_commit_write(struct dao *dao)
{
	int idx = daio_device_index(dao->daio.type, dao->hw);

	if (idx < 0)
		return idx;
	dao->hw->dao_commit_write(dao->hw, idx, dao->ctrl_blk);
	return 0;
}

static int dao_set_left_input(struct dao *dao, struct rsc *input)
{
	struct imapper *entry;
	struct daio *daio = &dao->daio;
	int i;

	dbgprintf(("dao_set_left_input(%s", getrsctypeX(&dao->daio.rscl))); dbgprintf((" input=%s): msr=%d\n", getrsctypeX(input), daio->rscl.msr));
	entry = calloc(1, sizeof( struct imapper) * daio->rscl.msr);
	if (!entry) {
		dbgprintf(("dao_set_left_input: calloc() failed\n"));
		return -ENOMEM;
	}

	//dao->ops->clear_left_input(dao);
	/* Program master and conjugate resources */
	input->ops->master(input);
	daio->rscl.ops->master(&daio->rscl);
	for (i = 0; i < daio->rscl.msr; i++, entry++) {
		entry->slot = input->ops->output_slot(input);
		entry->user = entry->addr = daio->rscl.ops->index(&daio->rscl);
		dao->mgr->imap_add(dao->mgr, entry);
		dao->imappers[i] = entry;
		dbgprintf(("dao_set_left_input: entry slot=%u user=%u addr=%u next=%u\n", entry->slot, entry->user, entry->addr, entry->next ));

		input->ops->next_conj(input);
		daio->rscl.ops->next_conj(&daio->rscl);
	}
	input->ops->master(input);
	daio->rscl.ops->master(&daio->rscl);

	return 0;
}

static int dao_set_right_input(struct dao *dao, struct rsc *input)
{
	struct imapper *entry;
	struct daio *daio = &dao->daio;
	int i;

	dbgprintf(("dao_set_right_input(%s", getrsctypeX(&dao->daio.rscr))); dbgprintf((" input=%s): msr=%d\n", getrsctypeX(input), daio->rscl.msr));
	entry = calloc(1, sizeof( struct imapper) * daio->rscr.msr);
	if (!entry)
		return -ENOMEM;

	//dao->ops->clear_right_input(dao);
	/* Program master and conjugate resources */
	input->ops->master(input);
	daio->rscr.ops->master(&daio->rscr);
	for (i = 0; i < daio->rscr.msr; i++, entry++) {
		entry->slot = input->ops->output_slot(input);
		entry->user = entry->addr = daio->rscr.ops->index(&daio->rscr);
		dao->mgr->imap_add(dao->mgr, entry);
		dao->imappers[daio->rscl.msr + i] = entry;
		dbgprintf(("dao_set_right_input: entry slot=%u user=%u addr=%u next=%u\n", entry->slot, entry->user, entry->addr, entry->next ));

		input->ops->next_conj(input);
		daio->rscr.ops->next_conj(&daio->rscr);
	}
	input->ops->master(input);
	daio->rscr.ops->master(&daio->rscr);

	return 0;
}

static const struct dao_rsc_ops dao_ops = {
	//.set_spos = dao_spdif_set_spos,
	.commit_write = dao_commit_write,
	//.get_spos = dao_spdif_get_spos,
	//.reinit = dao_rsc_reinit,
	.set_left_input = dao_set_left_input,
	.set_right_input = dao_set_right_input,
	//.clear_left_input = dao_clear_left_input,
	//.clear_right_input = dao_clear_right_input,
};

static int daio_mgr_get_rsc(struct rsc_mgr *mgr, enum DAIOTYP type)
{
	if (((struct daio_usage *)mgr->rscs)->data & (0x1 << type))
		return -ENOENT;

	((struct daio_usage *)mgr->rscs)->data |= (0x1 << type);

	return 0;
}

#if ADC_SUPP
static int dai_set_srt_srcl(struct dai *dai, struct rsc *src)
{
	src->ops->master(src);
	dai->hw->dai_srt_set_srcm(dai->ctrl_blk, src->ops->index(src));
	return 0;
}

static int dai_set_srt_srcr(struct dai *dai, struct rsc *src)
{
	src->ops->master(src);
	dai->hw->dai_srt_set_srco(dai->ctrl_blk, src->ops->index(src));
	return 0;
}

static int dai_set_srt_msr(struct dai *dai, unsigned int msr)
{
	unsigned int rsr;

	for (rsr = 0; msr > 1; msr >>= 1)
		rsr++;

	dai->hw->dai_srt_set_rsr(dai->ctrl_blk, rsr);
	return 0;
}

static int dai_set_enb_src(struct dai *dai, unsigned int enb)
{
	dai->hw->dai_srt_set_ec(dai->ctrl_blk, enb);
	return 0;
}

static int dai_set_enb_srt(struct dai *dai, unsigned int enb)
{
	dai->hw->dai_srt_set_et(dai->ctrl_blk, enb);
	return 0;
}

static int dai_commit_write(struct dai *dai)
{
	int idx = daio_device_index(dai->daio.type, dai->hw);

	if (idx < 0)
		return idx;
	dai->hw->dai_commit_write(dai->hw, idx, dai->ctrl_blk);
	return 0;
}

static const struct dai_rsc_ops dai_ops = {
	.set_srt_srcl = dai_set_srt_srcl,
	.set_srt_srcr = dai_set_srt_srcr,
	.set_srt_msr = dai_set_srt_msr,
	.set_enb_src = dai_set_enb_src,
	.set_enb_srt = dai_set_enb_srt,
	.commit_write = dai_commit_write,
};
#endif

static int daio_rsc_init(struct daio *daio, const struct daio_desc *desc, struct hw *hw)
////////////////////////////////////////////////////////////////////////////////////////
{
	int err;
	unsigned int idx_l, idx_r;

	switch (hw->chip_type) {
	case ATC20K1:
		idx_l = idx_20k1[desc->type].left;
		idx_r = idx_20k1[desc->type].right;
		break;
#if CT20K2
	case ATC20K2:
		idx_l = idx_20k2[desc->type].left;
		idx_r = idx_20k2[desc->type].right;
		break;
#endif
	default:
		return -EINVAL;
	}
	err = rsc_init(&daio->rscl, idx_l, DAIO, desc->msr, hw);
	if (err)
		return err;

	err = rsc_init(&daio->rscr, idx_r, DAIO, desc->msr, hw);
	if (err)
		goto error1;

	/* Set daio->rscl/r->ops to daio specific ones */
	if (desc->output) {
		daio->rscl.ops = daio->rscr.ops = &daio_out_rsc_ops;
	} else {
		dbgprintf(("daio_rsc_init: ERROR, unexpected desc->output=0\n"));
#if ADC_SUPP
		switch (hw->chip_type) {
		case ATC20K1:
			daio->rscl.ops = daio->rscr.ops = &daio_in_rsc_ops_20k1;
			break;
# if CT20K2
		case ATC20K2:
			daio->rscl.ops = daio->rscr.ops = &daio_in_rsc_ops_20k2;
			break;
# endif
		default:
			break;
		}
#endif
	}
	daio->type = desc->type;
	daio->output = desc->output;

	return 0;

error1:
	//rsc_uninit(&daio->rscl);
	return err;
}

static int dao_rsc_init(struct dao *dao, const struct daio_desc *desc, struct daio_mgr *mgr)
////////////////////////////////////////////////////////////////////////////////////////////
{
	struct hw *hw = mgr->mgr.hw;
	unsigned int conf;
	int idx, err;

	err = daio_rsc_init(&dao->daio, desc, mgr->mgr.hw);
	if (err)
		return err;

	//dao->imappers = kzalloc(array3_size(sizeof(void *), desc->msr, 2), GFP_KERNEL);
	dao->imappers = calloc(1, sizeof(void *) * 3 * desc->msr * 2);
	if (!dao->imappers) {
		err = -ENOMEM;
		goto error1;
	}

	dao->ops = &dao_ops;
	dao->mgr = mgr;
	dao->hw = hw;
	err = hw->dao_get_ctrl_blk(&dao->ctrl_blk);
	if (err)
		goto error2;

	idx = daio_device_index(dao->daio.type, hw);
	if (idx < 0) {
		err = idx;
		goto error2;
	}

	hw->daio_mgr_dsb_dao(mgr->mgr.ctrl_blk, idx);
	hw->daio_mgr_commit_write(hw, mgr->mgr.ctrl_blk);

	conf = (desc->msr & 0x7) | (desc->passthru << 3);
	hw->daio_mgr_dao_init(hw, mgr->mgr.ctrl_blk, idx, conf);
	hw->daio_mgr_enb_dao(mgr->mgr.ctrl_blk, idx);
	hw->daio_mgr_commit_write(hw, mgr->mgr.ctrl_blk);

	return 0;

error2:
	//kfree(dao->imappers);
	dao->imappers = NULL;
error1:
	//daio_rsc_uninit(&dao->daio);
	return err;
}

#if ADC_SUPP
static int dai_rsc_init(struct dai *dai, const struct daio_desc *desc, struct daio_mgr *mgr)
{
	int idx, err;
	struct hw *hw = mgr->mgr.hw;
	unsigned int rsr, msr;

	err = daio_rsc_init(&dai->daio, desc, mgr->mgr.hw);
	if (err)
		return err;

	dai->ops = &dai_ops;
	dai->hw = mgr->mgr.hw;
	err = hw->dai_get_ctrl_blk(&dai->ctrl_blk);
	if (err)
		goto error1;

	idx = daio_device_index(dai->daio.type, dai->hw);
	if (idx < 0) {
		err = idx;
		goto error1;
	}

	for (rsr = 0, msr = desc->msr; msr > 1; msr >>= 1)
		rsr++;

	hw->dai_srt_set_rsr(dai->ctrl_blk, rsr);
	hw->dai_srt_set_drat(dai->ctrl_blk, 0);
	/* default to disabling control of a SRC */
	hw->dai_srt_set_ec(dai->ctrl_blk, 0);
	hw->dai_srt_set_et(dai->ctrl_blk, 0); /* default to disabling SRT */
	hw->dai_commit_write(hw, idx, dai->ctrl_blk);

	return 0;

error1:
	//daio_rsc_uninit(&dai->daio);
	return err;
}
#endif

static int get_daio_rsc(struct daio_mgr *mgr, const struct daio_desc *desc, struct daio **rdaio)
////////////////////////////////////////////////////////////////////////////////////////////////
{
	int err;

	*rdaio = NULL;

	/* Check whether there are sufficient daio resources to meet request. */
	if (err = daio_mgr_get_rsc(&mgr->mgr, desc->type))
		goto error;

	err = -ENOMEM;
	/* Allocate mem for daio resource */
	if (desc->output) {
		struct dao *dao = calloc(1, sizeof(struct dao));
		if (!dao)
			goto error;

		if (err = dao_rsc_init(dao, desc, mgr))
			goto error;

		*rdaio = &dao->daio;
	} else {
#if ADC_SUPP
		struct dai *dai = calloc(1, sizeof(struct dai));
		if (!dai)
			goto error;

		if (err = dai_rsc_init(dai, desc, mgr))
			goto error;

		*rdaio = &dai->daio;
#else
		dbgprintf(("daiomgr.get_daio: ERROR - DAI not implemented!\n"));
		err = -1;
		goto error;
#endif
	}

	mgr->daio_enable(mgr, *rdaio);
	mgr->commit_write(mgr);
	dbgprintf(("daiomgr.get_daio(mgr=%X, desc.output=%u, dst=%X)=%X\n", mgr, desc->output, rdaio, *rdaio));

	return 0;

error:
	//	daio_mgr_put_rsc(&mgr->mgr, desc->type);
	dbgprintf(("daiomgr.get_daio(mgr=%X): ERROR %d\n", mgr, err));
	return err;
}

static int daio_mgr_enb_daio(struct daio_mgr *mgr, struct daio *daio)
{
	struct hw *hw = mgr->mgr.hw;
	int idx = daio_device_index(daio->type, hw);

	if (idx < 0)
		return idx;
	if (daio->output)
		hw->daio_mgr_enb_dao(mgr->mgr.ctrl_blk, idx);
	else {
#if ADC_SUPP
		hw->daio_mgr_enb_dai(mgr->mgr.ctrl_blk, idx);
#else
		dbgprintf(("daio_mgr_enb_daio: ERROR - DAI not implemented!\n"));
#endif
	}
	return 0;
}

static int daio_map_op(void *data, struct imapper *entry)
{
	struct rsc_mgr *mgr = &((struct daio_mgr *)data)->mgr;
	struct hw *hw = mgr->hw;

	hw->daio_mgr_set_imaparc(mgr->ctrl_blk, entry->slot);
	hw->daio_mgr_set_imapnxt(mgr->ctrl_blk, entry->next);
	hw->daio_mgr_set_imapaddr(mgr->ctrl_blk, entry->addr);
	hw->daio_mgr_commit_write(mgr->hw, mgr->ctrl_blk);

	return 0;
}

static int daio_imap_add(struct daio_mgr *mgr, struct imapper *entry)
{
	dbgprintf(("daio_imap_add(mgr=%X, entry=%X)\n", mgr, entry));
	if (!entry->addr && mgr->init_imap_added) {
		input_mapper_delete(&mgr->imappers, mgr->init_imap, daio_map_op, mgr);
		mgr->init_imap_added = 0;
	}
	return input_mapper_add(&mgr->imappers, entry, daio_map_op, mgr);
}

static int daio_mgr_commit_write(struct daio_mgr *mgr)
{
	struct hw *hw = mgr->mgr.hw;

	hw->daio_mgr_commit_write(hw, mgr->mgr.ctrl_blk);
	return 0;
}

static int daio_mgr_create(struct hw *hw, void **rdaio_mgr)
///////////////////////////////////////////////////////////
{
	int err, i;
	struct daio_mgr *daio_mgr;
	struct imapper *entry;

	*rdaio_mgr = NULL;
	daio_mgr = calloc(1, sizeof(struct daio_mgr));
	if (!daio_mgr)
		return -ENOMEM;

	err = rsc_mgr_init(&daio_mgr->mgr, DAIO, NUM_DAIOTYP, hw);
	if (err)
		goto error1;

	//spin_lock_init(&daio_mgr->mgr_lock);
	//spin_lock_init(&daio_mgr->imap_lock);

	INIT_LIST_HEAD(&daio_mgr->imappers);

	entry = calloc(1, sizeof( struct imapper ));
	if (!entry) {
		err = -ENOMEM;
		goto error2;
	}
	entry->slot = entry->addr = entry->next = entry->user = 0;
	list_add(&entry->list, &daio_mgr->imappers);
	daio_mgr->init_imap = entry;
	daio_mgr->init_imap_added = 1;

	daio_mgr->get_daio = get_daio_rsc;
	//daio_mgr->put_daio = put_daio_rsc;
	daio_mgr->daio_enable = daio_mgr_enb_daio;
	//daio_mgr->daio_disable = daio_mgr_dsb_daio;
	daio_mgr->imap_add = daio_imap_add;
	//daio_mgr->imap_delete = daio_imap_delete;
	daio_mgr->commit_write = daio_mgr_commit_write;
	daio_mgr->card = hw->card;

	for (i = 0; i < 8; i++) {
		hw->daio_mgr_dsb_dao(daio_mgr->mgr.ctrl_blk, i);
		//hw->daio_mgr_dsb_dai(daio_mgr->mgr.ctrl_blk, i);
	}
	hw->daio_mgr_commit_write(hw, daio_mgr->mgr.ctrl_blk);

	*rdaio_mgr = daio_mgr;

	return 0;

error2:
	//rsc_mgr_uninit(&daio_mgr->mgr);
error1:
	//kfree(daio_mgr);
	return err;
}

/*-----------------------------------------------------------*/

/* ctsrc.c */

#define SRC_RESOURCE_NUM 256
#define SRCIMP_RESOURCE_NUM 256

static int src_default_config_memrd(struct src *src);
static int src_default_config_memwr(struct src *src);
static int src_default_config_arcrw(struct src *src);

static int (*src_default_config[3])(struct src *) = {
	[MEMRD] = src_default_config_memrd,
	[MEMWR] = src_default_config_memwr,
	[ARCRW] = src_default_config_arcrw
};

static int src_index( struct rsc *rsc)
{
	return rsc->conj;
}

static int src_set_state(struct src *src, unsigned int state)
{
	struct hw *hw = src->rsc.hw;
	hw->src_set_state(src->rsc.ctrl_blk, state);
	return 0;
}

static int src_set_bm(struct src *src, unsigned int bm)
{
	struct hw *hw = src->rsc.hw;
	hw->src_set_bm(src->rsc.ctrl_blk, bm);
	return 0;
}

static int src_set_sf(struct src *src, unsigned int sf)
{
	struct hw *hw = src->rsc.hw;
	hw->src_set_sf(src->rsc.ctrl_blk, sf);
	return 0;
}

static int src_set_pm(struct src *src, unsigned int pm)
{
	struct hw *hw = src->rsc.hw;
	hw->src_set_pm(src->rsc.ctrl_blk, pm);
	return 0;
}

static int src_set_rom(struct src *src, unsigned int rom)
{
	struct hw *hw = src->rsc.hw;
	hw->src_set_rom(src->rsc.ctrl_blk, rom);
	return 0;
}

static int src_set_vo(struct src *src, unsigned int vo)
{
	struct hw *hw = src->rsc.hw;
	hw->src_set_vo(src->rsc.ctrl_blk, vo);
	return 0;
}

static int src_set_st(struct src *src, unsigned int st)
{
	struct hw *hw = src->rsc.hw;
	hw->src_set_st(src->rsc.ctrl_blk, st);
	return 0;
}

static int src_set_bp(struct src *src, unsigned int bp)
{
	struct hw *hw = src->rsc.hw;
	hw->src_set_bp(src->rsc.ctrl_blk, bp);
	return 0;
}

static int src_set_cisz(struct src *src, unsigned int cisz)
{
	struct hw *hw = src->rsc.hw;
	hw->src_set_cisz(src->rsc.ctrl_blk, cisz);
	return 0;
}

static int src_set_ca(struct src *src, unsigned int ca)
{
	struct hw *hw = src->rsc.hw;
	hw->src_set_ca(src->rsc.ctrl_blk, ca);
	return 0;
}

static int src_set_sa(struct src *src, unsigned int sa)
{
	struct hw *hw = src->rsc.hw;
	hw->src_set_sa(src->rsc.ctrl_blk, sa);
	return 0;
}

static int src_set_la(struct src *src, unsigned int la)
{
	struct hw *hw = src->rsc.hw;
	hw->src_set_la(src->rsc.ctrl_blk, la);
	return 0;
}

static int src_set_pitch(struct src *src, unsigned int pitch)
{
	struct hw *hw = src->rsc.hw;
	hw->src_set_pitch(src->rsc.ctrl_blk, pitch);
	return 0;
}

static int src_set_clear_zbufs(struct src *src)
{
	struct hw *hw = src->rsc.hw;
	hw->src_set_clear_zbufs(src->rsc.ctrl_blk, 1);
	return 0;
}

static int src_commit_write(struct src *src)
{
	struct hw *hw = src->rsc.hw;
	int i;
	unsigned int dirty = 0;

	src->rsc.ops->master(&src->rsc);
	if (src->rsc.msr > 1) {
		/* Save dirty flags for conjugate resource programming */
		//dirty = hw->src_get_dirty(src->rsc.ctrl_blk) & conj_mask;
		dirty = hw->src_get_dirty(src->rsc.ctrl_blk) & hw->src_dirty_conj_mask();

	}
	hw->src_commit_write(hw, src->rsc.ops->index(&src->rsc), src->rsc.ctrl_blk);

	/* Program conjugate parameter mixer resources */
	if (MEMWR == src->mode)
		return 0;

	for (i = 1; i < src->rsc.msr; i++) {
		src->rsc.ops->next_conj(&src->rsc);
		hw->src_set_dirty(src->rsc.ctrl_blk, dirty);
		hw->src_commit_write(hw, src->rsc.ops->index(&src->rsc), src->rsc.ctrl_blk);
	}
	src->rsc.ops->master(&src->rsc);

	return 0;
}

static int src_get_ca(struct src *src)
{
	struct hw *hw = src->rsc.hw;
	return hw->src_get_ca(hw, src->rsc.ops->index(&src->rsc), src->rsc.ctrl_blk);
}

static int src_init(struct src *src)
{
	dbgprintf(("src_init(%X) mode=%u\n", src, src->mode ));
	src_default_config[src->mode](src);

	return 0;
}

static struct src *src_next_interleave(struct src *src)
{
	return src->intlv;
}

static int src_default_config_memrd(struct src *src)
{
	struct hw *hw = src->rsc.hw;
	unsigned int rsr, msr;

	hw->src_set_state(src->rsc.ctrl_blk, SRC_STATE_OFF);
	hw->src_set_bm(src->rsc.ctrl_blk, 1);
	for (rsr = 0, msr = src->rsc.msr; msr > 1; msr >>= 1)
		rsr++;

	hw->src_set_rsr(src->rsc.ctrl_blk, rsr);
	hw->src_set_sf(src->rsc.ctrl_blk, SRC_SF_S16);
	hw->src_set_wr(src->rsc.ctrl_blk, 0);
	hw->src_set_pm(src->rsc.ctrl_blk, 0);
	hw->src_set_rom(src->rsc.ctrl_blk, 0);
	hw->src_set_vo(src->rsc.ctrl_blk, 0);
	hw->src_set_st(src->rsc.ctrl_blk, 0);
	hw->src_set_ilsz(src->rsc.ctrl_blk, src->multi - 1);
	hw->src_set_cisz(src->rsc.ctrl_blk, 0x80);
	hw->src_set_sa(src->rsc.ctrl_blk, 0x0);
	hw->src_set_la(src->rsc.ctrl_blk, 0x1000);
	hw->src_set_ca(src->rsc.ctrl_blk, 0x80);
	hw->src_set_pitch(src->rsc.ctrl_blk, 0x1000000);
	hw->src_set_clear_zbufs(src->rsc.ctrl_blk, 1);

	src->rsc.ops->master(&src->rsc);
	hw->src_commit_write(hw, src->rsc.ops->index(&src->rsc), src->rsc.ctrl_blk);

	for (msr = 1; msr < src->rsc.msr; msr++) {
		src->rsc.ops->next_conj(&src->rsc);
		hw->src_set_pitch(src->rsc.ctrl_blk, 0x1000000);
		hw->src_commit_write(hw, src->rsc.ops->index(&src->rsc), src->rsc.ctrl_blk);
	}
	src->rsc.ops->master(&src->rsc);

	return 0;
}

static int src_default_config_memwr(struct src *src)
{
	struct hw *hw = src->rsc.hw;

	hw->src_set_state(src->rsc.ctrl_blk, SRC_STATE_OFF);
	hw->src_set_bm(src->rsc.ctrl_blk, 1);
	hw->src_set_rsr(src->rsc.ctrl_blk, 0);
	hw->src_set_sf(src->rsc.ctrl_blk, SRC_SF_S16);
	hw->src_set_wr(src->rsc.ctrl_blk, 1);
	hw->src_set_pm(src->rsc.ctrl_blk, 0);
	hw->src_set_rom(src->rsc.ctrl_blk, 0);
	hw->src_set_vo(src->rsc.ctrl_blk, 0);
	hw->src_set_st(src->rsc.ctrl_blk, 0);
	hw->src_set_ilsz(src->rsc.ctrl_blk, 0);
	hw->src_set_cisz(src->rsc.ctrl_blk, 0x80);
	hw->src_set_sa(src->rsc.ctrl_blk, 0x0);
	hw->src_set_la(src->rsc.ctrl_blk, 0x1000);
	hw->src_set_ca(src->rsc.ctrl_blk, 0x80);
	hw->src_set_pitch(src->rsc.ctrl_blk, 0x1000000);
	hw->src_set_clear_zbufs(src->rsc.ctrl_blk, 1);

	src->rsc.ops->master(&src->rsc);
	hw->src_commit_write(hw, src->rsc.ops->index(&src->rsc), src->rsc.ctrl_blk);

	return 0;
}

static int src_default_config_arcrw(struct src *src)
{
	struct hw *hw = src->rsc.hw;
	unsigned int rsr, msr;
	unsigned int dirty;

	hw->src_set_state(src->rsc.ctrl_blk, SRC_STATE_OFF);
	hw->src_set_bm(src->rsc.ctrl_blk, 0);
	for (rsr = 0, msr = src->rsc.msr; msr > 1; msr >>= 1)
		rsr++;

	hw->src_set_rsr(src->rsc.ctrl_blk, rsr);
	hw->src_set_sf(src->rsc.ctrl_blk, SRC_SF_F32);
	hw->src_set_wr(src->rsc.ctrl_blk, 0);
	hw->src_set_pm(src->rsc.ctrl_blk, 0);
	hw->src_set_rom(src->rsc.ctrl_blk, 0);
	hw->src_set_vo(src->rsc.ctrl_blk, 0);
	hw->src_set_st(src->rsc.ctrl_blk, 0);
	hw->src_set_ilsz(src->rsc.ctrl_blk, 0);
	hw->src_set_cisz(src->rsc.ctrl_blk, 0x80);
	hw->src_set_sa(src->rsc.ctrl_blk, 0x0);
	/*hw->src_set_sa(src->rsc.ctrl_blk, 0x100);*/
	hw->src_set_la(src->rsc.ctrl_blk, 0x1000);
	/*hw->src_set_la(src->rsc.ctrl_blk, 0x03ffffe0);*/
	hw->src_set_ca(src->rsc.ctrl_blk, 0x80);
	hw->src_set_pitch(src->rsc.ctrl_blk, 0x1000000);
	hw->src_set_clear_zbufs(src->rsc.ctrl_blk, 1);

	dirty = hw->src_get_dirty(src->rsc.ctrl_blk);
	src->rsc.ops->master(&src->rsc);
	for (msr = 0; msr < src->rsc.msr; msr++) {
		hw->src_set_dirty(src->rsc.ctrl_blk, dirty);
		hw->src_commit_write(hw, src->rsc.ops->index(&src->rsc), src->rsc.ctrl_blk);
		src->rsc.ops->next_conj(&src->rsc);
	}
	src->rsc.ops->master(&src->rsc);

	return 0;
}

static const struct src_rsc_ops src_rsc_ops = {
	.set_state = src_set_state,
	.set_bm = src_set_bm,
	.set_sf = src_set_sf,
	.set_pm = src_set_pm,
	.set_rom = src_set_rom,
	.set_vo = src_set_vo,
	.set_st = src_set_st,
	.set_bp = src_set_bp,
	.set_cisz = src_set_cisz,
	.set_ca = src_set_ca,
	.set_sa = src_set_sa,
	.set_la = src_set_la,
	.set_pitch = src_set_pitch,
	.set_clr_zbufs = src_set_clear_zbufs,
	.commit_write = src_commit_write,
	.get_ca = src_get_ca,
	.init = src_init,
	.next_interleave = src_next_interleave,
};

static int src_rsc_init(struct src *src, unsigned idx, const struct src_desc *desc, struct src_mgr *mgr )
/////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	int err;
	int i, n;
	struct src *p;

	n = (MEMRD == desc->mode) ? desc->multi : 1;
	for (i = 0, p = src; i < n; i++, p++) {
		err = rsc_init(&p->rsc, idx + i, SRC, desc->msr, mgr->mgr.hw);
		if (err)
			goto error1;

		/* Initialize src specific rsc operations */
		p->ops = &src_rsc_ops;
		p->multi = (0 == i) ? desc->multi : 1;
		p->mode = desc->mode;
		//src_default_config[desc->mode](p);
		src_init(p);
		mgr->src_enable(mgr, p);
		p->intlv = p + 1;
	}
	(--p)->intlv = NULL;	/* Set @intlv of the last SRC to NULL */

	mgr->commit_write(mgr);

	return 0;

error1:
	for (i--, p--; i >= 0; i--, p--) {
		//mgr->src_disable(mgr, p);
		//rsc_uninit(&p->rsc);
	}
	mgr->commit_write(mgr);
	return err;
}

static int get_src_rsc(struct src_mgr *mgr, const struct src_desc *desc, struct src **rsrc)
///////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int idx = SRC_RESOURCE_NUM;
	int err;
	struct src *src;

	*rsrc = NULL;

	/* Check whether there are sufficient src resources to meet request. */
	if (MEMRD == desc->mode)
		err = mgr_get_resource(&mgr->mgr, desc->multi, &idx);
	else
		err = mgr_get_resource(&mgr->mgr, 1, &idx);

	if (err) {
		dbgprintf(("srcmgr.get_src(mgr=%X, desc.mode/multi=%u/%u): Can't meet SRC resource request!\n", mgr, desc->mode, desc->multi));
		return err;
	}

	/* Allocate mem for master src resource */
	if (MEMRD == desc->mode)
		src = calloc(1, sizeof(struct src) * desc->multi);
	else
		src = calloc(1, sizeof(struct src));

	if (!src) {
		err = -ENOMEM;
		goto error;
	}

	if (err = src_rsc_init(src, idx, desc, mgr))
		goto error;

	dbgprintf(("srcmgr.get_src(mgr=%X, desc.mode/multi=%u/%u, dst=%X)=%X - SRC(%u)\n", mgr, desc->mode, desc->multi, rsrc, src, idx));
	*rsrc = src;

	return 0;

error:
	dbgprintf(("srcmgr.get_src(mgr=%X, desc.mode/multi=%u/%u, dst=%X): ERROR %d\n", mgr, desc->mode, desc->multi, rsrc, err));
	//if (MEMRD == desc->mode)
	//	mgr_put_resource(&mgr->mgr, desc->multi, idx);
	//else
	//	mgr_put_resource(&mgr->mgr, 1, idx);
	return err;
}

static int src_enable_s(struct src_mgr *mgr, struct src *src)
{
	struct hw *hw = mgr->mgr.hw;
	int i;

	src->rsc.ops->master(&src->rsc);
	for (i = 0; i < src->rsc.msr; i++) {
		hw->src_mgr_enbs_src(mgr->mgr.ctrl_blk, src->rsc.ops->index(&src->rsc));
		src->rsc.ops->next_conj(&src->rsc);
	}
	src->rsc.ops->master(&src->rsc);

	return 0;
}

static int src_enable(struct src_mgr *mgr, struct src *src)
{
	struct hw *hw = mgr->mgr.hw;
	int i;

	src->rsc.ops->master(&src->rsc);
	for (i = 0; i < src->rsc.msr; i++) {
		hw->src_mgr_enb_src(mgr->mgr.ctrl_blk, src->rsc.ops->index(&src->rsc));
		src->rsc.ops->next_conj(&src->rsc);
	}
	src->rsc.ops->master(&src->rsc);

	return 0;
}

static int src_disable(struct src_mgr *mgr, struct src *src)
{
	struct hw *hw = mgr->mgr.hw;
	int i;

	src->rsc.ops->master(&src->rsc);
	for (i = 0; i < src->rsc.msr; i++) {
		hw->src_mgr_dsb_src(mgr->mgr.ctrl_blk, src->rsc.ops->index(&src->rsc));
		src->rsc.ops->next_conj(&src->rsc);
	}
	src->rsc.ops->master(&src->rsc);

	return 0;
}

static int srcmgr_commit_write(struct src_mgr *mgr)
///////////////////////////////////////////////////
{
	struct hw *hw = mgr->mgr.hw;
	hw->src_mgr_commit_write(hw, mgr->mgr.ctrl_blk);
	return 0;
}

static int src_mgr_create(struct hw *hw, void **rsrc_mgr)
/////////////////////////////////////////////////////////
{
	int err, i;
	struct src_mgr *src_mgr;

	*rsrc_mgr = NULL;
	src_mgr = calloc(1, sizeof(struct src_mgr));
	if (!src_mgr) {
		err = -ENOMEM;
		goto error;
	}

	if (err = rsc_mgr_init(&src_mgr->mgr, SRC, SRC_RESOURCE_NUM, hw))
		goto error;

	//spin_lock_init(&src_mgr->mgr_lock);
	//conj_mask = hw->src_dirty_conj_mask();

	src_mgr->get_src = get_src_rsc;
	//src_mgr->put_src = put_src_rsc;
	src_mgr->src_enable_s = src_enable_s;
	src_mgr->src_enable = src_enable;
	//src_mgr->src_disable = src_disable;
	src_mgr->commit_write = srcmgr_commit_write;
	src_mgr->card = hw->card;

	/* Disable all SRC resources. */
	for (i = 0; i < 256; i++)
		hw->src_mgr_dsb_src(src_mgr->mgr.ctrl_blk, i);

	hw->src_mgr_commit_write(hw, src_mgr->mgr.ctrl_blk);

	*rsrc_mgr = src_mgr;
	dbgprintf(("src_mgr_create(hw=%X, dst=%X)=%X\n", hw, rsrc_mgr, src_mgr));

	return 0;

error:
	//kfree(src_mgr);
	dbgprintf(("src_mgr_create(hw=%X, dst=%X): ERROR %d\n", hw, rsrc_mgr, err));
	return err;
}

#if ADC_SUPP

/* SRCIMP resource manager operations */

static void srcimp_master(struct rsc *rsc)
{
	rsc->conj = 0;
	//rsc->idx = container_of(rsc, struct srcimp, rsc)->idx[0];
	rsc->idx = ((struct srcimp *)rsc)->idx[0];
}

static void srcimp_next_conj(struct rsc *rsc)
{
	rsc->conj++;
}

static int srcimp_index(const struct rsc *rsc)
{
	//return container_of(rsc, struct srcimp, rsc)->idx[rsc->conj];
	return ((struct srcimp *)rsc)->idx[rsc->conj];
}

static const struct rsc_ops srcimp_basic_rsc_ops = {
	.master = srcimp_master,
	.next_conj = srcimp_next_conj,
	.index = srcimp_index,
	.output_slot = NULL,
};

static int srcimp_map(struct srcimp *srcimp, struct src *src, struct rsc *input)
{
	struct imapper *entry;
	int i;

	dbgprintf(("srcimp_map(%s, ", getrsctypeX(&srcimp->rsc))); dbgprintf(("%s, ", getrsctypeX(&src->rsc))); dbgprintf(("%s)\n", getrsctypeX(input)));
	srcimp->rsc.ops->master(&srcimp->rsc);
	src->rsc.ops->master(&src->rsc);
	input->ops->master(input);

	/* Program master and conjugate resources */
	for (i = 0; i < srcimp->rsc.msr; i++) {
		entry = &srcimp->imappers[i];
		entry->slot = input->ops->output_slot(input);
		entry->user = src->rsc.ops->index(&src->rsc);
		entry->addr = srcimp->rsc.ops->index(&srcimp->rsc);
		srcimp->mgr->imap_add(srcimp->mgr, entry);
		srcimp->mapped |= (0x1 << i);

		srcimp->rsc.ops->next_conj(&srcimp->rsc);
		input->ops->next_conj(input);
	}

	srcimp->rsc.ops->master(&srcimp->rsc);
	input->ops->master(input);

	return 0;
}

static int srcimp_unmap(struct srcimp *srcimp)
{
	int i;

	/* Program master and conjugate resources */
	for (i = 0; i < srcimp->rsc.msr; i++) {
		if (srcimp->mapped & (0x1 << i)) {
			srcimp->mgr->imap_delete(srcimp->mgr, &srcimp->imappers[i]);
			srcimp->mapped &= ~(0x1 << i);
		}
	}

	return 0;
}

static const struct srcimp_rsc_ops srcimp_ops = {
	.map = srcimp_map,
	.unmap = srcimp_unmap
};

static int srcimp_rsc_init(struct srcimp *srcimp, const struct srcimp_desc *desc, struct srcimp_mgr *mgr)
{
	int err;

	err = rsc_init(&srcimp->rsc, srcimp->idx[0], SRCIMP, desc->msr, mgr->mgr.hw);
	if (err)
		return err;

	/* Set srcimp specific operations */
	srcimp->rsc.ops = &srcimp_basic_rsc_ops;
	srcimp->ops = &srcimp_ops;
	srcimp->mgr = mgr;

	srcimp->rsc.ops->master(&srcimp->rsc);

	return 0;
}

static int get_srcimp_rsc(struct srcimp_mgr *mgr, const struct srcimp_desc *desc, struct srcimp **rsrcimp)
//////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	int err, i;
	unsigned int idx;
	struct srcimp *srcimp;

	*rsrcimp = NULL;

	/* Allocate mem for SRCIMP resource */
	//srcimp = kzalloc_flex(*srcimp, imappers, desc->msr);
	srcimp = calloc(1, sizeof(struct srcimp) * desc->msr);
	if (!srcimp)
		return -ENOMEM;

	/* Check whether there are sufficient SRCIMP resources. */
	err = 0;
	//scoped_guard(spinlock_irqsave, &mgr->mgr_lock) {
	for (i = 0; i < desc->msr; i++) {
		err = mgr_get_resource(&mgr->mgr, 1, &idx);
		if (err)
			break;

		srcimp->idx[i] = idx;
	}
	//}
	if (err) {
		dbgprintf(("get_srcimp_rsc: can't meet SRCIMP resource request!\n"));
		goto error1;
	}

	if (err = srcimp_rsc_init(srcimp, desc, mgr))
		goto error1;

	*rsrcimp = srcimp;

	dbgprintf(("get_srcimp_rsc(mgr=%X, desc.msr=%u, dst=%X)=%X - SRC(%u)\n", mgr, desc->msr, rsrcimp, srcimp, idx));
	return 0;

error1:
	dbgprintf(("get_srcimp_rsc(mgr=%X, desc.msr=%u, dst=%X): ERROR %d\n", mgr, desc->msr, rsrcimp, err));
	//scoped_guard(spinlock_irqsave, &mgr->mgr_lock) {
	//for (i--; i >= 0; i--)
	//	mgr_put_resource(&mgr->mgr, 1, srcimp->idx[i]);
	//}
	//kfree(srcimp);
	return err;
}

static int srcimp_map_op(void *data, struct imapper *entry)
{
	struct rsc_mgr *mgr = &((struct srcimp_mgr *)data)->mgr;
	struct hw *hw = mgr->hw;

	hw->srcimp_mgr_set_imaparc(mgr->ctrl_blk, entry->slot);
	hw->srcimp_mgr_set_imapuser(mgr->ctrl_blk, entry->user);
	hw->srcimp_mgr_set_imapnxt(mgr->ctrl_blk, entry->next);
	hw->srcimp_mgr_set_imapaddr(mgr->ctrl_blk, entry->addr);
	hw->srcimp_mgr_commit_write(mgr->hw, mgr->ctrl_blk);

	return 0;
}

static int srcimp_imap_add(struct srcimp_mgr *mgr, struct imapper *entry)
{
	//guard(spinlock_irqsave)(&mgr->imap_lock);
	if ((0 == entry->addr) && (mgr->init_imap_added)) {
		input_mapper_delete(&mgr->imappers, mgr->init_imap, srcimp_map_op, mgr);
		mgr->init_imap_added = 0;
	}
	return input_mapper_add(&mgr->imappers, entry, srcimp_map_op, mgr);
}

static int srcimp_imap_delete(struct srcimp_mgr *mgr, struct imapper *entry)
{
	int err;

	//guard(spinlock_irqsave)(&mgr->imap_lock);
	err = input_mapper_delete(&mgr->imappers, entry, srcimp_map_op, mgr);
	if (list_empty(&mgr->imappers)) {
		input_mapper_add(&mgr->imappers, mgr->init_imap, srcimp_map_op, mgr);
		mgr->init_imap_added = 1;
	}

	return err;
}

int srcimp_mgr_create(struct hw *hw, void **rsrcimp_mgr)
{
	int err;
	struct srcimp_mgr *srcimp_mgr;
	struct imapper *entry;

	*rsrcimp_mgr = NULL;
	srcimp_mgr = calloc(1, sizeof(struct srcimp_mgr));
	if (!srcimp_mgr)
		return -ENOMEM;

	err = rsc_mgr_init(&srcimp_mgr->mgr, SRCIMP, SRCIMP_RESOURCE_NUM, hw);
	if (err)
		goto error1;

	//spin_lock_init(&srcimp_mgr->mgr_lock);
	//spin_lock_init(&srcimp_mgr->imap_lock);
	INIT_LIST_HEAD(&srcimp_mgr->imappers);
	entry = calloc(1, sizeof( struct imapper));
	if (!entry) {
		err = -ENOMEM;
		goto error2;
	}
	entry->slot = entry->addr = entry->next = entry->user = 0;
	list_add(&entry->list, &srcimp_mgr->imappers);
	srcimp_mgr->init_imap = entry;
	srcimp_mgr->init_imap_added = 1;

	srcimp_mgr->get_srcimp = get_srcimp_rsc;
	//srcimp_mgr->put_srcimp = put_srcimp_rsc;
	srcimp_mgr->imap_add = srcimp_imap_add;
	srcimp_mgr->imap_delete = srcimp_imap_delete;
	srcimp_mgr->card = hw->card;

	*rsrcimp_mgr = srcimp_mgr;
	dbgprintf(("srcimp_mgr_create(hw=%X, dst=%X)=%X\n", hw, rsrcimp_mgr, srcimp_mgr));

	return 0;

error2:
	//rsc_mgr_uninit(&srcimp_mgr->mgr);
error1:
	//kfree(srcimp_mgr);
	dbgprintf(("srcimp_mgr_create(hw=%X, dst=%X): ERROR %d\n", hw, rsrcimp_mgr, err));
	return err;
}
#endif

/*-----------------------------------------------------------*/

/* ctamixer.c */

#define AMIXER_RESOURCE_NUM 256
#define SUM_RESOURCE_NUM    256

#define AMIXER_Y_IMMEDIATE    1 /* mode */
#define BLANK_SLOT        4094

static void amixer_master(struct rsc *rsc)
{
	rsc->conj = 0;
	//rsc->idx = container_of(rsc, struct amixer, rsc)->idx[0];
	rsc->idx = ((struct amixer *)rsc)->idx[0];
}

static void amixer_next_conj(struct rsc *rsc)
{
	rsc->conj++;
}
static int amixer_index(const struct rsc *rsc)
{
	//return container_of(rsc, struct amixer, rsc)->idx[rsc->conj];
	return ((struct amixer *)rsc)->idx[rsc->conj];
}

static int amixer_output_slot(const struct rsc *rsc)
{
	return (amixer_index(rsc) << 4) + 0x4;
}

static const struct rsc_ops amixer_basic_rsc_ops = {
	.master = amixer_master,
	.next_conj = amixer_next_conj,
	.index = amixer_index,
	.output_slot = amixer_output_slot,
};

static int amixer_set_input(struct amixer *amixer, struct rsc *rsc)
{
	struct hw *hw = amixer->rsc.hw;

	dbgprintf(("amixer_set_input(mixer=%s", getrsctypeX(&amixer->rsc))); dbgprintf((" input=%s)\n", getrsctypeX(rsc) ));
	hw->amixer_set_mode(amixer->rsc.ctrl_blk, AMIXER_Y_IMMEDIATE);
	amixer->input = rsc;
	if (!rsc)
		hw->amixer_set_x(amixer->rsc.ctrl_blk, BLANK_SLOT);
	else {
		hw->amixer_set_x(amixer->rsc.ctrl_blk, rsc->ops->output_slot(rsc));
	}

	return 0;
}

static int amixer_set_y(struct amixer *amixer, unsigned int y)
{
	struct hw *hw = amixer->rsc.hw;
	hw->amixer_set_y(amixer->rsc.ctrl_blk, y);
	return 0;
}

static int amixer_set_invalid_squash(struct amixer *amixer, unsigned int iv)
{
	struct hw *hw = amixer->rsc.hw;
	hw->amixer_set_iv(amixer->rsc.ctrl_blk, iv);
	return 0;
}

static int amixer_set_sum(struct amixer *amixer, struct sum *sum)
{
	struct hw *hw = amixer->rsc.hw;

	dbgprintf(("amixer_set_sum(mixer=%s", getrsctypeX(&amixer->rsc))); dbgprintf((" sum=%s)\n", getrsctypeX(&sum->rsc)));
	amixer->sum = sum;
	if (!sum) {
		hw->amixer_set_se(amixer->rsc.ctrl_blk, 0);
	} else {
		hw->amixer_set_se(amixer->rsc.ctrl_blk, 1);
		hw->amixer_set_sadr(amixer->rsc.ctrl_blk, sum->rsc.ops->index(&sum->rsc));
	}

	return 0;
}

static int amixer_commit_write(struct amixer *amixer)
/////////////////////////////////////////////////////
{
	struct hw *hw = amixer->rsc.hw;
	unsigned int index;
	int i;
	struct rsc *input;
	struct sum *sum;

	dbgprintf(("amixer_commit_write(%s)\n", getrsctypeX(&amixer->rsc)));
	input = amixer->input;
	sum = amixer->sum;

	/* Program master and conjugate resources */
	amixer->rsc.ops->master(&amixer->rsc);
	if (input)
		input->ops->master(input);

	if (sum)
		sum->rsc.ops->master(&sum->rsc);

	for (i = 0; i < amixer->rsc.msr; i++) {
		hw->amixer_set_dirty_all(amixer->rsc.ctrl_blk);
		if (input) {
			hw->amixer_set_x(amixer->rsc.ctrl_blk, input->ops->output_slot(input));
			input->ops->next_conj(input);
		}
		if (sum) {
			hw->amixer_set_sadr(amixer->rsc.ctrl_blk, sum->rsc.ops->index(&sum->rsc));
			sum->rsc.ops->next_conj(&sum->rsc);
		}
		index = amixer->rsc.ops->output_slot(&amixer->rsc);
		hw->amixer_commit_write(amixer->rsc.hw, index, amixer->rsc.ctrl_blk);
		amixer->rsc.ops->next_conj(&amixer->rsc);
	}
	amixer->rsc.ops->master(&amixer->rsc);
	if (input)
		input->ops->master(input);

	if (sum)
		sum->rsc.ops->master(&sum->rsc);

	return 0;
}

static int amixer_get_y(struct amixer *amixer)
{
	struct hw *hw = amixer->rsc.hw;
	return hw->amixer_get_y(amixer->rsc.ctrl_blk);
}

static int amixer_setup(struct amixer *amixer, struct rsc *input, unsigned int scale, struct sum *sum)
{
	dbgprintf(("amixer_setup(%s, input=%X, scale=%d, sum=%X)\n", getrsctypeX(&amixer->rsc), input, scale, sum));
	amixer_set_input(amixer, input);
	amixer_set_y(amixer, scale);
	amixer_set_sum(amixer, sum);
	amixer_commit_write(amixer);
	return 0;
}

static const struct amixer_rsc_ops amixer_ops = {
	.set_input = amixer_set_input,
	.set_scale = amixer_set_y,
	.set_invalid_squash = amixer_set_invalid_squash,
	.set_sum = amixer_set_sum,
	.commit_write = amixer_commit_write,
	//.commit_raw_write = amixer_commit_raw_write,
	.setup = amixer_setup,
	.get_scale = amixer_get_y,
};

static int amixer_rsc_init(struct amixer *amixer, const struct amixer_desc *desc, struct amixer_mgr *mgr)
/////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	int err;

	dbgprintf(("amixer_rsc_init(%X, desc.msr=%u, mgr=%X)\n", amixer, desc->msr, mgr));
	err = rsc_init(&amixer->rsc, amixer->idx[0], AMIXER, desc->msr, mgr->mgr.hw);
	if (err)
		return err;

	/* Set amixer specific operations */
	amixer->rsc.ops = &amixer_basic_rsc_ops;
	amixer->rsc.conj = 0;
	amixer->ops = &amixer_ops;
	//amixer->input = NULL;
	//amixer->sum = NULL;

	amixer_setup(amixer, NULL, 0, NULL);

	return 0;
}

static int get_amixer_rsc(struct amixer_mgr *mgr, const struct amixer_desc *desc, struct amixer **ramixer)
//////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	int err, i;
	unsigned int idx;
	struct amixer *amixer;

	*ramixer = NULL;

	/* Allocate mem for amixer resource */
	amixer = calloc(1, sizeof( struct amixer ));
	if (!amixer) {
		err = -ENOMEM;
		goto error;
	}

	/* Check whether there are sufficient
	 * amixer resources to meet request. */
	for (i = 0; i < desc->msr; i++) {
		if (err = mgr_get_resource(&mgr->mgr, 1, &idx))
			goto error;
		amixer->idx[i] = idx;
	}

	if (err = amixer_rsc_init(amixer, desc, mgr))
		goto error;

	*ramixer = amixer;
	dbgprintf(("amixermgr.get_amixer(mgr=%X, desc.msr=%u, dst=%X)=%X - AMIXER(%u)\n", mgr, desc->msr, ramixer, amixer, idx));

	return 0;

error:
	//for (i--; i >= 0; i--)
	//	mgr_put_resource(&mgr->mgr, 1, amixer->idx[i]);
	//}
	dbgprintf(("get_amixer_rsc(mgr=%X, desc.msr=%u, dst=%X): ERROR %d\n", mgr, desc->msr, ramixer, err));

	//kfree(amixer);
	return err;
}

static int amixer_mgr_create(struct hw *hw, void **ramixer_mgr)
///////////////////////////////////////////////////////////////
{
	int err;
	struct amixer_mgr *amixer_mgr;

	*ramixer_mgr = NULL;
	amixer_mgr = calloc(1, sizeof( struct amixer_mgr));
	if (!amixer_mgr) {
		err = -ENOMEM;
		goto error;
	}

	if (err = rsc_mgr_init(&amixer_mgr->mgr, AMIXER, AMIXER_RESOURCE_NUM, hw))
		goto error;

	//spin_lock_init(&amixer_mgr->mgr_lock);

	amixer_mgr->get_amixer = get_amixer_rsc;
	//amixer_mgr->put_amixer = put_amixer_rsc;
	amixer_mgr->card = hw->card;

	*ramixer_mgr = amixer_mgr;
	dbgprintf(("amixer_mgr_create(hw=%X, dst=%X)=%X\n", hw, ramixer_mgr, amixer_mgr));

	return 0;

error:
	dbgprintf(("amixer_mgr_create(hw=%X, dst=%X): ERROR %d\n", hw, ramixer_mgr, err));
	//kfree(amixer_mgr);
	return err;
}

static void sum_master(struct rsc *rsc)
{
	rsc->conj = 0;
	rsc->idx = container_of(rsc, struct sum, rsc)->idx[0];
	//rsc->idx = ((struct sum *)rsc)->idx[0];
}

static void sum_next_conj(struct rsc *rsc)
{
	rsc->conj++;
}

static int sum_index(const struct rsc *rsc)
{
	//return container_of(rsc, struct sum, rsc)->idx[rsc->conj];
	return ((struct sum *)rsc)->idx[rsc->conj];
}

static int sum_output_slot(const struct rsc *rsc)
{
	return (sum_index(rsc) << 4) + 0xc;
}

static const struct rsc_ops sum_basic_rsc_ops = {
	.master = sum_master,
	.next_conj = sum_next_conj,
	.index = sum_index,
	.output_slot = sum_output_slot,
};

static int sum_rsc_init(struct sum *sum, const struct sum_desc *desc, struct sum_mgr *mgr)
//////////////////////////////////////////////////////////////////////////////////////////
{
	int err;

	err = rsc_init(&sum->rsc, sum->idx[0], SUM, desc->msr, mgr->mgr.hw);
	if (err)
		return err;

	sum->rsc.ops = &sum_basic_rsc_ops;
	sum->rsc.conj = 0;

	return 0;
}


static int get_sum_rsc(struct sum_mgr *mgr, const struct sum_desc *desc, struct sum **rsum)
///////////////////////////////////////////////////////////////////////////////////////////
{
	int err, i;
	unsigned int idx;
	struct sum *sum;

	*rsum = NULL;

	/* Allocate mem for sum resource */
	sum = calloc(1, sizeof( struct sum));
	if (!sum) {
		err =  -ENOMEM;
		goto error;
	}

	/* Check whether there are sufficient sum resources to meet request. */
	for (i = 0; i < desc->msr; i++) {
		if (err = mgr_get_resource(&mgr->mgr, 1, &idx))
			goto error;
		sum->idx[i] = idx;
	}

	if (err = sum_rsc_init(sum, desc, mgr))
		goto error;

	*rsum = sum;
	dbgprintf(("summgr.get_sum(mgr=%X, desc.msr=%u, dst=%X)=%X - SUM(%u)\n", mgr, desc->msr, rsum, sum, idx));

	return 0;

error:
	dbgprintf(("summgr.get_sum(mgr=%X, desc.msr=%u, dst=%X): ERROR %d\n", mgr, desc->msr, rsum, err));
	//	for (i--; i >= 0; i--)
	//		mgr_put_resource(&mgr->mgr, 1, sum->idx[i]);
	//}
	//kfree(sum);
	return err;
}

static int sum_mgr_create(struct hw *hw, void **rsum_mgr)
/////////////////////////////////////////////////////////
{
	int err;
	struct sum_mgr *sum_mgr;

	*rsum_mgr = NULL;
	sum_mgr = calloc(1, sizeof( struct sum_mgr));
	if (!sum_mgr) {
		err = -ENOMEM;
		goto error;
	}

	if (err = rsc_mgr_init(&sum_mgr->mgr, SUM, SUM_RESOURCE_NUM, hw))
		goto error;

	//spin_lock_init(&sum_mgr->mgr_lock);

	sum_mgr->get_sum = get_sum_rsc;
	//sum_mgr->put_sum = put_sum_rsc;
	sum_mgr->card = hw->card;

	*rsum_mgr = sum_mgr;
	dbgprintf(("sum_mgr_create(hw=%X, dst=%X)=%X\n", hw, rsum_mgr, sum_mgr));

	return 0;

error:
	//kfree(sum_mgr);
	dbgprintf(("sum_mgr_create(hw=%X, dst=%X): ERROR %d\n", hw, rsum_mgr, err));
	return err;
}

/*-----------------------------------------------------------*/

static const unsigned short models_20k1[] = {
	0x0021, CTSB046X,
	0x0022, CTSB055X,
	0x002f, CTSB055X,
	0x0029, CTSB073X,
	0x0031, CTSB073X,
};
#if CT20K2
static const unsigned short models_20k2[] = {
	0x0024, CTSB0760,
	0x0041, CTSB0880,
	0x0042, CTSB0880,
	0x0043, CTSB0880,
	0x0062, CTSB1270,
};
#endif

static enum CTCARDS get_model( const unsigned short *typearray, int cnt, short subsys, enum CTCARDS maskmodel )
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	int i;

	if ( subsys & 0xf000 == 0x6000 ) {
		return maskmodel;

	}
	for ( i = 0; i < cnt; i++ )
		if (typearray[i*2] == subsys )
			return typearray[i*2+1];
	return -1;
}

/* cthardware.c */

static struct hw *create_hw_obj( struct pci_config_s *pci, unsigned short pci_subsys )
//////////////////////////////////////////////////////////////////////////////////////
{
    enum CTCARDS model;
	struct hw *hw;

	switch (pci->device_type) {
	case ATC20K1:
		model = get_model( models_20k1, 5, pci_subsys, CTUAA);
		hw = create_20k1_hw_obj();
		break;
#if CT20K2
	case ATC20K2:
		model = get_model( models_20k2, 5, pci_subsys, CTHENDRIX);
		hw = create_20k2_hw_obj();
		break;
#endif
	default:
		return NULL;
	}
	if ( hw ) {
		hw->chip_type = pci->device_type;
		hw->model = model;
	}

	return hw;
}

/*-----------------------------------------------------------*/

/* cttimer.c */

#define CT_TIMER_FREQ 48000

/*-----------------------------------------------------------*/

/* ctatc.c, atc_get_pitch() */

static unsigned int atc_get_pitch(unsigned int input_rate, unsigned int output_rate)
////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int pitch;
	int b;
#ifdef _DEBUG
	int oldirate = input_rate;
	int oldorate = output_rate;
#endif

	// get pitch and convert to fixed-point 8.24 format
	pitch = (input_rate / output_rate) << 24;
	input_rate %= output_rate;
	input_rate /= 100;
	output_rate /= 100;
	for(b = 31; ((b >= 0) && !(input_rate >> b)); )
		b--;

	if(b >= 0) {
		input_rate <<= (31 - b);
		input_rate /= output_rate;
		b = 24 - (31 - b);
		if (b >= 0)
			input_rate <<= b;
		else
			input_rate >>= -b;

		pitch |= input_rate;
	}
	dbgprintf(("atc_get_pitch(inp_rate=%u, out_rate=%u)=0x%X.%06X\n", oldirate, oldorate, pitch >> 24, pitch & 0xffffff ));

	return pitch;
}

/* ctatc.c, select_rom() */

static int select_rom(unsigned int pitch)
/////////////////////////////////////////
{
	if ((pitch > 0x00428f5c) && (pitch < 0x01b851ec)) { // 0.26 <= pitch <= 1.72
		return 1;
	}else if ((0x01d66666 == pitch) || (0x01d66667 == pitch)) { // pitch == 1.8375
		return 2;
	}else if (0x02000000 == pitch) { // pitch == 2
		return 3;
	}else if (pitch <= 0x08000000) { // 0 <= pitch <= 8
		return 0;
	}
	return -1;
}

/*-----------------------------------------------------------*/

/* ctmixer.c */

#define VOL_SCALE 0x1c
#define VOL_MAX 0x100

#define CHN_NUM 2

enum CT_SUM_CTL {
	SUM_IN_F,
//	SUM_IN_R,
//	SUM_IN_C,
//	SUM_IN_S,
//	SUM_IN_F_C,

	NUM_CT_SUMS
};

enum CT_AMIXER_CTL{
 // volume control mixers
 AMIXER_MASTER_F,
 AMIXER_MASTER_END = AMIXER_MASTER_F,
 //AMIXER_MASTER_R,
 //AMIXER_MASTER_C,
 //AMIXER_MASTER_S,
 AMIXER_PCM_F,
 AMIXER_PCM_END = AMIXER_PCM_F,
 //AMIXER_PCM_R,
 //AMIXER_PCM_C,
 //AMIXER_PCM_S,
 //AMIXER_SPDIFI,
#if ADC_SUPP
 AMIXER_LINEIN,
 AMIXER_MIC,
#endif
 //AMIXER_SPDIFO,
 AMIXER_WAVE_F,
 AMIXER_WAVE_END = AMIXER_WAVE_F,
 //AMIXER_WAVE_R,
 //AMIXER_WAVE_C,
 //AMIXER_WAVE_S,
 AMIXER_MASTER_F_C,
 AMIXER_PCM_F_C,
 //AMIXER_SPDIFI_C,
#if ADC_SUPP
 AMIXER_LINEIN_C,
 AMIXER_MIC_C,
#endif
 // this should always be the last one
 NUM_CT_AMIXERS
};

static enum CT_AMIXER_CTL get_recording_amixer(enum CT_AMIXER_CTL index)
{
	switch (index) {
	case AMIXER_MASTER_F: return AMIXER_MASTER_F_C;
	case AMIXER_PCM_F:    return AMIXER_PCM_F_C;
	//case AMIXER_SPDIFI:   return AMIXER_SPDIFI_C;
#if ADC_SUPP
	case AMIXER_LINEIN:   return AMIXER_LINEIN_C;
	case AMIXER_MIC:      return AMIXER_MIC_C;
#endif
	default: return NUM_CT_AMIXERS;
	}
}

#if 0

/* ctmixer.c, uint16_to_float14() */

// Map integer value ranging from 0 to 65535 to 14-bit float value ranging from 2^-6 to (1+1023/1024)

static unsigned int uint16_to_float14(unsigned int x)
/////////////////////////////////////////////////////
{
	unsigned int i;

	if(x < 17)
		return 0;

	x *= 2031;
	x /= 65535;
	x += 16;

	for (i = 0; !(x & 0x400); i++)
		x <<= 1;

	x = (((7 - i) & 0x7) << 10) | (x & 0x3ff);

	return x;
}

/* ctmixer.c, float14_to_uint16() */

static unsigned int float14_to_uint16(unsigned int x)
/////////////////////////////////////////////////////
{
	unsigned int e;

	if(!x)
		return x;

	e = (x >> 10) & 0x7;
	x &= 0x3ff;
	x += 1024;
	x >>= (7 - e);
	x -= 16;
	x *= 65535;
	x /= 2031;

	return x;
}
#endif

static int ct_mixer_topology_build(struct ct_mixer *mixer)
//////////////////////////////////////////////////////////
{
	struct sum *sum;
	struct amixer *amix_d, *amix_s;
	enum CT_AMIXER_CTL i, j;
	enum CT_SUM_CTL k;

	dbgprintf(("ct_mixer_topology_build enter\n"));

	/* Build topology from destination to source */

	/* Set up Master mixer */
	for (i = AMIXER_MASTER_F, k = SUM_IN_F; i <= AMIXER_MASTER_END; i++, k++) {
		amix_d = mixer->amixers[i*CHN_NUM];
		sum = mixer->sums[k*CHN_NUM];
		amix_d->ops->setup(amix_d, &sum->rsc, INIT_VOL, NULL);
		amix_d = mixer->amixers[i*CHN_NUM+1];
		sum = mixer->sums[k*CHN_NUM+1];
		amix_d->ops->setup(amix_d, &sum->rsc, INIT_VOL, NULL);
	}

	/* Set up Wave-out mixer */
	for (i = AMIXER_WAVE_F, j = AMIXER_MASTER_F; i <= AMIXER_WAVE_END; i++, j++) {
		amix_d = mixer->amixers[i*CHN_NUM];
		amix_s = mixer->amixers[j*CHN_NUM];
		amix_d->ops->setup(amix_d, &amix_s->rsc, INIT_VOL, NULL);
		amix_d = mixer->amixers[i*CHN_NUM+1];
		amix_s = mixer->amixers[j*CHN_NUM+1];
		amix_d->ops->setup(amix_d, &amix_s->rsc, INIT_VOL, NULL);
	}
#if 0
	/* Set up S/PDIF-out mixer */
	amix_d = mixer->amixers[AMIXER_SPDIFO*CHN_NUM];
	amix_s = mixer->amixers[AMIXER_MASTER_F*CHN_NUM];
	amix_d->ops->setup(amix_d, &amix_s->rsc, INIT_VOL, NULL);
	amix_d = mixer->amixers[AMIXER_SPDIFO*CHN_NUM+1];
	amix_s = mixer->amixers[AMIXER_MASTER_F*CHN_NUM+1];
	amix_d->ops->setup(amix_d, &amix_s->rsc, INIT_VOL, NULL);
#endif
#if 1
	/* Set up PCM-in mixer */
	for (i = AMIXER_PCM_F, k = SUM_IN_F; i <= AMIXER_PCM_END; i++, k++) {
		amix_d = mixer->amixers[i*CHN_NUM];
		sum = mixer->sums[k*CHN_NUM];
		amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
		amix_d = mixer->amixers[i*CHN_NUM+1];
		sum = mixer->sums[k*CHN_NUM+1];
		amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
	}
#endif
#if ADC_SUPP
	/* Set up Line-in mixer */
	amix_d = mixer->amixers[AMIXER_LINEIN*CHN_NUM];
	sum = mixer->sums[SUM_IN_F*CHN_NUM];
	amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
	amix_d = mixer->amixers[AMIXER_LINEIN*CHN_NUM+1];
	sum = mixer->sums[SUM_IN_F*CHN_NUM+1];
	amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
#endif
#if ADC_SUPP
	/* Set up Mic-in mixer */
	amix_d = mixer->amixers[AMIXER_MIC*CHN_NUM];
	sum = mixer->sums[SUM_IN_F*CHN_NUM];
	amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
	amix_d = mixer->amixers[AMIXER_MIC*CHN_NUM+1];
	sum = mixer->sums[SUM_IN_F*CHN_NUM+1];
	amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
#endif
#if 0
	/* Set up S/PDIF-in mixer */
	amix_d = mixer->amixers[AMIXER_SPDIFI*CHN_NUM];
	sum = mixer->sums[SUM_IN_F*CHN_NUM];
	amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
	amix_d = mixer->amixers[AMIXER_SPDIFI*CHN_NUM+1];
	sum = mixer->sums[SUM_IN_F*CHN_NUM+1];
	amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
#endif
#if 0
	/* Set up Master recording mixer */
	amix_d = mixer->amixers[AMIXER_MASTER_F_C*CHN_NUM];
	sum = mixer->sums[SUM_IN_F_C*CHN_NUM];
	amix_d->ops->setup(amix_d, &sum->rsc, INIT_VOL, NULL);
	amix_d = mixer->amixers[AMIXER_MASTER_F_C*CHN_NUM+1];
	sum = mixer->sums[SUM_IN_F_C*CHN_NUM+1];
	amix_d->ops->setup(amix_d, &sum->rsc, INIT_VOL, NULL);
#endif
#if 0
	/* Set up PCM-in recording mixer */
	amix_d = mixer->amixers[AMIXER_PCM_F_C*CHN_NUM];
	sum = mixer->sums[SUM_IN_F_C*CHN_NUM];
	amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
	amix_d = mixer->amixers[AMIXER_PCM_F_C*CHN_NUM+1];
	sum = mixer->sums[SUM_IN_F_C*CHN_NUM+1];
	amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
#endif
#if 0
	/* Set up Line-in recording mixer */
	amix_d = mixer->amixers[AMIXER_LINEIN_C*CHN_NUM];
	sum = mixer->sums[SUM_IN_F_C*CHN_NUM];
	amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
	amix_d = mixer->amixers[AMIXER_LINEIN_C*CHN_NUM+1];
	sum = mixer->sums[SUM_IN_F_C*CHN_NUM+1];
	amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
#endif
#if 0
	/* Set up Mic-in recording mixer */
	amix_d = mixer->amixers[AMIXER_MIC_C*CHN_NUM];
	sum = mixer->sums[SUM_IN_F_C*CHN_NUM];
	amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
	amix_d = mixer->amixers[AMIXER_MIC_C*CHN_NUM+1];
	sum = mixer->sums[SUM_IN_F_C*CHN_NUM+1];
	amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
#endif
#if 0
	/* Set up S/PDIF-in recording mixer */
	amix_d = mixer->amixers[AMIXER_SPDIFI_C*CHN_NUM];
	sum = mixer->sums[SUM_IN_F_C*CHN_NUM];
	amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
	amix_d = mixer->amixers[AMIXER_SPDIFI_C*CHN_NUM+1];
	sum = mixer->sums[SUM_IN_F_C*CHN_NUM+1];
	amix_d->ops->setup(amix_d, NULL, INIT_VOL, sum);
#endif
	dbgprintf(("ct_mixer_topology_build exit\n"));
	return 0;
}

static int mixer_set_input_port(struct amixer *amixer, struct rsc *rsc)
{
	amixer->ops->set_input(amixer, rsc);
	amixer->ops->commit_write(amixer);

	return 0;
}

static enum CT_AMIXER_CTL port_to_amixer(enum MIXER_PORT_T type)
{
	switch (type) {
	case MIX_WAVE_FRONT: return AMIXER_WAVE_F;
	//case MIX_WAVE_SURROUND: return AMIXER_WAVE_S;
	//case MIX_WAVE_CENTLFE: return AMIXER_WAVE_C;
	//case MIX_WAVE_REAR: return AMIXER_WAVE_R;
	case MIX_PCMO_FRONT: return AMIXER_MASTER_F_C;
	//case MIX_SPDIF_OUT: return AMIXER_SPDIFO;
#if ADC_SUPP
	case MIX_LINE_IN: return AMIXER_LINEIN;
	case MIX_MIC_IN: return AMIXER_MIC;
#endif
	//case MIX_SPDIF_IN: return AMIXER_SPDIFI;
	case MIX_PCMI_FRONT: return AMIXER_PCM_F;
	//case MIX_PCMI_SURROUND: return AMIXER_PCM_S;
	//case MIX_PCMI_CENTLFE: return AMIXER_PCM_C;
	//case MIX_PCMI_REAR: return AMIXER_PCM_R;
	default: return 0;
	}
}

static int mixer_get_output_ports(struct ct_mixer *mixer, enum MIXER_PORT_T type, struct rsc **rleft, struct rsc **rright)
{
	enum CT_AMIXER_CTL amix = port_to_amixer(type);

	if (NULL != rleft)
		*rleft = &((struct amixer *)mixer->amixers[amix*CHN_NUM])->rsc;

	if (NULL != rright)
		*rright = &((struct amixer *)mixer->amixers[amix*CHN_NUM+1])->rsc;

	return 0;
}

static int mixer_set_input_left(struct ct_mixer *mixer, enum MIXER_PORT_T type, struct rsc *rsc)
{
	enum CT_AMIXER_CTL amix = port_to_amixer(type);

	mixer_set_input_port(mixer->amixers[amix*CHN_NUM], rsc);
	amix = get_recording_amixer(amix);
	if (amix < NUM_CT_AMIXERS)
		mixer_set_input_port(mixer->amixers[amix*CHN_NUM], rsc);

	return 0;
}

static int mixer_set_input_right(struct ct_mixer *mixer, enum MIXER_PORT_T type, struct rsc *rsc)
{
	enum CT_AMIXER_CTL amix = port_to_amixer(type);

	mixer_set_input_port(mixer->amixers[amix*CHN_NUM+1], rsc);
	amix = get_recording_amixer(amix);
	if (amix < NUM_CT_AMIXERS)
		mixer_set_input_port(mixer->amixers[amix*CHN_NUM+1], rsc);

	return 0;
}

static int ct_mixer_get_resources(struct ct_mixer *mixer)
/////////////////////////////////////////////////////////
{
	struct sum_mgr *sum_mgr;
	struct sum *sum;
	struct sum_desc sum_desc = {0};
	struct amixer_mgr *amixer_mgr;
	struct amixer *amixer;
	struct amixer_desc am_desc = {0};
	int err;
	int i;

	/* Allocate sum resources for mixer obj */
	sum_mgr = (struct sum_mgr *)mixer->card->rsc_mgrs[SUM];
	sum_desc.msr = mixer->card->msr;
	for (i = 0; i < (NUM_CT_SUMS * CHN_NUM); i++) {
		err = sum_mgr->get_sum(sum_mgr, &sum_desc, &sum);
		if (err) {
			dbgprintf(("ct_mixer_get_resources: Failed to get sum resources for front output!\n"));
			break;
		}
		mixer->sums[i] = sum;
	}
	if (err)
		goto error1;

	/* Allocate amixer resources for mixer obj */
	amixer_mgr = (struct amixer_mgr *)mixer->card->rsc_mgrs[AMIXER];
	am_desc.msr = mixer->card->msr;
	for (i = 0; i < (NUM_CT_AMIXERS * CHN_NUM); i++) {
		err = amixer_mgr->get_amixer(amixer_mgr, &am_desc, &amixer);
		if (err) {
			dbgprintf(("ct_mixer_get_resources: Failed to get amixer resources for mixer obj!\n"));
			break;
		}
		mixer->amixers[i] = amixer;
	}
	if (err)
		goto error2;

	return 0;

error2:
	for (i = 0; i < (NUM_CT_AMIXERS * CHN_NUM); i++) {
		if (NULL != mixer->amixers[i]) {
			amixer = mixer->amixers[i];
			//amixer_mgr->put_amixer(amixer_mgr, amixer);
			mixer->amixers[i] = NULL;
		}
	}
error1:
	for (i = 0; i < (NUM_CT_SUMS * CHN_NUM); i++) {
		if (NULL != mixer->sums[i]) {
			//sum_mgr->put_sum(sum_mgr, (struct sum *)mixer->sums[i]);
			mixer->sums[i] = NULL;
		}
	}

	return err;
}

static int ct_mixer_get_mem(struct ct_mixer **rmixer)
{
	struct ct_mixer *mixer;
	size_t alloc_size;

	*rmixer = NULL;
	/* Allocate mem for mixer obj */
	//alloc_size = struct_size(mixer, amixers, NUM_CT_AMIXERS * CHN_NUM);
	alloc_size = sizeof( struct ct_mixer ) +
		sizeof( struct amixer *) * NUM_CT_AMIXERS * CHN_NUM +
		sizeof( struct sums   *) * NUM_CT_SUMS    * CHN_NUM;
	mixer = calloc(1, alloc_size);
	if (!mixer)
		return -ENOMEM;

	mixer->sums = (struct sum **)(mixer->amixers + (NUM_CT_AMIXERS * CHN_NUM));

	*rmixer = mixer;
	return 0;
}

static int ct_mixer_create(struct emu20kx_card_s *card, struct ct_mixer **rmixer)
/////////////////////////////////////////////////////////////////////////////////
{
	struct ct_mixer *mixer;
	int err;

	dbgprintf(("ct_mixer_create() enter\n"));
	*rmixer = NULL;
	/* Allocate mem for mixer obj */
	err = ct_mixer_get_mem(&mixer);
	if (err)
		return err;

	mixer->switch_state = 0;
	mixer->card = card;
	/* Set operations */
	mixer->get_output_ports = mixer_get_output_ports;
	mixer->set_input_left = mixer_set_input_left;
	mixer->set_input_right = mixer_set_input_right;

	/* Allocate chip resources for mixer obj */
	err = ct_mixer_get_resources(mixer);
	if (err)
		goto error;

	/* Build internal mixer topology */
	ct_mixer_topology_build(mixer);

	*rmixer = mixer;
	dbgprintf(("ct_mixer_create() exit\n"));

	return 0;

error:
	//ct_mixer_destroy(mixer);
	dbgprintf(("ct_mixer_create() failed\n"));
	return err;
}

/*-----------------------------------------------------------*/

#if 0

#include "AC97.H"

static void emu20kx_ac97_write(struct emu20kx_card_s *card,unsigned int reg,unsigned int data)
{
	hw_write_20kx(card,AC97A,reg);
	hw_write_20kx(card,AC97D,data);
}

static void snd_emu20kx_ac97_init(struct emu20kx_card_s *card)
{
	dbgprintf(("emu20kx_ac97_init\n"));
	emu20kx_ac97_write(card, AC97_MASTER_VOL_STEREO, 0x0404);
	emu20kx_ac97_write(card, AC97_PCMOUT_VOL, 0x0404);
	emu20kx_ac97_write(card, AC97_HEADPHONE_VOL, 0x0404);
	emu20kx_ac97_write(card, AC97_EXTENDED_STATUS,AC97_EA_SPDIF);
}
#endif

/*-----------------------------------------------------------*/

static void snd_emu20kx_set_output_format(struct emu20kx_card_s *card,struct audioout_info_s *aui)
//////////////////////////////////////////////////////////////////////////////////////////////////
{
	card->rsr = ( aui->freq_card % 11025 ) ? 48000 : 44100;
	card->msr = aui->freq_card / card->rsr;
	if ( card->msr == 0 )
		card->msr = 1;
	else if ( card->msr > 4 || card->msr == 3 )
		card->msr = 4;

	//aui->freq_card = card->msr * card->rsr;
	aui->freq_card = card->rsr;

	dbgprintf(("emu20kx_set_output_format: rsr=%u msr=%u\n", card->rsr, card->msr ));
}

/*-----------------------------------------------------------*/

/* ctvmem.c, ct_vm_create() */

static unsigned int snd_emu20kx_buffer_init(struct emu20kx_card_s *card,struct audioout_info_s *aui)
////////////////////////////////////////////////////////////////////////////////////////////////////
{
	uint32_t pagecount,pcmbufp,pages;

	//card->pcmout_bufsize = MDma_get_bufsize( aui, 0, EMU20KX_PAGESIZE);
	card->pcmout_bufsize = MDma_get_bufsize( aui, 0, aui->gvars->period_size ? aui->gvars->period_size : 512);
	/* alloc memory for 1) page table, 2) silentpage, 3) pcmout-buffer */
	if (!MDma_alloc_cardmem( &card->dm, EMU20KX_MAXPAGES * sizeof(uint32_t)
							+ EMU20KX_PAGESIZE				 // silentpage
							+ card->pcmout_bufsize			 // pcm output
							+ (1024*4) ))					 // round (XMS alignment is 1kB)
		return 0;

	card->silentpage = (void *)(((uint32_t)card->dm.pMem + 0x0fff) & 0xfffff000); // buffer begins on page boundary
	card->virtualpagetable = (uint32_t *)((uint32_t)card->silentpage + EMU20KX_PAGESIZE);
	card->pcmout_buffer = (char *)(card->virtualpagetable + EMU20KX_MAXPAGES);

#define PAGESHIFT 0 /* todo: check correct value for PAGESHIFT - it's 1 for SB Life/Audigy */

	pcmbufp=(uint32_t)card->pcmout_buffer;
	pages = (card->pcmout_bufsize + EMU20KX_PAGESIZE - 1 ) / EMU20KX_PAGESIZE;
	//pcmbufp <<= PAGESHIFT;
	for(pagecount = 0; pagecount < pages; pagecount++){
		//card->virtualpagetable[pagecount] = (pds_cardmem_physicalptr(card->dm,pcmbufp) << PAGESHIFT) | pagecount;
		card->virtualpagetable[pagecount] = pds_cardmem_physicalptr(card->dm,pcmbufp) << PAGESHIFT;
		dbgprintf(("emu20kx_buffer_init: page %u - phys addr=%X\n",pagecount, card->virtualpagetable[pagecount]));
		pcmbufp += EMU20KX_PAGESIZE;
	}
	for( ; pagecount < EMU20KX_MAXPAGES; pagecount++)
		//card->virtualpagetable[pagecount] = ((uint32_t)card->silentpage) << PAGESHIFT;
		card->virtualpagetable[pagecount] = (pds_cardmem_physicalptr(card->dm,card->silentpage)) << PAGESHIFT;

	aui->card_pDmaBuffer = card->pcmout_buffer;
	dbgprintf(("emu20kx_buffer_init: pcmout_buffer=%X size=%d\n",(unsigned long)card->pcmout_buffer,card->pcmout_bufsize));
	return 1;
}

static unsigned int snd_emu20kx_chip_init(struct emu20kx_card_s *card)
//////////////////////////////////////////////////////////////////////
{
	unsigned int i,gctl,trnctl,ctl_amoplo;
	struct card_conf info;

	dbgprintf(("emu20kx_chip_init: enter, device=%u, subsys_id=0x%X\n", card->pci_dev.device_type, card->subsys_id));

	/* create_hw_obj() - ctatc.c, atc_create_hw_devs() */
	card->hw = create_hw_obj( &card->pci_dev, card->subsys_id );
	if (!card->hw) {
		dbgprintf(("emu20kx_chip_init: create_hw_obj failed\n"));
		return 0;
	}

	/* card_init() - ctatc.c, atc_create_hw_devs() */
	info.rsr = card->rsr;
	info.msr = card->msr;

	/* trn_init() needs the physical address of the page table */
	info.vm_pgt_phys = (unsigned long)pds_cardmem_physicalptr(card->dm, card->virtualpagetable);
	info.pci = &card->pci_dev;

	/* card_init() does:
	 * - calls card_start()
	 * - calls pll_init()
	 * - calls auto_init()
	 * - enable audio ring
	 * - reset interrupt
	 * - config GPIO
	 * - calls trn_init()
	 * - calls daio_init()
	 * - calls dac_init()
     * - enables input from audio ring
	 */

	if ( card->hw->card_init(card->hw, &info) ) {
		dbgprintf(("emu20kx_chip_init: card_init failed\n"));
		return 0;
	}

	dbgprintf(("emu20kx_chip_init: exit\n"));
	return 1;
}

static void snd_emu20kx_chip_close(struct emu20kx_card_s *card)
///////////////////////////////////////////////////////////////
{
}

/* ctatm.c, atc_pcm_playback_prepare() */

static void snd_emu20kx_prepare_playback(struct emu20kx_card_s *card,struct audioout_info_s *aui)
/////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int pitch /*,pm_idx */;
	struct src_desc desc = {0};
	struct amixer_desc mix_dsc;
	struct src *src;
	struct src_mgr *srcmgr = (struct src_mgr *)card->rsc_mgrs[SRC];
	struct amixer_mgr *amixermgr = (struct amixer_mgr *)card->rsc_mgrs[AMIXER];
	int n_amixer, i;
	int sformat;
	int device = 0; //apcm->substream->pcm->device;
	int err;

	dbgprintf(("emu20kx_prepare_playback: enter\n" ));
	/* Get SRC resource */
	desc.multi = 2;
	desc.msr = card->msr;
	desc.mode = MEMRD;
	err = srcmgr->get_src(srcmgr, &desc, &src);
	if (err) {
		dbgprintf(("emu20kx_prepare_playback: error, no src\n" ));
		return;
	}
	card->apcm_src = src; /* setup to be used in snd_emu20kx_start() */

	if (aui->card_wave_id == WAVEID_PCM_FLOAT )
		sformat = SRC_SF_F32;
	else {
		switch(aui->bits_card) {
		case 32: sformat = SRC_SF_S32; break;
		case 24: sformat = SRC_SF_S24; break;
		default: sformat = SRC_SF_S16; aui->bits_card = 16; break;
		}
	}

	pitch = atc_get_pitch( aui->freq_card, card->msr * card->rsr );
	src->ops->set_pitch(src, pitch);
	src->ops->set_rom(src, select_rom(pitch));
	src->ops->set_sf(src, sformat);
	src->ops->set_pm(src, (src->ops->next_interleave(src) != NULL));

	/* Get AMIXER resource */
	n_amixer = 2;
	mix_dsc.msr = card->msr;
	for (i = 0; i < n_amixer; i++) {
		if ( err = amixermgr->get_amixer(amixermgr, &mix_dsc, &card->apcm_amixer[i]) ) {
			dbgprintf(("emu20kx_prepare_playback: error - no more AMIXERs\n" ));
			return;
		}
	}
	/* Connect resources */
	for (i = 0; i < n_amixer; i++) {
		card->apcm_amixer[i]->ops->setup(card->apcm_amixer[i], &src->rsc, INIT_VOL, card->pcm[i+device*2]);
		src = src->ops->next_interleave(src);
		if (!src)
			src = card->apcm_src;
	}
	dbgprintf(("emu20kx_prepare_playback: exit, pitch=0x%X.%06X\n", pitch >> 24, pitch & 0xffffff));
	return;
}

static int atc_get_resources(struct emu20kx_card_s *card)
/////////////////////////////////////////////////////////
{
	struct daio_desc da_desc = {0};
	struct daio_mgr *daio_mgr;
	struct src_desc src_dsc = {0};
	struct src_mgr *src_mgr;
#if ADC_SUPP
	struct srcimp_desc srcimp_dsc = {0};
	struct srcimp_mgr *srcimp_mgr;
#endif
	struct sum_desc sum_dsc = {0};
	struct sum_mgr *sum_mgr;
	int err, i;

	dbgprintf(("atc_get_resources: enter\n" ));

	daio_mgr = (struct daio_mgr *)card->rsc_mgrs[DAIO];
	da_desc.msr = card->msr;
	for (i = 0; i < NUM_DAIOTYP; i++) {
		//if (((i == SPDIFIO) && (atc->model == CTSB073X)) ||
		//	((i == SPDIFI_BAY) && (atc->model != CTSB073X)) ||
		//	((i == MIC) && !cap.dedicated_mic) ||
		//	((i == RCA) && !cap.dedicated]rca))
		//	continue;
		da_desc.type = i;
		//da_desc.output = (i < LINEIM) || (i == RCA);
#if ADC_SUPP
		da_desc.output = (i < LINEIM);
#else
		da_desc.output = 1; /* only outputs are defined (just LINE01) */
#endif
		err = daio_mgr->get_daio(daio_mgr, &da_desc, (struct daio **)&card->daios[i]);
		if (err) {
			dbgprintf(("atc_get_resources: Failed to get DAIO resource %u!\n", i));
			return err;
		}
	}

#if NUM_ATC_SRCS
	src_mgr = (struct src_mgr *)card->rsc_mgrs[SRC];
	src_dsc.multi = 1;
	src_dsc.msr = card->msr;
	src_dsc.mode = ARCRW;
	for (i = 0; i < NUM_ATC_SRCS; i++) {
		err = src_mgr->get_src(src_mgr, &src_dsc, (struct src **)&card->srcs[i]);
		if (err) {
			dbgprintf(("atc_get_resources: Failed to get SRC resource %u!\n", i));
			return err;
		}
	}
#endif

#if ADC_SUPP
	srcimp_mgr = (struct srcimp_mgr *)card->rsc_mgrs[SRCIMP];
	srcimp_dsc.msr = 8;
	//for (i = 0; i < atc_srcs_limit; i++) {
	for (i = 0; i < 4; i++) {
		err = srcimp_mgr->get_srcimp(srcimp_mgr, &srcimp_dsc, (struct srcimp **)&card->srcimps[i]);
		if (err) {
			dbgprintf(("atc_get_resources: Failed to get SRCIMP resource %u!\n", i));
			return err;
		}
	}
#endif

	sum_mgr = (struct sum_mgr *)card->rsc_mgrs[SUM];
	sum_dsc.msr = card->msr;
	for (i = 0; i < NUM_ATC_PCM; i++) {
		err = sum_mgr->get_sum(sum_mgr, &sum_dsc, (struct sum **)&card->pcm[i]);
		if (err) {
			dbgprintf(("atc_get_resources: Failed to get SUM resource %u!\n", i));
			return err;
		}
	}

	dbgprintf(("atc_get_resources: exit\n" ));
	return 0;
}

#if ADC_SUPP
static void atc_connect_dai(struct src_mgr *src_mgr, struct dai *dai, struct src **srcs, struct srcimp **srcimps)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	struct rsc *rscs[2] = {NULL};
	struct src *src;
	struct srcimp *srcimp;
	int i = 0;

	dbgprintf(("atc_connect_dai(src_mgr=%X, dai=%X, &srcs=%X, &srcimps=%X\n", src_mgr, dai, srcs, srcimps));
	rscs[0] = &dai->daio.rscl;
	rscs[1] = &dai->daio.rscr;
	for (i = 0; i < 2; i++) {
		src = srcs[i];
		srcimp = srcimps[i];
		srcimp->ops->map(srcimp, src, rscs[i]);
		src_mgr->src_disable(src_mgr, src);
	}

	src_mgr->commit_write(src_mgr); /* Actually disable SRCs */

	src = srcs[0];
	src->ops->set_pm(src, 1);
	for (i = 0; i < 2; i++) {
		src = srcs[i];
		src->ops->set_state(src, SRC_STATE_RUN);
		src->ops->commit_write(src);
		src_mgr->src_enable_s(src_mgr, src);
	}

	dai->ops->set_srt_srcl(dai, &(srcs[0]->rsc));
	dai->ops->set_srt_srcr(dai, &(srcs[1]->rsc));

	dai->ops->set_enb_src(dai, 1);
	dai->ops->set_enb_srt(dai, 1);
	dai->ops->commit_write(dai);

	src_mgr->commit_write(src_mgr); /* Synchronously enable SRCs */
}
#endif

static void atc_connect_resources(struct emu20kx_card_s *card)
//////////////////////////////////////////////////////////////
{
#if ADC_SUPP
	struct dai *dai;
#endif
	struct dao *dao;
	//struct src *src;
	struct sum *sum;
	struct src *src;
	struct ct_mixer *mixer;
	struct rsc *rscs[2] = {NULL};
	//struct capabilities cap;
	int i, j;

	mixer = card->mixer;
	//cap = card->capabilities(atc);

	dbgprintf(("atc_connect_resources(ctmixer=%X): MIX_WAVE_FRONT\n", mixer));
	//for (i = MIX_WAVE_FRONT, j = LINEO1; i <= MIX_SPDIF_OUT; i++, j++) {
	for (i = MIX_WAVE_FRONT, j = LINEO1; i < MIX_WAVE_FRONT+1; i++, j++) {
		mixer->get_output_ports(mixer, i, &rscs[0], &rscs[1]);
		dao = container_of(card->daios[j], struct dao, daio);
		dbgprintf(("atc_connect_resources: dao.rscl=%s", getrsctypeX(&dao->daio.rscl))); dbgprintf((" rscs[0]=%s\n", getrsctypeX(rscs[0])));
		dao->ops->set_left_input(dao, rscs[0]);
		dbgprintf(("atc_connect_resources: dao.rscr=%s", getrsctypeX(&dao->daio.rscr))); dbgprintf((" rscs[1]=%s\n", getrsctypeX(rscs[1])));
		dao->ops->set_right_input(dao, rscs[1]);
	}
#if 0
	if (cap.dedicated_rca) {
		/* SE-300PCIE has a dedicated DAC for the RCA. */
		atc_dedicated_rca_select(atc);
	}
#endif
#if ADC_SUPP
	dai = container_of(card->daios[LINEIM], struct dai, daio);
	atc_connect_dai((struct src_mgr *)card->rsc_mgrs[SRC], dai, (struct src **)&card->srcs[2], (struct srcimp **)&card->srcimps[2]);
	src = card->srcs[2];
	//mixer->set_input_left(mixer, MIX_LINE_IN, &src->rsc);
	mixer->set_input_left(mixer, MIX_MIC_IN, &src->rsc);
	src = card->srcs[3];
	//mixer->set_input_right(mixer, MIX_LINE_IN, &src->rsc);
	mixer->set_input_right(mixer, MIX_MIC_IN, &src->rsc);
#endif
#if 0
	if (cap.dedicated_mic) {
		/* Titanium HD has a dedicated ADC for the Mic. */
		/* SE-300PCIE has a 4-channel ADC. */
		dai = container_of(card->daios[MIC], struct dai, daio);
		atc_connect_dai(atc->rsc_mgrs[SRC], dai, (struct src **)&card->srcs[4], (struct srcimp **)&card->srcimps[4]);
		src = card->srcs[4];
		mixer->set_input_left(mixer, MIX_MIC_IN, &src->rsc);
		src = card->srcs[5];
		mixer->set_input_right(mixer, MIX_MIC_IN, &src->rsc);
	}
#endif
#if 0
	dai = container_of(card->daios[atc_spdif_in_type(atc)], struct dai, daio);
	atc_connect_dai(card->rsc_mgrs[SRC], dai, (struct src **)&card->srcs[0], (struct srcimp **)&card->srcimps[0]);

	src = atc->srcs[0];
	mixer->set_input_left(mixer, MIX_SPDIF_IN, &src->rsc);
	src = atc->srcs[1];
	mixer->set_input_right(mixer, MIX_SPDIF_IN, &src->rsc);
#endif
#if 1
	dbgprintf(("atc_connect_resources: MIX_PCMI_FRONT\n"));
	//for (i = MIX_PCMI_FRONT, j = 0; i <= MIX_PCMI_SURROUND; i++, j += 2) {
	for (i = MIX_PCMI_FRONT, j = 0; i < MIX_PCMI_FRONT+1; i++, j += 2) {
		sum = card->pcm[j];
		mixer->set_input_left(mixer, i, &sum->rsc);
		sum = card->pcm[j+1];
		mixer->set_input_right(mixer, i, &sum->rsc);
	}
#endif
	dbgprintf(("atc_connect_resources: exit\n"));
}

//-------------------------------------------------------------------------
static const struct pci_device_s emu20kx_devices[]={
 {"EMU20K1",0x1102,0x0005, ATC20K1}, /* device name, vendor id, device id, device type */
#if CT20K2
 {"EMU20K2",0x1102,0x000b, ATC20K2},
#endif
 {NULL,0,0,0}
};

static void EMU20KX_close(struct audioout_info_s *aui);

static struct {
	int (*create)(struct hw *hw, void **rmgr);
} rsc_mgr_funcs[NUM_RSCTYP] = {
	[SRC]    = { src_mgr_create },
#if ADC_SUPP
	[SRCIMP] = { srcimp_mgr_create },
#else
	[SRCIMP] = { NULL },
#endif
	[AMIXER] = { amixer_mgr_create },
	[SUM]    = { sum_mgr_create },
	[DAIO]   = { daio_mgr_create },
};

static int EMU20KX_adetect(struct audioout_info_s *aui)
///////////////////////////////////////////////////////
{
	struct emu20kx_card_s *card = aui->card_private_data;
	int i;

	if(pcibios_search_devices( emu20kx_devices, &card->pci_dev) != PCI_SUCCESSFUL) {
		dbgprintf(("emu20kx_adetect: pcibios_search_devices failed\n"));
		goto err_adetect;
	}

	//pcibios_set_master(&card->pci_dev);
#if 0
	/* EMU20K1 and EMU20K2 use different PCI address slots
	 * so the io base will be detected in cthw20k1/cthw20k2 later.
	 */
	card->iobase = pcibios_ReadConfig_Dword(&card->pci_dev, PCIR_NAMBAR);
	card->iobase &= 0xfffffff8;
	if(!card->iobase)
		goto err_adetect;
#endif
	//card->irq = pcibios_ReadConfig_Byte(&card->pci_dev, PCIR_INTR_LN);
	aui->card_irq = card->pci_dev.bIrq;
	card->subsys_id = pcibios_ReadConfig_Word(&card->pci_dev,PCIR_SSID);

	dbgprintf(("emu20kx_adetect: vend_id=%X dev_id=%X subid=%X irq=%u\n",
			card->pci_dev.vendor_id, card->pci_dev.device_id, card->subsys_id, card->pci_dev.bIrq));

	if(!snd_emu20kx_buffer_init(card,aui))
		goto err_adetect;

	snd_emu20kx_set_output_format(card,aui);

	/* ctatc.c, ct_atc_create() - atc_create_hw_devs() - create_hw_obj() */

	if(!snd_emu20kx_chip_init(card))
		goto err_adetect;

	/* ctatc.c, ct_atc_create() - atc_create_hw_devs() - rsc_mgr_funcs[].create */

	for (i = 0; i < NUM_RSCTYP; i++) {
		int err;
		if (rsc_mgr_funcs[i].create) {
			if (err = rsc_mgr_funcs[i].create(card->hw, (void **)&card->rsc_mgrs[i])) {
				dbgprintf(("emu20kx_adetect: create rsc_mgr[%u] failed\n", i));
				goto err_adetect;
			}
		}
	}

	/* ctatc.c, ct_atc_create() */

	if ( ct_mixer_create(card, &card->mixer) ) {
		goto err_adetect;
	}

	/* ctatc.c, ct_atc_create() */

	if ( atc_get_resources(card) ) {
		goto err_adetect;
	}

	/* Build topology */
	atc_connect_resources(card);

	return 1;

err_adetect:
	EMU20KX_close(aui);
	return 0;
}

static void EMU20KX_close(struct audioout_info_s *aui)
//////////////////////////////////////////////////-///
{
	struct emu20kx_card_s *card = aui->card_private_data;
	dbgprintf(("emu20kx_close\n"));
	if(card){
		if(card->hw) {
			snd_emu20kx_chip_close(card);
			//dpmi_unmap_physical_memory(capd->iobase);
		}
		MDma_free_cardmem( &card->dm );
	}
}

static void EMU20KX_setrate(struct audioout_info_s *aui)
//////////////////-///////////////////////////////-/////
{
	struct emu20kx_card_s *card = aui->card_private_data;
	dbgprintf(("emu20kx_setrate: freq_card=%u\n", aui->freq_card));

	/* emu20kx_set_output_format() has been called during card_init()
	 * because that function needs card->rsr/msr to be set.
	 * Must be called again!
	 */
	snd_emu20kx_set_output_format( card, aui );

	MDma_initbuf( aui, card->pcmout_bufsize );
	snd_emu20kx_prepare_playback( card, aui );
}

/* ctatm.c, atm_pcm_playback_start() */

#define TIMER_SHIFT 1

static void EMU20KX_start(struct audioout_info_s *aui)
//////////////////////////////////////////////////////
{
	struct emu20kx_card_s *card = aui->card_private_data;
	struct src *src = card->apcm_src;
	int period_size = aui->gvars->period_size ? aui->gvars->period_size : 512;
	unsigned int max_cisz;

#if 1
	max_cisz = src->multi * src->rsc.msr;
	max_cisz = 0x80 * min(max_cisz, 8);
#else
	max_cisz = period_size;
#endif

	dbgprintf(("emu20kx_start: period_size=%u, max_cisz=%u\n", period_size, max_cisz));

	/* ctatc.c, atm_pcm_playback_start()
	 * the addresses in set_sa(), set_la() and set_ca() are "logical" -
	 * that is, they are relative to the buffer start.
	 */
	src->ops->set_sa(src, 0);
	src->ops->set_la(src, aui->card_dmasize);
	src->ops->set_ca(src, max_cisz);
	src->ops->set_cisz(src, max_cisz);

	src->ops->set_bm(src, 1);
	src->ops->set_state(src, SRC_STATE_INIT);
	src->ops->commit_write(src);

	card->hw->set_timer_tick(card->hw, period_size >> TIMER_SHIFT );
	card->hw->set_timer_irq(card->hw, 1); /* enable timer interrupt */
}

/* ctatm.c, atm_pcm_stop() */

static void EMU20KX_stop(struct audioout_info_s *aui)
/////////////////////////////////////////////////////
{
	struct emu20kx_card_s *card = aui->card_private_data;
	struct src *src = card->apcm_src;

	dbgprintf(("emu20kx_stop\n"));

	card->hw->set_timer_tick(card->hw, 0);
	card->hw->set_timer_irq(card->hw, 0);

	src->ops->set_bm(src, 0);
	src->ops->set_state(src, SRC_STATE_OFF);
	src->ops->commit_write(src);
}

/*-----------------------------------------------------------*/

/* ctatm.c - atc_pcm_playback_position() */

static unsigned int EMU20KX_getbufpos(struct audioout_info_s *aui)
//////////////////////////////////////////////////////////////////
{
	struct emu20kx_card_s *card = aui->card_private_data;
	unsigned int bufpos;
	//unsigned int max_cisz;

	/* reprogram timer */
	card->hw->set_timer_tick(card->hw, (aui->gvars->period_size ? aui->gvars->period_size : 512 ) >> TIMER_SHIFT );

	bufpos = src_get_ca(card->apcm_src);

	/* todo: explain max_cisz purpose */
	//max_cisz = card->apcm_src->multi * card->apcm_src->rsc.msr * 0x80;
	//bufpos = (bufpos + aui->card_dmasize - max_cisz) % aui->card_dmasize;
	//dbgprintf(("emu20kx_getbufpos: bufpos=%X max_cisz=%X\n", bufpos, max_cisz ));

	//dbgprintf(("emu20kx_getbufpos: pos=0x%X dmasize=%d\n", bufpos, aui->card_dmasize));

	return bufpos;
}

/*-----------------------------------------------------------*/

/* AU_CARDS.C volume range is 0-100 ( decimal )
 * SB X-Fi amixer volume range is 0-1C00h (INIT_VOL), decimal 7168
 * translation: volXFI = volAU * 7168 / 100
 */

static void EMU20KX_writeMIXER(struct audioout_info_s *aui, unsigned long reg, unsigned long val)
/////////////////////////////////////////////////////////////////////////////////////////////////
{
	struct emu20kx_card_s *card = aui->card_private_data;
	unsigned int lval = (val & 15) << 4;
	int i;
	struct amixer *master;
	//struct amixer *pcm;

	dbgprintf(("emu20kx_writeMIXER(%X, reg=%X, value=%u)\n", aui, reg, val));

	/* set left & right channel */
	for (i = 0; i < 2; i++ ) {
		master = card->mixer->amixers[AMIXER_MASTER_F * 2 + i];
		master->ops->set_scale(master, val * INIT_VOL / 100); // set Master
		master->ops->commit_write(master);
		//pcm = card->mixer->amixers[AMIXER_WAVE_F * 2 + i];
		//pcm->ops->set_scale(pcm, val * INIT_VOL / 100); // set PCM
		//pcm->ops->commit_write(pcm);
	}
}

static unsigned long EMU20KX_readMIXER(struct audioout_info_s *aui, unsigned long reg)
//////////////////////////////////////////////////////////////////////////////////////
{
	//struct emu20kx_card_s *card = aui->card_private_data;
	dbgprintf(("emu20kx_readMIXER\n"));
	return 0;
}

static int EMU20KX_IRQRoutine( struct audioout_info_s *aui )
////////////////////////////////////////////////////////////
{
	struct emu20kx_card_s *card = aui->card_private_data;
	int status = card->hw->get_timer_interrupt_pending(card->hw);

	//dbgprintf(("emu20kx_IRQRoutine status=0x%X\n", status));
	if ( status ) {
		card->hw->ack_interrupt(card->hw, status ); /* ack interrupt */
	}
	return status;
}

static struct aucards_mixerchan_s sbxfi_master_vol = {
	AU_MIXCHAN_MASTER, AU_MIXCHANFUNC_VOLUME, 2, {
		{0, 0, 0, SUBMIXCH_INFOBIT_CARD_SETVOL},
		{0, 0, 0, SUBMIXCH_INFOBIT_CARD_SETVOL},
	}
};

static const struct aucards_mixerchan_s *sbxfi_mixerset[] = {
	&sbxfi_master_vol,
	NULL
};

struct sndcard_info_s EMU20KX_sndcard_info = {
 "CTXFI",
 0,
 &EMU20KX_adetect,
 &EMU20KX_start,
 &EMU20KX_stop,
 &EMU20KX_close,
 &EMU20KX_setrate,

 &MDma_writedata,
 &EMU20KX_getbufpos,
 &EMU20KX_IRQRoutine,
 &EMU20KX_writeMIXER,
 &EMU20KX_readMIXER,
 sbxfi_mixerset,
 sizeof(struct emu20kx_card_s)
};

