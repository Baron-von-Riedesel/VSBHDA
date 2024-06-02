//**************************************************************************
//*                     This file is part of the                           *
//*                      Mpxplay - audio player.                           *
//*                  The source code of Mpxplay is                         *
//*        (C) copyright 1998-2014 by PDSoft (Attila Padar)                *
//*                http://mpxplay.sourceforge.net                          *
//*                  email: mpxplay@freemail.hu                            *
//**************************************************************************
//*  This program is distributed in the hope that it will be useful,       *
//*  but WITHOUT ANY WARRANTY; without even the implied warranty of        *
//*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                  *
//*  Please contact with the author (with me) if you want to use           *
//*  or modify this source.                                                *
//**************************************************************************
//function: Intel HD audio driver for Mpxplay
//based on ALSA (http://www.alsa-project.org) and WSS libs

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "CONFIG.H"
#include "MPXPLAY.H"
#include "DMAIRQ.H"
#include "PCIBIOS.H"
#include "SC_INTHD.H"
#include "LINEAR.H"

#define SETPOWERSTATE 1  /* apparently necessary on some laptops */
#define RESETCODECONCLOSE 1 /* todo: explain the benefits! */

/* config_select: bits 0-1: out device */
#define AUCARDSCONFIG_IHD_OUT_DEV_MASK   0x3     /* 00=lineout, 01=speaker, 02=hp */
#define AUCARDSCONFIG_IHD_USE_FIXED_SDO  (1<<2) // don't read stream offset (for sd_addr) from GCAP (use 0x100)

#define INTHD_MAX_CHANNELS 8
#ifdef SBEMU
#define AZX_PERIOD_SIZE 512
#else
#define AZX_PERIOD_SIZE 4096
#endif

typedef uint16_t hda_nid_t;

struct hda_gnode {
 hda_nid_t nid;        /* NID of this widget */
 unsigned short nconns;    /* number of input connections */
 hda_nid_t conn_list[HDA_MAX_CONNECTIONS];
 unsigned int  wid_caps;    /* widget capabilities */
 unsigned char type;    /* widget type */
 unsigned char pin_ctl;    /* pin controls */
 unsigned char checked;    /* the flag indicates that the node is already parsed */
 unsigned int  pin_caps;    /* pin widget capabilities */
 unsigned int  def_cfg;    /* default configuration */
 unsigned int  amp_out_caps;    /* AMP out capabilities */
 unsigned int  amp_in_caps;    /* AMP in capabilities */
 unsigned long supported_formats; // format_val
};

struct pcm_vol_s {
 struct hda_gnode *node;  /* Node for PCM volume */
 unsigned int index;      /* connection of PCM volume */
};


struct intelhd_card_s
{
 unsigned long  iobase;
 struct pci_config_s  *pci_dev;
 unsigned int  board_driver_type; /* ATI, NVIDIA, HDMI, ... */
 long          codec_vendor_id;
 unsigned long codec_mask;
 unsigned int  codec_index;
 hda_nid_t afg_root_nodenum;
 int afg_num_nodes;
 struct hda_gnode *afg_nodes;
 unsigned int def_amp_out_caps;
 unsigned int def_amp_in_caps;
 struct hda_gnode *dac_node[2];            // DAC nodes
 struct hda_gnode *out_pin_node[MAX_PCM_VOLS];    // Output pin (Line-Out) nodes
 unsigned int pcm_num_vols;            // number of PCM volumes
 struct pcm_vol_s pcm_vols[MAX_PCM_VOLS]; // PCM volume nodes

 struct cardmem_s *dm;
 uint32_t *table_buffer;
 char *pcmout_buffer;
 long pcmout_bufsize;
 unsigned long* corb_buffer;
 unsigned long long* rirb_buffer;
 unsigned long pcmout_dmasize;
 unsigned int  pcmout_num_periods;
 unsigned long pcmout_period_size;
 unsigned long sd_addr;    // stream io address (one playback stream only)
 unsigned int  format_val; // stream type
 unsigned int  dacout_num_bits;
 unsigned long supported_formats;
 unsigned long supported_max_freq;
 unsigned int  supported_max_bits;
 unsigned int  config_select;
};

struct hda_rate_tbl {
 unsigned int hz;
 unsigned int hda_fmt;
};

/* supported rates:
 * dividend in bits 8-10: 1,2,3,4,5,6,8 (000,001,010,011,100,101,111)
 * multip. in bits 11-13: 1,2,3,4 (000,001,010,011)
 * bit 14 is base: 1=44100, 0=48000;
 * index is fix, defined by HDA [ R12 missing ( 384 kHz ) ]
 */

#define BASE48K 0x0000
#define BASE44K 0x4000
#define DIV2 (1 << 8)
#define DIV3 (2 << 8)
#define DIV4 (3 << 8)
#define DIV6 (5 << 8)
#define MUL2 (1 << 11)
#define MUL4 (3 << 11)

static const struct hda_rate_tbl rate_bits[] = {
 { 48000 / 6,      BASE48K | DIV6 },          //  8000 0
 { 44100 / 4,      BASE44K | DIV4 },          // 11025 1
 { 48000 / 3,      BASE48K | DIV3 },          // 16000 2  
 { 44100 / 2,      BASE44K | DIV2 },          // 22050 3  
 { 48000 * 2 / 3,  BASE48K | MUL2 | DIV2 },   // 32000 4  
 { 44100,          BASE44K },                 // 44100 5  
 { 48000,          BASE48K },                 // 48000 6  
 { 44100 * 2,      BASE44K | MUL2 },          // 88200 7  
 { 48000 * 2,      BASE48K | MUL2 },          // 96000 8  
 { 44100 * 4,      BASE44K | MUL4 },          //176400 9  
 { 48000 * 4,      BASE48K | MUL4 },          //192000 10 
 { 0xffffffff,     BASE48K | MUL4 },          //192000 11
 {0,0}
};

static struct aucards_mixerchan_s hda_master_vol = {
	AU_MIXCHANFUNCS_PACK( AU_MIXCHAN_MASTER, AU_MIXCHANFUNC_VOLUME), MAX_PCM_VOLS, {
		{0, 0x00, 0, SUBMIXCH_INFOBIT_CARD_SETVOL}, // card->pcm_vols[0] register, max, shift, infobits
		{0, 0x00, 0, SUBMIXCH_INFOBIT_CARD_SETVOL}, // card->pcm_vols[1]
	}
};

//Intel HDA codec has memory mapping only (by specification)
// VSBHDA: access changed to "volatile"

#define azx_writel(chip,reg,value) *((volatile int32_t *)((chip)->iobase + HDA_REG_##reg)) = value
#define azx_writew(chip,reg,value) *((volatile int16_t *)((chip)->iobase + HDA_REG_##reg)) = value
#define azx_writeb(chip,reg,value) *((volatile uint8_t *)((chip)->iobase + HDA_REG_##reg)) = value

#define azx_readl(chip,reg) *((volatile int32_t *)((chip)->iobase + HDA_REG_##reg))
#define azx_readw(chip,reg) *((volatile int16_t *)((chip)->iobase + HDA_REG_##reg))
#define azx_readb(chip,reg) *((volatile uint8_t *)((chip)->iobase + HDA_REG_##reg))

#define azx_sd_writel(dev,reg,value) *((volatile int32_t *)((dev)->sd_addr + HDA_REG_##reg)) = value
#define azx_sd_writew(dev,reg,value) *((volatile int16_t *)((dev)->sd_addr + HDA_REG_##reg)) = value
#define azx_sd_writeb(dev,reg,value) *((volatile uint8_t *)((dev)->sd_addr + HDA_REG_##reg)) = value

#define azx_sd_readl(dev,reg) *((volatile int32_t *)((dev)->sd_addr + HDA_REG_##reg))
#define azx_sd_readw(dev,reg) *((volatile int16_t *)((dev)->sd_addr + HDA_REG_##reg))
#define azx_sd_readb(dev,reg) *((volatile uint8_t *)((dev)->sd_addr + HDA_REG_##reg))

#define codec_param_read(codec,nid,param) hda_codec_read(codec,nid,0,AC_VERB_PARAMETERS,param)

static void update_pci_byte( struct pci_config_s *pci, unsigned int reg, unsigned char mask, unsigned char val)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned char data;

	data = pcibios_ReadConfig_Byte(pci, reg);
	data &= ~mask;
	data |= (val & mask);
	pcibios_WriteConfig_Byte(pci, reg, data);
}

static void azx_init_pci(struct intelhd_card_s *card)
/////////////////////////////////////////////////////
{
	unsigned int tmp;

	switch(card->board_driver_type) {
	case AZX_DRIVER_ATI:
		update_pci_byte( card->pci_dev, ATI_SB450_HDAUDIO_MISC_CNTR2_ADDR,
			0x07, ATI_SB450_HDAUDIO_ENABLE_SNOOP ); // enable snoop
		break;
	case AZX_DRIVER_ATIHDMI_NS:
		update_pci_byte( card->pci_dev, ATI_SB450_HDAUDIO_MISC_CNTR2_ADDR,
			ATI_SB450_HDAUDIO_ENABLE_SNOOP, 0 ); // disable snoop
		break;
	case AZX_DRIVER_NVIDIA:
		update_pci_byte( card->pci_dev, NVIDIA_HDA_TRANSREG_ADDR,
			0x0f, NVIDIA_HDA_ENABLE_COHBITS );
		update_pci_byte( card->pci_dev, NVIDIA_HDA_ISTRM_COH,
			0x01, NVIDIA_HDA_ENABLE_COHBIT );
		update_pci_byte( card->pci_dev, NVIDIA_HDA_OSTRM_COH,
			0x01, NVIDIA_HDA_ENABLE_COHBIT );
		break;
	case AZX_DRIVER_SCH:
	case AZX_DRIVER_PCH:
	case AZX_DRIVER_SKL:
	case AZX_DRIVER_HDMI:
		tmp = pcibios_ReadConfig_Word(card->pci_dev, INTEL_SCH_HDA_DEVC);
		if(tmp & INTEL_SCH_HDA_DEVC_NOSNOOP)
			pcibios_WriteConfig_Word(card->pci_dev, INTEL_SCH_HDA_DEVC, tmp & (~INTEL_SCH_HDA_DEVC_NOSNOOP));
		break;
	case AZX_DRIVER_ULI:
		tmp = pcibios_ReadConfig_Word(card->pci_dev, INTEL_HDA_HDCTL);
		pcibios_WriteConfig_Word(card->pci_dev, INTEL_HDA_HDCTL, tmp | 0x10);
		pcibios_WriteConfig_Dword(card->pci_dev, INTEL_HDA_HDBARU, 0);
		break;
	}

	/* HDA chips uses memory mapping only */
	pcibios_enable_BM_MM(card->pci_dev);

	/* TCSEL, bits 0-2: select traffic class */
	if( card->pci_dev->vendor_id != 0x1002 ) // != ATI
		update_pci_byte(card->pci_dev, HDA_PCIREG_TCSEL, 0x07, 0); /* set TC0 */
}

static void azx_single_send_cmd(struct intelhd_card_s *chip,uint32_t val)
/////////////////////////////////////////////////////////////////////////
{
	int        timeout      = 2000; // 200 ms
	static int corbsizes[4] = {2, 16, 256, 0};
	int        corbsize     = corbsizes[(azx_readb( chip, CORBSIZE) & 0x3 )];
	int        corbindex    = azx_readw( chip, CORBWP ) & 0xFF;  /* get current CORB write pointer */
    uint8_t    corbrp       = azx_readw( chip, CORBRP ) & 0xFF;

	corbindex = (corbindex + 1) % corbsize;
	chip->corb_buffer[corbindex] = val;
	azx_writew(chip, CORBWP, corbindex); /* update buffer pointer */
	/* wait till cmd has been sent */
    for ( ; timeout && (corbrp == ( azx_readw( chip, CORBRP ) & 0xFF ) ); timeout--, pds_delay_10us(100) );
}

/* argument "direct" is always zero.
 * 12-bit verbs have a 8-bit "payload" (=parm);
 * 4-bits verb may have a 16-bit "payload";
 * since the 4-bit verbs are defined so that the lower 8bits are zero,
 * it's no problem.
 */

static void hda_codec_write(struct intelhd_card_s *chip, hda_nid_t nid, uint32_t direct, unsigned int verb, unsigned int parm)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	uint32_t val;

	val = (uint32_t)(chip->codec_index & 0x0f) << 28;
	val|= (uint32_t)direct << 27;
	val|= (uint32_t)nid << 20;
	val|= verb << 8;
	val|= parm;

	azx_single_send_cmd(chip, val);
}

/* send a cmd to codec and return response.
 * argument "direct" is always zero.
 */

static unsigned int hda_codec_read(struct intelhd_card_s *chip, hda_nid_t nid, uint32_t direct, unsigned int verb, unsigned int parm)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	int timeout = 2000; // 200 ms
	int rirbindex;
	uint16_t rirbwp = azx_readw( chip, RIRBWP );
	long long data;

	dbgprintf(( "hda_codec_read: write verb %X, parm=%X\n", verb, parm ));

	hda_codec_write( chip, nid, direct, verb, parm );

	//dbgprintf(( "hda_codec_read: waiting for response\n" ));
	for( ; timeout && ( rirbwp == azx_readw( chip, RIRBWP ) ); timeout--, pds_delay_10us(100) );
	if (!timeout) {
		dbgprintf(( "hda_codec_read: timeout waiting for codec response\n" ));
		return 0;
	}
	rirbindex = azx_readw( chip, RIRBWP );
	data = chip->rirb_buffer[rirbindex];
	//azx_writeb(chip, RIRBSTS, 1); /* writing a 1 clears bit 0! */
	return (unsigned int)data;
}

static void hda_codec_setup_stream(struct intelhd_card_s *chip, hda_nid_t nid, uint32_t stream_tag, int channel_id, int format)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	hda_codec_write(chip, nid, 0, AC_VERB_SET_CHANNEL_STREAMID, (stream_tag << 4) | channel_id);
	pds_delay_10us(100);
	hda_codec_write(chip, nid, 0, AC_VERB_SET_STREAM_FORMAT, format);
	pds_delay_10us(100);
}

static unsigned int hda_get_sub_nodes(struct intelhd_card_s *card, hda_nid_t nid, hda_nid_t *start_id)
//////////////////////////////////////////////////////////////////////////////////////////////////////
{
	int parm;

	parm = codec_param_read(card, nid, AC_PAR_NODE_COUNT);
	dbgprintf(("hda_get_sub_nodes: nid=%d, returns parm=%X\n", nid, parm ));
	if( parm < 0 )
		return 0;
	*start_id = (parm >> 16) & 0xff;
	return (parm & 0xff);
}

/* get audio function group node */

static void hda_search_audio_node(struct intelhd_card_s *card)
//////////////////////////////////////////////////////////////
{
	int i, total_nodes;
	hda_nid_t nid;

	card->afg_root_nodenum = 0;
	total_nodes = hda_get_sub_nodes(card, AC_NODE_ROOT, &nid);
	dbgprintf(( "hda_search_audio_node: nodes=%d\n",total_nodes ));
	for( i = 0; i < total_nodes; i++, nid++ ){
		if(( codec_param_read( card, nid, AC_PAR_FUNCTION_TYPE ) & 0xff) == AC_GRP_AUDIO_FUNCTION ) {
			card->afg_root_nodenum = nid;
			break;
		}
	}
	dbgprintf(( "hda_search_audio_node: exit, afg_nodenum=%d\n", (int)card->afg_root_nodenum ));
}

static int hda_get_connections(struct intelhd_card_s *card, hda_nid_t nid, hda_nid_t *conn_list, int max_conns)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int parm;
	int i, conn_len, conns;
	unsigned int shift, num_elems, mask;
	hda_nid_t prev_nid;

	parm = codec_param_read(card, nid, AC_PAR_CONNLIST_LEN);
	if (parm & AC_CLIST_LONG) {
		shift = 16;
		num_elems = 2;
	} else {
		shift = 8;
		num_elems = 4;
	}

	conn_len = parm & AC_CLIST_LENGTH;
	if(!conn_len)
		return 0;

	mask = (1 << (shift-1)) - 1;

	if(conn_len == 1){
		parm = hda_codec_read(card, nid, 0, AC_VERB_GET_CONNECT_LIST, 0);
		conn_list[0] = parm & mask;
		return 1;
	}

	conns = 0;
	prev_nid = 0;
	for (i = 0; i < conn_len; i++) {
		int range_val;
		hda_nid_t val, n;

		if (i % num_elems == 0)
			parm = hda_codec_read(card, nid, 0,AC_VERB_GET_CONNECT_LIST, i);

		range_val = !!(parm & (1 << (shift-1)));
		val = parm & mask;
		parm >>= shift;
		if(range_val) {
			if(!prev_nid || prev_nid >= val)
				continue;
			for(n = prev_nid + 1; n <= val; n++) {
				if(conns >= max_conns)
					return -1;
				conn_list[conns++] = n;
			}
		}else{
			if (conns >= max_conns)
				return -1;
			conn_list[conns++] = val;
		}
		prev_nid = val;
	}
	return conns;
}

static int hda_add_node(struct intelhd_card_s *card, struct hda_gnode *node, hda_nid_t nid)
///////////////////////////////////////////////////////////////////////////////////////////
{
	int nconns = 0;

	node->nid = nid;
	node->wid_caps = codec_param_read( card, nid, AC_PAR_AUDIO_WIDGET_CAP );
	node->type = (node->wid_caps & AC_WCAP_TYPE) >> AC_WCAP_TYPE_SHIFT;

	//dbgprintf(("hda_add_node: nid=%u, caps=%X\n", node->nid, node->wid_caps ));
	if( node->wid_caps & AC_WCAP_CONN_LIST )
		nconns = hda_get_connections(card, nid,&node->conn_list[0],HDA_MAX_CONNECTIONS);

	//dbgprintf(("hda_add_node: conns=%u\n", nconns ));
	if( nconns >= 0 ) {
		node->nconns = nconns;

		if( node->type == AC_WID_PIN ){
			node->pin_caps = codec_param_read(card, node->nid, AC_PAR_PIN_CAP);
			node->pin_ctl = hda_codec_read(card, node->nid, 0, AC_VERB_GET_PIN_WIDGET_CONTROL, 0);
			node->def_cfg = hda_codec_read(card, node->nid, 0, AC_VERB_GET_CONFIG_DEFAULT, 0);
			/* dbgprintf(("hda_add_node: PIN id:%d caps:%X conn:%d ctl:%X caps:%X def:%X\n",
				  (int)nid, node->wid_caps, nconns, (int)node->pin_ctl, node->pin_caps, node->def_cfg, */
		}

		if( node->wid_caps & AC_WCAP_OUT_AMP ) {
			if(node->wid_caps & AC_WCAP_AMP_OVRD)
				node->amp_out_caps = codec_param_read(card, node->nid, AC_PAR_AMP_OUT_CAP);
			if(!node->amp_out_caps)
				node->amp_out_caps = card->def_amp_out_caps;
			dbgprintf(("hda_add_node: caps & OUT_AMP, amp_out_caps=%X\n", node->amp_out_caps ));
		}

		if( node->wid_caps & AC_WCAP_IN_AMP ) {
			if(node->wid_caps & AC_WCAP_AMP_OVRD)
				node->amp_in_caps = codec_param_read(card, node->nid, AC_PAR_AMP_IN_CAP);
			if(!node->amp_in_caps)
				node->amp_in_caps = card->def_amp_in_caps;
			//dbgprintf(("hda_add_node: caps & IN_AMP, amp_in_caps=%X\n", node->amp_in_caps ));
		}

		if( node->wid_caps & AC_WCAP_FORMAT_OVRD ) {
			node->supported_formats = codec_param_read(card, node->nid, AC_PAR_PCM);
		}
	}

	return nconns;
}

static struct hda_gnode *hda_get_node(struct intelhd_card_s *card, hda_nid_t nid)
/////////////////////////////////////////////////////////////////////////////////
{
	struct hda_gnode *node = card->afg_nodes;
	unsigned int i;

	for( i = 0; i < card->afg_num_nodes; i++,node++ )
		if( node->nid == nid )
			return node;

	return NULL;
}

/* called by hda_codec_amp_stereo() and hda_writeMIXER() */

static void hda_set_vol_mute(struct intelhd_card_s *card, hda_nid_t nid, int ch, int direction, int index, uint8_t val)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	uint32_t parm;

	parm  = (ch) ? AC_AMP_SET_RIGHT : AC_AMP_SET_LEFT; /* bits 12/13 */
	parm |= (direction == HDA_OUTPUT) ? AC_AMP_SET_OUTPUT : AC_AMP_SET_INPUT; /* bits 14/15 */
	parm |= index << AC_AMP_SET_INDEX_SHIFT; /* bits 8-11 */
	/* bit 7=1 is mute! */
	parm |= val; /* bits 0-6 */
	dbgprintf(("hda_set_vol_mute(chnl=%u, dir=%u, idx=%u val=%X): write_codec( %u, %X, %X)\n", ch, direction, index, val, nid, AC_VERB_SET_AMP_GAIN_MUTE, parm ));
	hda_codec_write(card, nid, 0, AC_VERB_SET_AMP_GAIN_MUTE, parm);
}

/* called by hda_codec_amp_stereo() and hda_readMIXER() */

static uint8_t hda_get_vol_mute(struct intelhd_card_s *card, hda_nid_t nid, int ch, int direction, int index)
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	uint32_t parm;
	uint8_t val;

	parm  = (ch) ? AC_AMP_GET_RIGHT : AC_AMP_GET_LEFT;
	parm |= ( direction == HDA_OUTPUT ) ? AC_AMP_GET_OUTPUT : AC_AMP_GET_INPUT;
	parm |= index;
	val = (uint8_t)hda_codec_read(card, nid, 0, AC_VERB_GET_AMP_GAIN_MUTE, parm);
	return ( val ); /* bit 7: mute, 0-6: gain */
}

/* called by hda_unmute_output() and hda_unmute_input() */

static int hda_codec_amp_stereo(struct intelhd_card_s *card, hda_nid_t nid, int direction, int idx, uint8_t val)
////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	int ch;
	for ( ch = 0; ch < 2; ch++ ) {
		val |= hda_get_vol_mute( card, nid, ch, direction, idx );
		hda_set_vol_mute( card, nid, ch, direction, idx, val & 0x7f );
	}
	return 1;
}

/* unmute an output widget ( headphone, speaker,... )
 * called by parse_output_path() & parse_output_jack()
 */

static void hda_unmute_output(struct intelhd_card_s *card, struct hda_gnode *node)
//////////////////////////////////////////////////////////////////////////////////
{
	uint8_t val = ((node->amp_out_caps >> AC_AMPCAP_NUM_STEPS_SHIFT) & AC_AMPCAP_NUM_STEPS_MASK);
	hda_codec_amp_stereo(card, node->nid, HDA_OUTPUT, 0, val);
}

static void hda_unmute_input(struct intelhd_card_s *card, struct hda_gnode *node, unsigned int index)
/////////////////////////////////////////////////////////////////////////////////////////////////////
{
	uint8_t val = ((node->amp_in_caps >> AC_AMPCAP_NUM_STEPS_SHIFT) & AC_AMPCAP_NUM_STEPS_MASK);
	hda_codec_amp_stereo(card, node->nid, HDA_INPUT, index, val);
}

static void select_input_connection(struct intelhd_card_s *card, struct hda_gnode *node, unsigned int index)
////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	hda_codec_write(card, node->nid, 0,AC_VERB_SET_CONNECT_SEL, index);
}

static void clear_check_flags(struct intelhd_card_s *card)
//////////////////////////////////////////////////////////
{
	struct hda_gnode *node = card->afg_nodes;
	unsigned int i;

	for( i = 0; i < card->afg_num_nodes; i++,node++ )
		node->checked = 0;
}

/* scan output path of a widget ( lineout, headphone, speaker )
 */

static int parse_output_path(struct intelhd_card_s *card,struct hda_gnode *node, int dac_idx)
/////////////////////////////////////////////////////////////////////////////////////////////
{
	int i, err;
	struct hda_gnode *child;

	if(node->checked)
		return 0;

#if SETPOWERSTATE /* seems necessary on (some?) notebooks */
	hda_codec_write(card, node->nid, 0, AC_VERB_SET_POWER_STATE, 0 );
#endif
	node->checked = 1;
	if(node->type == AC_WID_AUD_OUT) {
		if(node->wid_caps & AC_WCAP_DIGITAL)
			return 0;
		if(card->dac_node[dac_idx])
			return (node == card->dac_node[dac_idx]);

		card->dac_node[dac_idx] = node;
		if((node->wid_caps & AC_WCAP_OUT_AMP) && (card->pcm_num_vols < MAX_PCM_VOLS)){
			card->pcm_vols[card->pcm_num_vols].node = node;
			card->pcm_vols[card->pcm_num_vols].index = 0;
			card->pcm_num_vols++;
		}
		return 1;
	}

	for( i = 0; i < node->nconns; i++ ) {
		child = hda_get_node(card, node->conn_list[i]);
		if(!child)
			continue;
		err = parse_output_path(card, child, dac_idx);
		if(err < 0)
			return err;
		else if( err > 0 ) {
			if( node->nconns > 1 )
				select_input_connection(card, node, i);
			hda_unmute_input(card, node, i);
			hda_unmute_output(card, node);
			if( card->dac_node[dac_idx] && ( card->pcm_num_vols < MAX_PCM_VOLS ) && !( card->dac_node[dac_idx]->wid_caps & AC_WCAP_OUT_AMP ) ) {
				if((node->wid_caps & AC_WCAP_IN_AMP) || (node->wid_caps & AC_WCAP_OUT_AMP)) {
					int n = card->pcm_num_vols;
					card->pcm_vols[n].node = node;
					card->pcm_vols[n].index = i;
					card->pcm_num_vols++;
				}
			}
			return 1;
		}
	}
	return 0;
}

/* check an output node
 */

static struct hda_gnode *parse_output_jack(struct intelhd_card_s *card, int jack_type )
///////////////////////////////////////////////////////////////////////////////////////
{
	struct hda_gnode *node = card->afg_nodes;
	int err,i;

	for( i = 0; i < card->afg_num_nodes; i++, node++ ){

		dbgprintf(("parse_output_jack(%u)[%u]: type=%X caps=%X\n", jack_type, i, node->type, node->pin_caps ));
		if(node->type != AC_WID_PIN)  /* widget must be a "pin" */
			continue;
		if(!(node->pin_caps & AC_PINCAP_OUT))
			continue;
		dbgprintf(("parse_output_jack[%u]: node id=%u cfg=0x%X\n", i, node->nid, node->def_cfg ));

		/* check port connectivity ( bits 30-31; 01=not connected ) */
		if(defcfg_port_conn(node) == AC_JACK_PORT_NONE)
			continue;

		if( jack_type >= 0 ) {
			if(jack_type != defcfg_type(node)) /* has widget the type we're searching? */
				continue;
			if(node->wid_caps & AC_WCAP_DIGITAL)
				continue;
		} else {
			if(!(node->pin_ctl & AC_PINCTL_OUT_ENABLE))
				continue;
		}
		dbgprintf(("parse_output_jack[%u]: widget %u preselected\n", i, node->nid ));
		clear_check_flags(card);
		err = parse_output_path(card, node, 0);
		if( err < 0 ) {
			dbgprintf(("parse_output_jack[%u]: parse_output_path() failed, err=%d\n", i, err ));
			return NULL;
		}
		/* ??? */
		if(!err && card->out_pin_node[0]){
			err = parse_output_path(card, node, 1);
			if( err < 0 ) {
				dbgprintf(("parse_output_jack[%u]: parse_output_path() failed [2], err=%d\n", i, err ));
				return NULL;
			}
		}
		if( err > 0 ){
			hda_unmute_output(card, node);
#if SETPOWERSTATE
			hda_codec_write(card, node->nid, 0, AC_VERB_SET_POWER_STATE, 0 );
#endif
			hda_codec_write(card, node->nid, 0,
								AC_VERB_SET_PIN_WIDGET_CONTROL,
								AC_PINCTL_OUT_ENABLE |
								((node->pin_caps & AC_PINCAP_HP_DRV)? AC_PINCTL_HP_ENABLE : 0));
			dbgprintf(("parse_output_jack: found node, id=%u, type=%u\n", node->nid, jack_type ));
			return node;
		}
	}
	dbgprintf(("parse_output_jack: nothing found\n" ));
	return NULL;
}

static void hda_enable_eapd(struct intelhd_card_s *card, struct hda_gnode *node)
////////////////////////////////////////////////////////////////////////////////
{
	if(node->pin_caps & AC_PINCAP_EAPD){
		unsigned int eapd_set = hda_codec_read(card, node->nid, 0, AC_VERB_GET_EAPD_BTLENABLE, 0);
		eapd_set |= AC_PINCTL_EAPD_ENABLE;
		hda_codec_write(card, node->nid, 0, AC_VERB_SET_EAPD_BTLENABLE, eapd_set);
	}
}

/* scan codec for ouput devices:
 * lineout, speaker, headphones
 */

static int hda_parse_output(struct intelhd_card_s *card)
////////////////////////////////////////////////////////
{
	int i;
	static char *dtstrings[] = {"lineout","speaker","headphone"};
	int8_t prefered_devtype;
	struct hda_gnode *node;
	int8_t *po,parseorder_line[] = {AC_JACK_LINE_OUT, AC_JACK_HP_OUT, -1};
	int8_t parseorder_speaker[] = {AC_JACK_SPEAKER, AC_JACK_HP_OUT, AC_JACK_LINE_OUT, -1};

    /* scan for output device according to /O option */
	switch ( card->config_select & AUCARDSCONFIG_IHD_OUT_DEV_MASK ) {
	case 0: po = parseorder_line; break;
	case 1: po = parseorder_speaker; break;
	case 2: po = &parseorder_speaker[1]; break;
	}

	prefered_devtype = *po;
	i = 0;
	do{
		node = parse_output_jack(card, *po);
		if( node ) {
			card->out_pin_node[i++] = node;
			if((*po == AC_JACK_SPEAKER) || (*po == AC_JACK_HP_OUT))
				hda_enable_eapd(card, node);
		}
		po++;
	}while((i < MAX_PCM_VOLS) && (*po >= 0));

	if(!card->out_pin_node[0]){ // should not happen
		node = parse_output_jack(card, -1); // parse 1st output
		if(!node)
			return 0;
		card->out_pin_node[0] = node;
	}
	/* in case the selected output type isn't the one wanted, tell it */
	if ( defcfg_type(card->out_pin_node[0]) != prefered_devtype )
		printf("Pin %u (%s) used for output\n", card->out_pin_node[0]->nid, dtstrings[defcfg_type(card->out_pin_node[0])] );

	return 1;
}

static unsigned long hda_get_max_freq(struct intelhd_card_s *card)
//////////////////////////////////////////////////////////////////
{
	unsigned long i,freq = 0;
	for( i = 0; rate_bits[i].hz; i++)
		if((card->supported_formats & ( 1 << i)) && (rate_bits[i].hz < 0xffffffff))
			freq = rate_bits[i].hz;
	return freq;
}

static unsigned int hda_get_max_bits(struct intelhd_card_s *card)
/////////////////////////////////////////////////////////////////
{
	unsigned int bits = 16;
	if(card->supported_formats & AC_SUPPCM_BITS_32)
		bits = 32;
	else if(card->supported_formats & AC_SUPPCM_BITS_24)
		bits = 24;
	else if(card->supported_formats & AC_SUPPCM_BITS_20)
		bits = 20;
	return bits;
}

/* called by HDA_adetect() */

static unsigned int hda_buffer_init( struct audioout_info_s *aui, struct intelhd_card_s *card )
///////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int bytes_per_sample = (aui->bits_set > 16) ? 4:2;
	unsigned long allbufsize = BDL_SIZE + 1024 + (HDA_CORB_MAXSIZE + HDA_CORB_ALIGN + HDA_RIRB_MAXSIZE + HDA_RIRB_ALIGN), gcap, sdo_offset;
	unsigned int beginmem_aligned;

	dbgprintf(("hda_buffer_init: HDA data struct size=0x%X\n", allbufsize ));

	allbufsize += card->pcmout_bufsize = MDma_get_max_pcmoutbufsize( aui, 0, AZX_PERIOD_SIZE, bytes_per_sample * aui->chan_card / 2, aui->freq_set);
	card->dm = MDma_alloc_cardmem( allbufsize );
	if(!card->dm)
		return 0;
	beginmem_aligned = (((unsigned long)card->dm->pMem + 1023) & (~1023));
	card->table_buffer = (uint32_t *)beginmem_aligned;
	card->pcmout_buffer = (char *)(beginmem_aligned + BDL_SIZE);
	card->corb_buffer = (unsigned long *)(((uint32_t)card->pcmout_buffer + card->pcmout_bufsize + HDA_CORB_ALIGN - 1) & (~(HDA_CORB_ALIGN - 1)));
	card->rirb_buffer = (unsigned long long *)(((uint32_t)card->corb_buffer + HDA_CORB_MAXSIZE + HDA_RIRB_ALIGN - 1) & (~(HDA_RIRB_ALIGN - 1)));

    /* set offset for access to output stream descriptor; the first output stream descriptor is used */

	gcap = (unsigned long)azx_readw( card, GCAP);
	if(!(card->config_select & AUCARDSCONFIG_IHD_USE_FIXED_SDO) && (gcap & 0xF000)) // number of playback streams
		/* 0x80=offset start SDs in HDA, 0x20=size SD, gcap[8-11]=#input SDs */
		sdo_offset = ((gcap >> 8) & 0x0F) * 0x20 + 0x80; // number of capture streams
	else{
		switch(card->board_driver_type){
		case AZX_DRIVER_ATIHDMI:
		case AZX_DRIVER_ATIHDMI_NS:
			sdo_offset = 0x80; break;
		case AZX_DRIVER_TERA:
			sdo_offset = 0xe0; break;
		case AZX_DRIVER_ULI:
			sdo_offset = 0x120; break;
		default:
			sdo_offset = 0x100;break;
		}
	}

	dbgprintf(("hda_buffer_init: cs=%X csc=%d GCAP=%X SD-ofs=%X\n",
				   card->config_select, aui->card_select_config, gcap, sdo_offset));

	card->sd_addr = card->iobase + sdo_offset;
	card->pcmout_period_size = AZX_PERIOD_SIZE;
	card->pcmout_num_periods = card->pcmout_bufsize / card->pcmout_period_size;

	return 1;
}

/* called by hda_hw_init() */

static unsigned int azx_reset(struct intelhd_card_s *chip)
//////////////////////////////////////////////////////////
{
	int timeout;

	dbgprintf(("azx_reset: GCTL=%X\n", azx_readl(chip, GCTL)));

	/* wake up the HDA controller if it is in "reset" state */
	if ( !(azx_readl(chip, GCTL ) & HDA_GCTL_RESET )) {
		azx_writel(chip, GCTL, azx_readl(chip, GCTL) | HDA_GCTL_RESET );

		timeout = 500;
		while( (!azx_readl(chip, GCTL) & HDA_GCTL_RESET) && (--timeout))
			pds_delay_10us(100);
		if( !timeout ) {
			dbgprintf(("HDA controller not ready!\n"));
			return 0;
		}
	}
	// disable unsolicited responses (single cmd mode)
	azx_writel(chip, GCTL, (azx_readl(chip, GCTL) & (~HDA_GCTL_UREN)));

    /* reset CORB & RIRB */
    azx_writew( chip, CORBCTL, azx_readw( chip, CORBCTL ) & ~2 );
	azx_writew( chip, RIRBCTL, azx_readw( chip, CORBCTL ) & ~2 );
	for( timeout = 100;
		(azx_readw(chip, CORBCTL) & 2) && timeout;
		 timeout--, pds_delay_10us(10));

	/* STATESTS decides what codecs will be tried.
	 * Since this fields is writeable ( write '1' to clear bits ),
	 * it might be a good idea to reset the HDA if STATESTS is 0.
	 */
	chip->codec_mask = azx_readw(chip, STATESTS);

	// set CORB command DMA buffer
	azx_writel(chip, CORBLBASE, pds_cardmem_physicalptr(chip->dm, chip->corb_buffer));
	azx_writel(chip, CORBUBASE, 0 );
	azx_writew(chip, CORBWP, 0 );

	/* to reset CORB RP, set/reset bit 15 */
	azx_writew(chip, CORBRP, (int16_t)0x8000 ); /* OW needs a type cast for constant */
	/* according to specs wait until bit 15 changes to 1. then write a
	 * 0 and again wait until a 0 is read.
	 */
#if 1
	timeout = 500;
	while ( ( 0 == (azx_readw( chip, CORBRP ) & 0x8000 ) ) && timeout--) {
		pds_delay_10us(100);
	}
#endif
	azx_writew(chip, CORBRP, 0 );
#if 1
	timeout = 500;
	while ( (azx_readw( chip, CORBRP ) & 0x8000 ) && timeout-- ) {
		pds_delay_10us(100);
	}
#endif
	//azx_writeb(chip, CORBSIZE, 0);

	azx_writel(chip, RIRBLBASE, pds_cardmem_physicalptr(chip->dm, chip->rirb_buffer));
	azx_writel(chip, RIRBUBASE, 0 );
	azx_writew(chip, RIRBWP, (int16_t)0x8000 ); /* reset RIRB write pointer */
	//azx_writeb(chip, RIRBSIZE, 0); maybe only 1 supported
	azx_writew(chip, RIRBRIC, 64); //1 response for one interrupt each time

	pds_delay_10us(100);

	dbgprintf(("azx_reset: done, codec_mask:%X\n",chip->codec_mask));

	return 1;
}

static void hda_hw_init(struct intelhd_card_s *card)
////////////////////////////////////////////////////
{
	azx_init_pci(card);
	azx_reset(card);

	/* reset int errors by writing '1's in SD_STS */
	azx_sd_writeb(card, SD_STS, SD_INT_MASK);

	/* should not be written - writing '1' clears bits.
     * and STATESTS_INT_MASK is 0x7?
	 */
	//azx_writeb(card, STATESTS, STATESTS_INT_MASK);

	azx_writeb(card, RIRBSTS, RIRB_INT_MASK);

	/* set interrupt control register; don't enable all stream interrupts - will be done more specific later */
	//azx_writel(card, INTCTL, azx_readl(card, INTCTL) | HDA_INT_CTRL_EN | HDA_INT_GLOBAL_EN | HDA_INT_ALL_STREAM);
	azx_writel(card, INTCTL, azx_readl(card, INTCTL) | HDA_INT_CTRL_EN | HDA_INT_GLOBAL_EN );
	/* interrupt status register is RO! */
	//azx_writel(card, INTSTS, HDA_INT_CTRL_EN | HDA_INT_ALL_STREAM);

	azx_writel(card, DPLBASE, 0);
	azx_writel(card, DPUBASE, 0);

	dbgprintf(("hda_hw_init: STATESTS=%X INTCTL=%X INTSTS=%X\n", azx_readw( card, STATESTS ), azx_readl( card, INTCTL ), azx_readl( card, INTSTS ) ));
	dbgprintf(("hda_hw_init: CORB base=%X wp=%X rp=%X ctl=%X sts=%X siz=%X\n",
		azx_readl( card, CORBLBASE ), azx_readw( card, CORBWP ), azx_readw( card, CORBRP ), azx_readb( card, CORBCTL ), azx_readb( card, CORBSTS ),  azx_readb( card, CORBSIZE )));
	dbgprintf(("hda_hw_init: RIRB base=%X wp=%X ric=%X ctl=%X sts=%X siz=%X\n",
		azx_readl( card, RIRBLBASE ), azx_readw( card, RIRBWP ), azx_readw( card, RIRBRIC ), azx_readb( card, RIRBCTL ), azx_readb( card, RIRBSTS ),  azx_readb( card, RIRBSIZE )));

}

static unsigned int hda_mixer_init(struct intelhd_card_s *card)
///////////////////////////////////////////////////////////////
{
	unsigned int i;
	hda_nid_t nid;

	dbgprintf(("hda_mixer_init: reading codec vendor id...\n"));
	card->codec_vendor_id = codec_param_read(card, AC_NODE_ROOT, AC_PAR_VENDOR_ID);
	if( card->codec_vendor_id <= 0 )
		card->codec_vendor_id = codec_param_read(card, AC_NODE_ROOT, AC_PAR_VENDOR_ID);

	dbgprintf(("hda_mixer_init: codec vendor id=%X, searching afg node...\n",card->codec_vendor_id));
	hda_search_audio_node( card );
	if( !card->afg_root_nodenum ) {
		dbgprintf(("hda_mixer_init: no afg found\n"));
		goto err_out_mixinit;
	}

#if SETPOWERSTATE
	hda_codec_write(card, card->afg_root_nodenum, 0, AC_VERB_SET_POWER_STATE, 0 );
#endif

	card->def_amp_out_caps = codec_param_read(card, card->afg_root_nodenum, AC_PAR_AMP_OUT_CAP);
	card->def_amp_in_caps = codec_param_read(card, card->afg_root_nodenum, AC_PAR_AMP_IN_CAP);
	card->afg_num_nodes = hda_get_sub_nodes(card, card->afg_root_nodenum, &nid);
	if((card->afg_num_nodes <= 0) || !nid) {
		dbgprintf(("hda_mixer_init: no afg sub nodes\n"));
		goto err_out_mixinit;
	}

	dbgprintf(("hda_mixer_init: outcaps=%X incaps=%X afgsubnodes=%d anid=%d\n",card->def_amp_out_caps,card->def_amp_in_caps,card->afg_num_nodes,(int)nid));

	card->afg_nodes = (struct hda_gnode *)pds_calloc(card->afg_num_nodes,sizeof(struct hda_gnode));
	if(!card->afg_nodes) {
		dbgprintf(("hda_mixer_init: malloc failed\n"));
		goto err_out_mixinit;
	}
	for( i = 0; i < card->afg_num_nodes; i++, nid++ )
		hda_add_node(card, &card->afg_nodes[i], nid);

	/* get the widgets we need for the selected ouput pin ( lineout/speaker/hp ) */
	if(!hda_parse_output(card)) {
		dbgprintf(("hda_mixer_init: hda_parse_output failed\n"));
		goto err_out_mixinit;
	}

	/* check if DAC/AFG support the format we need */
	if( card->dac_node[0] ) {
		card->supported_formats = card->dac_node[0]->supported_formats;
		if( !card->supported_formats )
			card->supported_formats = codec_param_read(card, card->afg_root_nodenum, AC_PAR_PCM);

		card->supported_max_freq = hda_get_max_freq(card);
		card->supported_max_bits = hda_get_max_bits(card);
		dbgprintf(("hda_mixer_init: supp formats=%X, max freq=%u, max bits=%u\n", card->supported_formats, card->supported_max_freq, card->supported_max_bits ));
	}

	/* set the card_mixerchans values */
	/* vsbhda: AU_getmixer expects submixch_max to be 2^n-1!
	 * IOW, it is used as both a mask and a max value - not a very good design;
	 * for HDA it's simply not usable. So a new bit has been introduced that causes
	 * the mixer code in AU_cards to NOT use submixch_max ( or submixch_shift ).
	 */
	for( i = 0; i < MAX_PCM_VOLS; i++)
		if( card->pcm_vols[i].node ) {
			hda_master_vol.submixerchans[i].submixch_register = card->pcm_vols[i].node->nid;
			//hda_master_vol.submixerchans[i].submixch_max = (card->pcm_vols[i].node->amp_out_caps >> AC_AMPCAP_NUM_STEPS_SHIFT ) & AC_AMPCAP_NUM_STEPS_MASK;
		}

	dbgprintf(("hda_mixer_init: dac[0]=%d dac[1]=%d out[0]=%d out[1]=%d vol[0]=%d vol[1]=%d\n",
				(int)((card->dac_node[0]) ? card->dac_node[0]->nid: 0),
				(int)((card->dac_node[1]) ? card->dac_node[1]->nid: 0),
				(int)((card->out_pin_node[0]) ? card->out_pin_node[0]->nid: 0),
				(int)((card->out_pin_node[1]) ? card->out_pin_node[1]->nid: 0),
				(int)((card->pcm_vols[0].node) ? card->pcm_vols[0].node->nid: 0),
				(int)((card->pcm_vols[1].node) ? card->pcm_vols[1].node->nid: 0)));

	dbgprintf(("hda_mixer_init: done\n"));

	return 1;

err_out_mixinit:
	if( card->afg_nodes ){
		pds_free(card->afg_nodes);
		card->afg_nodes = NULL;
	}
	dbgprintf(("mixer_init: failed\n"));
	return 0;
}

static void hda_hw_close(struct intelhd_card_s *card)
/////////////////////////////////////////////////////
{
	azx_writel(card, DPLBASE, 0);
	azx_writel(card, DPUBASE, 0);

	if ( card->sd_addr ) {
		azx_sd_writel(card, SD_BDLPL, 0);
		azx_sd_writel(card, SD_BDLPU, 0);
		azx_sd_writew(card, SD_CTL, 0); /* stop DMA engine for this stream */
	}
#if RESETCODECONCLOSE
	/* reset codec */
	//hda_codec_write(card, card->afg_root_nodenum, 0, AC_VERB_SET_CODEC_RESET, 0);
	/* stop CORB & RIRB DMA engines */
	azx_writeb( card, CORBCTL, 0 );
	azx_writeb( card, RIRBCTL, 0 );
	azx_writel( card, INTSTS, 0 );
	dbgprintf(("hda_hw_close: STATESTS=%X INTCTL=%X INTSTS=%X\n", azx_readw( card, STATESTS ), azx_readl( card, INTCTL ), azx_readl( card, INTSTS ) ));
	dbgprintf(("hda_hw_close: CORB base=%X wp=%X rp=%X ctl=%X sts=%X siz=%X\n",
			  azx_readl( card, CORBLBASE ), azx_readw( card, CORBWP ), azx_readw( card, CORBRP ), azx_readb( card, CORBCTL ), azx_readb( card, CORBSTS ),  azx_readb( card, CORBSIZE )));
	dbgprintf(("hda_hw_close: RIRB base=%X wp=%X ric=%X ctl=%X sts=%X siz=%X\n",
			  azx_readl( card, RIRBLBASE ), azx_readw( card, RIRBWP ), azx_readw( card, RIRBRIC ), azx_readb( card, RIRBCTL ), azx_readb( card, RIRBSTS ),  azx_readb( card, RIRBSIZE )));
#endif
}

static void azx_setup_periods(struct intelhd_card_s *card)
//////////////////////////////////////////////////////////
{
	uint32_t *bdl = card->table_buffer;
	unsigned int i;

	card->pcmout_num_periods = card->pcmout_dmasize / card->pcmout_period_size;

	dbgprintf(("azx_setup_periods: dmasize=%d periods=%d prsize=%d\n",card->pcmout_dmasize,card->pcmout_num_periods,card->pcmout_period_size));

	azx_sd_writel(card, SD_BDLPL, 0);
	azx_sd_writel(card, SD_BDLPU, 0);

	for( i = 0; i < card->pcmout_num_periods; i++ ) {
		unsigned int off  = i << 2;
		uint32_t addr = ( pds_cardmem_physicalptr(card->dm, card->pcmout_buffer)) + i * card->pcmout_period_size;
		*(&bdl[off+0]) = addr;
		*(&bdl[off+1]) = 0;
		*(&bdl[off+2]) = card->pcmout_period_size;
#ifdef SBEMU
		*(&bdl[off+3]) = 0x01;
#else
		*(&bdl[off+3]) = 0x00; // 0x01 enable interrupt
#endif
 }
}

/* called by HDA_setrate() */

static void azx_setup_stream(struct intelhd_card_s *card)
/////////////////////////////////////////////////////////
{
	unsigned char val;
	unsigned int stream_tag = 1;
	int timeout;

	/* stop streams DMA engine */
	azx_sd_writeb(card, SD_CTL, azx_sd_readb(card, SD_CTL) & ~SD_CTL_DMA_START);
	/* reset stream */
	azx_sd_writeb(card, SD_CTL, azx_sd_readb(card, SD_CTL) | SD_CTL_STREAM_RESET);
	pds_delay_10us(100);

	/* wait till stream is in reset state */
	timeout = 300;
	while(!((val = azx_sd_readb(card, SD_CTL)) & SD_CTL_STREAM_RESET) && --timeout)
		pds_delay_10us(100);

#ifdef _DEBUG
	if ( !timeout )
		dbgprintf(("azx_setup_stream: stream reset timeout\n"));
#endif	

	 /* get stream out of reset state */
	val &= ~SD_CTL_STREAM_RESET;
	azx_sd_writeb(card, SD_CTL, val);
	pds_delay_10us(100);

	timeout = 300; /* wait till stream is ready */
	while(((val = azx_sd_readb(card, SD_CTL)) & SD_CTL_STREAM_RESET) && --timeout)
		pds_delay_10us(100);

#ifdef _DEBUG
	if ( !timeout )
		dbgprintf(("azx_setup_stream: stream ready timeout\n"));
#endif
	/* set stream# to use */
	azx_sd_writel(card, SD_CTL, (azx_sd_readl(card, SD_CTL) & ~SD_CTL_STREAM_TAG_MASK) | (stream_tag << SD_CTL_STREAM_TAG_SHIFT));
	azx_sd_writel(card, SD_CBL, card->pcmout_dmasize);
	azx_sd_writew(card, SD_LVI, card->pcmout_num_periods - 1);
	azx_sd_writew(card, SD_FORMAT, card->format_val);
	azx_sd_writel(card, SD_BDLPL, pds_cardmem_physicalptr(card->dm, card->table_buffer));
	azx_sd_writel(card, SD_BDLPU, 0); // upper 32 bit
	//azx_sd_writel(card, SD_CTL, azx_sd_readl(card, SD_CTL) | SD_INT_MASK);
#ifdef SBEMU
	/* set stream int mask; now done later in setrate() */
	//azx_sd_writel(card, SD_CTL, azx_sd_readl(card, SD_CTL) | SD_INT_COMPLETE);
#endif
	pds_delay_10us(100);

	if(card->dac_node[0])
		hda_codec_setup_stream(card, card->dac_node[0]->nid, stream_tag, 0, card->format_val);
	if(card->dac_node[1])
		hda_codec_setup_stream(card, card->dac_node[1]->nid, stream_tag, 0, card->format_val);
}

/* called by HDA_setrate() */

static unsigned int hda_calc_stream_format( struct audioout_info_s *aui, struct intelhd_card_s *card )
//////////////////////////////////////////////////////////////////////////////////////////////////////
{
	unsigned int i,val = 0;
	if(card->supported_max_freq && (aui->freq_card > card->supported_max_freq))
		aui->freq_card = card->supported_max_freq;

	for(i = 0; rate_bits[i].hz; i++)
		/* update freq_card with the first supported value thats >= current freq_card */
		if( (aui->freq_card <= rate_bits[i].hz) && (card->supported_formats & ( 1 << i ))) {
			aui->freq_card = rate_bits[i].hz;
			val = rate_bits[i].hda_fmt;
			break;
		}
	dbgprintf(("hda_calc_stream_format: freq_card=%u, supp formats=%X\n", aui->freq_card, card->supported_formats ));

	val |= aui->chan_card - 1;

	if(card->dacout_num_bits > card->supported_max_bits)
		card->dacout_num_bits = card->supported_max_bits;

	if((card->dacout_num_bits <= 16) && (card->supported_formats & AC_SUPPCM_BITS_16)){
		val |= 0x10; card->dacout_num_bits = 16; aui->bits_card = 16;
	}else if((card->dacout_num_bits <= 20) && (card->supported_formats & AC_SUPPCM_BITS_20)){
		val |= 0x20; card->dacout_num_bits = 20; aui->bits_card = 32;
	}else if((card->dacout_num_bits <= 24) && (card->supported_formats & AC_SUPPCM_BITS_24)){
		val |= 0x30; card->dacout_num_bits = 24; aui->bits_card = 32;
	}else if((card->dacout_num_bits <= 32) && (card->supported_formats & AC_SUPPCM_BITS_32)){
		val |= 0x40; card->dacout_num_bits = 32; aui->bits_card = 32;
	}

	return val;
}

//-------------------------------------------------------------------------

static const struct pci_device_s intelhda_devices[] = {
 {"Intel CPT6",                  0x8086, 0x1c20, AZX_DRIVER_PCH },
 {"Intel CPT7 (PBG)",            0x8086, 0x1d20, AZX_DRIVER_PCH },
 {"Intel PCH (Panther Point)",   0x8086, 0x1e20, AZX_DRIVER_PCH },
 {"Intel PCH (Lynx Point)",      0x8086, 0x8c20, AZX_DRIVER_PCH },
 {"Intel PCH (9 Series)",        0x8086, 0x8ca0, AZX_DRIVER_PCH },
 {"Intel PCH (Wellsburg)",       0x8086, 0x8d20, AZX_DRIVER_PCH },
 {"Intel PCH (Wellsburg)",       0x8086, 0x8d21, AZX_DRIVER_PCH },
 {"Intel PCH (Lewisburg)",       0x8086, 0xa1f0, AZX_DRIVER_PCH },
 {"Intel PCH (Lewisburg)",       0x8086, 0xa270, AZX_DRIVER_PCH },
 {"Intel PCH (Lynx Point-LP)",   0x8086, 0x9c20, AZX_DRIVER_PCH },
 {"Intel PCH (Lynx Point-LP)",   0x8086, 0x9c21, AZX_DRIVER_PCH },
 {"Intel PCH (Wildcat Point-LP)",0x8086, 0x9ca0, AZX_DRIVER_PCH },
 {"Intel SKL (Sunrise Point)",   0x8086, 0xa170, AZX_DRIVER_SKL },
 {"Intel SKL (Sunrise Point-LP)",0x8086, 0x9d70, AZX_DRIVER_SKL },
 {"Intel SKL (Kabylake)",        0x8086, 0xa171, AZX_DRIVER_SKL },
 {"Intel SKL (Kabylake-LP)",     0x8086, 0x9d71, AZX_DRIVER_SKL },
 {"Intel SKL (Kabylake-H)",      0x8086, 0xa2f0, AZX_DRIVER_SKL },
 {"Intel SKL (Coffelake)",       0x8086, 0xa348, AZX_DRIVER_SKL },
 {"Intel SKL (Cannonlake)",      0x8086, 0x9dc8, AZX_DRIVER_SKL },
 {"Intel SKL (CometLake-LP)",    0x8086, 0x02C8, AZX_DRIVER_SKL },
 {"Intel SKL (CometLake-H)",     0x8086, 0x06C8, AZX_DRIVER_SKL },
 {"Intel SKL (CometLake-H)",     0x8086, 0xf1c8, AZX_DRIVER_SKL },
 {"Intel SKL (CometLake-S)",     0x8086, 0xa3f0, AZX_DRIVER_SKL },
 {"Intel SKL (CometLake-R)",     0x8086, 0xf0c8, AZX_DRIVER_SKL },
 {"Intel SKL (Icelake)",         0x8086, 0x34c8, AZX_DRIVER_SKL },
 {"Intel SKL (Icelake-H)",       0x8086, 0x3dc8, AZX_DRIVER_SKL },
 {"Intel SKL (Jasperlake)",      0x8086, 0x38c8, AZX_DRIVER_SKL },
 {"Intel SKL (Jasperlake)",      0x8086, 0x4dc8, AZX_DRIVER_SKL },
 {"Intel SKL (Tigerlake)",       0x8086, 0xa0c8, AZX_DRIVER_SKL },
 {"Intel SKL (Tigerlake-H)",     0x8086, 0x43c8, AZX_DRIVER_SKL },
 {"Intel SKL (DG1)",             0x8086, 0x490d, AZX_DRIVER_SKL },
 {"Intel SKL (Alderlake-S)",     0x8086, 0x7ad0, AZX_DRIVER_SKL },
 {"Intel SKL (Alderlake-P)",     0x8086, 0x51c8, AZX_DRIVER_SKL },
 {"Intel SKL (Alderlake-M)",     0x8086, 0x51cc, AZX_DRIVER_SKL },
 {"Intel SKL (Elkhart Lake)",    0x8086, 0x4b55, AZX_DRIVER_SKL },
 {"Intel SKL (Elkhart Lake)",    0x8086, 0x4b58, AZX_DRIVER_SKL },
 {"Intel SKL (Broxton-P)",       0x8086, 0x5a98, AZX_DRIVER_SKL },
 {"Intel SKL (Broxton-T)",       0x8086, 0x1a98, AZX_DRIVER_SKL },
 {"Intel SKL (Gemini-Lake)",     0x8086, 0x3198, AZX_DRIVER_SKL },
 {"Intel HDMI (Haswell)",        0x8086, 0x0a0c, AZX_DRIVER_HDMI },
 {"Intel HDMI (Haswell)",        0x8086, 0x0c0c, AZX_DRIVER_HDMI },
 {"Intel HDMI (Haswell)",        0x8086, 0x0d0c, AZX_DRIVER_HDMI },
 {"Intel HDMI (Broadwell)",      0x8086, 0x160c, AZX_DRIVER_HDMI },
 {"Intel SCH (5 Series/3400)",   0x8086, 0x3b56, AZX_DRIVER_SCH },
 {"Intel SCH (Poulsbo)",         0x8086, 0x811b, AZX_DRIVER_SCH },
 {"Intel SCH (Oaktrail)",        0x8086, 0x080a, AZX_DRIVER_SCH },
 {"Intel PCH (BayTrail)",        0x8086, 0x0f04, AZX_DRIVER_PCH },
 {"Intel PCH (Braswell)",        0x8086, 0x2284, AZX_DRIVER_PCH },
 {"Intel ICH6",   0x8086, 0x2668, AZX_DRIVER_ICH },
 {"Intel ICH7",   0x8086, 0x27d8, AZX_DRIVER_ICH },
 {"Intel ESB2",   0x8086, 0x269a, AZX_DRIVER_ICH },
 {"Intel ICH8",   0x8086, 0x284b, AZX_DRIVER_ICH },
 {"Intel ICH",    0x8086, 0x2911, AZX_DRIVER_ICH },
 {"Intel ICH9",   0x8086, 0x293e, AZX_DRIVER_ICH },
 {"Intel ICH9",   0x8086, 0x293f, AZX_DRIVER_ICH },
 {"Intel ICH10",  0x8086, 0x3a3e, AZX_DRIVER_ICH },
 {"Intel ICH10",  0x8086, 0x3a6e, AZX_DRIVER_ICH },
 {"ATI SB450",    0x1002, 0x437b, AZX_DRIVER_ATI },
 {"ATI SB600",    0x1002, 0x4383, AZX_DRIVER_ATI },
 {"AMD Hudson",   0x1022, 0x780d, AZX_DRIVER_ATI }, // snoop type is ATI
 {"AMD X370 & co",0x1022, 0x1457, AZX_DRIVER_ATI }, //
 {"AMD X570 & co",0x1022, 0x1487, AZX_DRIVER_ATI }, //
 {"AMD Stoney",   0x1022, 0x157a, AZX_DRIVER_ATI }, //
 {"AMD Raven",    0x1022, 0x15e3, AZX_DRIVER_ATI }, //
 {"ATI HDNS",     0x1002, 0x0002, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0x1308, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0x157a, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0x15b3, AZX_DRIVER_ATIHDMI_NS },
 {"ATI RS600",    0x1002, 0x793b, AZX_DRIVER_ATIHDMI },
 {"ATI RS690",    0x1002, 0x7919, AZX_DRIVER_ATIHDMI },
 {"ATI RS780",    0x1002, 0x960f, AZX_DRIVER_ATIHDMI },
 {"ATI RS880",    0x1002, 0x970f, AZX_DRIVER_ATIHDMI },
 {"ATI HDNS",     0x1002, 0x9840, AZX_DRIVER_ATIHDMI_NS },
 {"ATI R600",     0x1002, 0xaa00, AZX_DRIVER_ATIHDMI },
 {"ATI RV630",    0x1002, 0xaa08, AZX_DRIVER_ATIHDMI },
 {"ATI RV610",    0x1002, 0xaa10, AZX_DRIVER_ATIHDMI },
 {"ATI RV670",    0x1002, 0xaa18, AZX_DRIVER_ATIHDMI },
 {"ATI RV635",    0x1002, 0xaa20, AZX_DRIVER_ATIHDMI },
 {"ATI RV620",    0x1002, 0xaa28, AZX_DRIVER_ATIHDMI },
 {"ATI RV770",    0x1002, 0xaa30, AZX_DRIVER_ATIHDMI },
 {"ATI RV710",    0x1002, 0xaa38, AZX_DRIVER_ATIHDMI },
 {"ATI HDMI",     0x1002, 0xaa40, AZX_DRIVER_ATIHDMI },
 {"ATI HDMI",     0x1002, 0xaa48, AZX_DRIVER_ATIHDMI },
 {"ATI R5800",    0x1002, 0xaa50, AZX_DRIVER_ATIHDMI },
 {"ATI R5700",    0x1002, 0xaa58, AZX_DRIVER_ATIHDMI },
 {"ATI R5600",    0x1002, 0xaa60, AZX_DRIVER_ATIHDMI },
 {"ATI R5000",    0x1002, 0xaa68, AZX_DRIVER_ATIHDMI },
 {"ATI R6xxx",    0x1002, 0xaa80, AZX_DRIVER_ATIHDMI },
 {"ATI R6800",    0x1002, 0xaa88, AZX_DRIVER_ATIHDMI },
 {"ATI R6xxx",    0x1002, 0xaa90, AZX_DRIVER_ATIHDMI },
 {"ATI R6400",    0x1002, 0xaa98, AZX_DRIVER_ATIHDMI },
 {"ATI HDNS",     0x1002, 0x9902, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaaa0, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaaa8, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaab0, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaac0, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaac8, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaad8, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaae0, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaae8, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaaf0, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xaaf8, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xab00, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xab08, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xab10, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xab18, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xab20, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xab28, AZX_DRIVER_ATIHDMI_NS },
 {"ATI HDNS",     0x1002, 0xab38, AZX_DRIVER_ATIHDMI_NS },
 {"VIA 82xx",     0x1106, 0x3288, AZX_DRIVER_VIA },
 {"VIA 7122",     0x1106, 0x9170, AZX_DRIVER_GENERIC },
 {"VIA 6122",     0x1106, 0x9140, AZX_DRIVER_GENERIC },
 {"SIS 966",      0x1039, 0x7502, AZX_DRIVER_SIS },
 {"ULI M5461",    0x10b9, 0x5461, AZX_DRIVER_ULI },
 {"Teradici",     0x6549, 0x1200, AZX_DRIVER_TERA },
 {"Teradici",     0x6549, 0x2200, AZX_DRIVER_TERA },
 {"CT HDA",       0x1102, 0x0010, AZX_DRIVER_CTHDA },
 {"CT HDA",       0x1102, 0x0012, AZX_DRIVER_CTHDA },
 {"Creative",     0x1102, 0x0009, AZX_DRIVER_GENERIC },
 {"CMedia",       0x13f6, 0x5011, AZX_DRIVER_CMEDIA },
 {"Vortex86MX",   0x17f3, 0x3010, AZX_DRIVER_GENERIC },
 {"VMwareHD",     0x15ad, 0x1977, AZX_DRIVER_GENERIC },
 {"Zhaoxin",      0x1d17, 0x3288, AZX_DRIVER_ZHAOXIN },

 {"NVidia MCP51", 0x10de, 0x026c, AZX_DRIVER_NVIDIA },
 {"NVidia MCP55", 0x10de, 0x0371, AZX_DRIVER_NVIDIA },
 {"NVidia MCP61", 0x10de, 0x03e4, AZX_DRIVER_NVIDIA },
 {"NVidia MCP61", 0x10de, 0x03f0, AZX_DRIVER_NVIDIA },
 {"NVidia MCP65", 0x10de, 0x044a, AZX_DRIVER_NVIDIA },
 {"NVidia MCP65", 0x10de, 0x044b, AZX_DRIVER_NVIDIA },
 {"NVidia MCP67", 0x10de, 0x055c, AZX_DRIVER_NVIDIA },
 {"NVidia MCP67", 0x10de, 0x055d, AZX_DRIVER_NVIDIA },
 {"NVidia MCP77", 0x10de, 0x0774, AZX_DRIVER_NVIDIA },
 {"NVidia MCP77", 0x10de, 0x0775, AZX_DRIVER_NVIDIA },
 {"NVidia MCP77", 0x10de, 0x0776, AZX_DRIVER_NVIDIA },
 {"NVidia MCP77", 0x10de, 0x0777, AZX_DRIVER_NVIDIA },
 {"NVidia MCP73", 0x10de, 0x07fc, AZX_DRIVER_NVIDIA },
 {"NVidia MCP73", 0x10de, 0x07fd, AZX_DRIVER_NVIDIA },
 {"NVidia MCP79", 0x10de, 0x0ac0, AZX_DRIVER_NVIDIA },
 {"NVidia MCP79", 0x10de, 0x0ac1, AZX_DRIVER_NVIDIA },
 {"NVidia MCP79", 0x10de, 0x0ac2, AZX_DRIVER_NVIDIA },
 {"NVidia MCP79", 0x10de, 0x0ac3, AZX_DRIVER_NVIDIA },
 {"NVidia MCP",   0x10de, 0x0d94, AZX_DRIVER_NVIDIA },
 {"NVidia MCP",   0x10de, 0x0d95, AZX_DRIVER_NVIDIA },
 {"NVidia MCP",   0x10de, 0x0d96, AZX_DRIVER_NVIDIA },
 {"NVidia MCP",   0x10de, 0x0d97, AZX_DRIVER_NVIDIA },
 //{"AMD Generic",     0x1002, 0x0000, AZX_DRIVER_GENERIC }, // TODO: cannot define these
 //{"NVidia Generic",  0x10de, 0x0000, AZX_DRIVER_GENERIC },

 {NULL,0,0,0}
};

#if 0

struct codec_vendor_list_s{
 unsigned short vendor_id;
 char *vendor_name;
};

static struct codec_vendor_list_s codecvendorlist[]={
 {0x1002,"ATI"},
 {0x1013,"Cirrus Logic"},
 {0x1057,"Motorola"},
 {0x1095,"Silicon Image"},
 {0x10de,"Nvidia"},
 {0x10ec,"Realtek"},
 {0x1022,"AMD"},
 {0x1102,"Creative"},
 {0x1106,"VIA"},
 {0x111d,"IDT"},
 {0x11c1,"LSI"},
 {0x11d4,"Analog Devices"},
 {0x1d17,"Zhaoxin"},
 {0x13f6,"C-Media"},
 {0x14f1,"Conexant"},
 {0x17e8,"Chrontel"},
 {0x1854,"LG"},
 {0x1aec,"Wolfson"},
 {0x434d,"C-Media"},
 {0x8086,"Intel"},
 {0x8384,"SigmaTel"},
 {0x0000,"Unknown"}
};

/* search codec vendor */

static char *hda_search_vendorname(unsigned int vendorid)
/////////////////////////////////////////////////////////
{
	struct codec_vendor_list_s *cl = &codecvendorlist[0];
	do{
		if(cl->vendor_id == vendorid)
			break;
		cl++;
	}while(cl->vendor_id);
	return cl->vendor_name;
}
#endif

/* display card status */

static void HDA_card_info( struct audioout_info_s *aui )
////////////////////////////////////////////////////////
{
}

/* called by HDA_adetect() if card isn't selected */

static void HDA_cardclose( struct intelhd_card_s *card )
////////////////////////////////////////////////////////
{
	if( card->iobase ){
		__dpmi_meminfo info;
		hda_hw_close( card );
		/* iobase has to be converted back to a linear address */
		info.address = LinearAddr( (void *)(card->iobase) );
		__dpmi_free_physical_address_mapping( &info );
		card->iobase = 0;
	}
	if( card->afg_nodes ) {
		pds_free( card->afg_nodes );
		card->afg_nodes = NULL;
	}
	if ( card->dm ) {
		MDma_free_cardmem( card->dm );
		card->dm = NULL;
	}
}

static void HDA_close( struct audioout_info_s *aui );

static int HDA_adetect( struct audioout_info_s *aui )
/////////////////////////////////////////////////////
{
	struct intelhd_card_s *card;
	unsigned int i;

	card = (struct intelhd_card_s *)pds_calloc( 1, sizeof(struct intelhd_card_s) );
	if( !card ) {
		dbgprintf(("HDA_adetect: 1. calloc() failed\n" ));
		return 0;
	}
	aui->card_private_data = card;

	card->pci_dev = (struct pci_config_s *)pds_calloc( 1, sizeof(struct pci_config_s) );
	if( !card->pci_dev ) {
		pds_free( card );
		dbgprintf(("HDA_adetect: 2. calloc() failed\n" ));
		return( 0 );
	}

	/* don't search for vendors/deviceIDs. Instead scan for HDAs only, and
	 * use card_select_devicenum as start index for the scan.
	 */
	//if(pcibios_search_devices(intelhda_devices,card->pci_dev)!=PCI_SUCCESSFUL)
	for ( ;; aui->card_select_devicenum++ ) {
		__dpmi_meminfo info;
		unsigned int timeout;
		char *pReason;

		card->pci_dev->device_type = AZX_DRIVER_GENERIC; /* set as default, will be changed if device is known */
		if(pcibios_FindDeviceClass( 4, 3, 0, aui->card_select_devicenum, intelhda_devices, card->pci_dev) != PCI_SUCCESSFUL)
			break;

		card->iobase = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NAMBAR);
		if( card->iobase & 0x1 ) // I/O address? - shouldn't happen with HDA; memory mapping only
			card->iobase = 0;
		if( !card->iobase ) {
			dbgprintf(("HDA_adetect: card index %u skipped (no PCI base addr)\n", aui->card_select_devicenum ));
			continue;
		}
		if( card->iobase & 4 ) {// 64-bit address? then check if it's beyond 4G...
			uint32_t tmp = pcibios_ReadConfig_Dword(card->pci_dev, PCIR_NAMBAR+4);
			if ( tmp ) {
				dbgprintf(("HDA_adetect: card index %u skipped (PCI base addr > 4G)\n", aui->card_select_devicenum ));
				continue;
			}
		}
		card->iobase &= 0xfffffff0;
		info.address = card->iobase;
		info.size = 0x2000;
		if (__dpmi_physical_address_mapping(&info) != 0) {
			printf("HDA: mapping MMIO for card %u failed\n", aui->card_select_devicenum );
			break;
		}
		/* the physical addr, after mapped to a linear addr, is finally converted to a ptr */
		card->iobase = (uint32_t)NearPtr( info.address );
		if( aui->card_select_config >= 0 )
			card->config_select = aui->card_select_config;

		card->board_driver_type = card->pci_dev->device_type;
		if( !hda_buffer_init( aui, card )) {
			printf("HDA: DMA buffer init failed\n", aui->card_select_devicenum );
			break;
		}

		aui->card_DMABUFF = card->pcmout_buffer;
		aui->card_irq = pcibios_ReadConfig_Byte(card->pci_dev, PCIR_INTR_LN);

		dbgprintf(("HDA_adetect, board type: %s (vendor/ID=%X/%X)\n",
			  card->pci_dev->device_name ? card->pci_dev->device_name : "NULL",
			  (long)card->pci_dev->vendor_id,(long)card->pci_dev->device_id));
		hda_hw_init( card );
		dbgprintf(("HDA_adetect: hw init done\n" ));

		/* hda_mixer_init talks to codec; means CORB/RIRB must be ready to use */
		azx_writeb( card, CORBCTL, 2 ); /* start CORB DMA engine */
		azx_writeb( card, RIRBCTL, 2 ); /* start RIRB DMA engine */
		/* wait till engines are up */
		for ( timeout = 0; timeout < 100 && !(azx_readb( card, CORBCTL ) & 2); timeout++, pds_delay_10us(100) );

		for( i = 0; i < AZX_MAX_CODECS; i++ ){
			if( card->codec_mask & ( 1 << i) ) {
				card->codec_index = i;
				if( hda_mixer_init( card ) ) {
					dbgprintf(("HDA_adetect: exit, found mixer for codec %u\n", i ));
					return( 1 );
				}
			}
		}
		pReason = ( card->codec_mask == 0 ) ? "no codecs attached" : "mixer init error";
		printf("HDA: card %u skipped - %s\n", aui->card_select_devicenum, pReason );
		HDA_cardclose( card );
	}
	HDA_close( aui );
	dbgprintf(("HDA_adetect: no acceptable card found\n" ));
	return 0;
}

static void HDA_close( struct audioout_info_s *aui )
////////////////////////////////////////////////////
{
	struct intelhd_card_s *card = aui->card_private_data;
	dbgprintf(("HDA_close\n" ));
	if( card ){
		if( card->iobase ){
			__dpmi_meminfo info;
			hda_hw_close( card );
			/* iobase has to be converted back to a linear address */
			info.address = LinearAddr( (void *)(card->iobase) );
			__dpmi_free_physical_address_mapping( &info );
			card->iobase = 0;
		}
		if( card->afg_nodes ) {
			pds_free( card->afg_nodes );
			card->afg_nodes = NULL;
		}
		if ( card->dm ) {
			MDma_free_cardmem( card->dm );
			card->dm = NULL;
		}
		if( card->pci_dev ) {
			pds_free( card->pci_dev );
			card->pci_dev = NULL;
		}
		pds_free( card );
		aui->card_private_data = NULL;
	}
}

static void HDA_setrate( struct audioout_info_s *aui )
//////////////////////////////////////////////////////
{
	struct intelhd_card_s *card = aui->card_private_data;

	dbgprintf(("HDA_setrate: freq_card=%u\n", aui->freq_card ));
	aui->card_wave_id = WAVEID_PCM_SLE;
	aui->chan_card = (aui->chan_set) ? aui->chan_set : PCM_CHANNELS_DEFAULT;
	if( aui->chan_card > INTHD_MAX_CHANNELS )
		aui->chan_card = INTHD_MAX_CHANNELS;
	if(!card->dacout_num_bits) // first initialization
		card->dacout_num_bits = (aui->bits_set) ? aui->bits_set : 16;

	card->format_val = hda_calc_stream_format( aui, card); /* may modify aui->freq_card */
	card->pcmout_dmasize = MDma_init_pcmoutbuf( aui, card->pcmout_bufsize, AZX_PERIOD_SIZE, 0);
	dbgprintf(("HDA_setrate: freq_card=%u, chan_card=%u, bits_card=%u\n", aui->freq_card, aui->chan_card, aui->bits_card ));

	azx_setup_periods( card );
	azx_setup_stream( card );
}

/* HDA implementation of card_start().
 * start the stream's DMA engine;
 * enable interrupts for this stream
 */

static void HDA_start( struct audioout_info_s *aui )
////////////////////////////////////////////////////
{
	struct intelhd_card_s *card = aui->card_private_data;
	unsigned int timeout;
	/* stream# isn't stored in card, so we have to calculate it */
	const unsigned int stream_index = ( card->sd_addr - (card->iobase + 0x80)) / 0x20;

	dbgprintf(("HDA_start, stream#=%u\n", stream_index ));

	/* enable interrupts from THIS stream */
	azx_writel(card, INTCTL, azx_readl(card, INTCTL) | (1 << stream_index)); // enable SIE

	azx_sd_writeb(card, SD_CTL, azx_sd_readb(card, SD_CTL) | SD_CTL_DMA_START | SD_INT_MASK); // start DMA
	//azx_sd_writeb(card, SD_CTL, azx_sd_readb(card, SD_CTL) | SD_CTL_DMA_START); // start DMA

	timeout = 500;
	while (!(azx_sd_readb(card, SD_CTL) & SD_CTL_DMA_START) && --timeout) // wait for DMA engine ready
		pds_delay_10us(100);
#ifdef _DEBUG
	if (!timeout)
		dbgprintf(("HDA_start: timeout starting stream DMA engine\n"));
#endif
	pds_delay_10us(100);
}

/* HDA implementation of card_stop().
 * stop the stream's DMA engine;
 * disable interrupts for this stream?
 */
static void HDA_stop( struct audioout_info_s *aui )
///////////////////////////////////////////////////
{
	struct intelhd_card_s *card = aui->card_private_data;
	unsigned int timeout;
	const unsigned int stream_index = (card->sd_addr - (card->iobase + 0x80)) / 0x20;

	dbgprintf(("HDA_stop\n" ));

	azx_sd_writeb(card, SD_CTL, azx_sd_readb(card, SD_CTL) & ~(SD_CTL_DMA_START | SD_INT_MASK)); // stop DMA

	timeout = 200;
	while((azx_sd_readb(card, SD_CTL) & SD_CTL_DMA_START) && --timeout) // wait for DMA stop
		pds_delay_10us(100);

	pds_delay_10us(100);

	/* clear pending interrupts, just to be sure */
	azx_sd_writeb( card, SD_STS, SD_INT_MASK);
	azx_writeb( card, INTCTL, azx_readb(card, INTCTL) & ~(1 << stream_index)); // disable SIE
}

/* HDA implementation of cardbuf_getpos() */

static long HDA_getbufpos( struct audioout_info_s *aui )
////////////////////////////////////////////////////////
{
	struct intelhd_card_s *card = aui->card_private_data;
	unsigned long bufpos;

	bufpos = azx_sd_readl(card, SD_LPIB);

	/*
	dbgprintf(("HDA_getbufpos: bufpos1=%u sts=%X ctl=%X cbl=%u ds=%u ps=%u pn=%u\n",
	    bufpos, azx_sd_readb(card, SD_STS), azx_sd_readl(card, SD_CTL), azx_sd_readl(card, SD_CBL), aui->card_dmasize,
	    card->pcmout_period_size, card->pcmout_num_periods ));
	 */

	if( bufpos < aui->card_dmasize )
		aui->card_dma_lastgoodpos = bufpos;

	return aui->card_dma_lastgoodpos;
}

//mixer

/* vsbhda: the card is supposed to handle the volume setting on its own;
 * the "val" argument just gives the percentage (0-100) of the volume to set.
 */

static void HDA_writeMIXER( struct audioout_info_s *aui, unsigned long reg, unsigned long val )
///////////////////////////////////////////////////////////////////////////////////////////////
{
	struct intelhd_card_s *card = aui->card_private_data;
	int maxstep = ( card->dac_node[0]->amp_out_caps >> 8 ) & 0x7F;
	val = maxstep * val / 100;
	hda_set_vol_mute( card, reg, 0, HDA_OUTPUT, 0, val ); /* left channel */
	hda_set_vol_mute( card, reg, 1, HDA_OUTPUT, 0, val ); /* right channel */
}

/* called by AU_setmixer_one();
 * for HDA just return a percentage value.
 */

static unsigned long HDA_readMIXER( struct audioout_info_s *aui, unsigned long reg )
////////////////////////////////////////////////////////////////////////////////////
{
	struct intelhd_card_s *card = aui->card_private_data;
	uint32_t val;
	int maxstep = ( card->dac_node[0]->amp_out_caps >> 8 ) & 0x7F;
	val = hda_get_vol_mute( card, reg, 0, HDA_OUTPUT, 0 );
	if ( maxstep )
		val = val * 100 / maxstep;
	else
		val = 100;
	return val;
}

#if 1 /* vsbhda */

 /* check if the interrupt comes from the sound card's DMA engines.
 * The CORB/RIRB DMA engines are minor, since communication
 * was during init, while when playing there's nothing to communicate.
 */

static int HDA_IRQRoutine( struct audioout_info_s* aui )
////////////////////////////////////////////////////////
{
	struct intelhd_card_s *card = aui->card_private_data;
	int status = azx_sd_readb(card, SD_STS) & SD_INT_MASK;
	int corbsts;
	int rirbsts;

	if(status)
		azx_sd_writeb(card, SD_STS, status); //ack all

	//ack CORB/RIRB status
	corbsts = azx_readb(card, CORBSTS) & 0x1;
	rirbsts = azx_readb(card, RIRBSTS) & RIRB_INT_MASK; /* bits 0 & 2 */
	if(corbsts)
		azx_writeb(card, CORBSTS, corbsts); /* by writing 0 the bits are cleared */
	if(rirbsts)
		azx_writeb(card, RIRBSTS, rirbsts); /* by writing 0 the bits are cleared */
	return status | corbsts | rirbsts;
}
#endif

static const struct aucards_mixerchan_s *hda_mixerset[] = {
	&hda_master_vol,
	NULL
};

const struct sndcard_info_s HDA_sndcard_info = {
 "Intel HDA",
 0,
 NULL,              // card_config
 NULL,              // no init
 &HDA_adetect,      // only autodetect
 &HDA_card_info,
 &HDA_start,
 &HDA_stop,
 &HDA_close,
 &HDA_setrate,

 &MDma_writedata, /* =cardbuf_writedata() */
 &HDA_getbufpos,  /* =cardbuf_getpos() */
 &MDma_clearbuf,  /* =cardbuf_clear() */
 &HDA_IRQRoutine, /* vsbhda */
 &HDA_writeMIXER, /* =card_writemixer() */
 &HDA_readMIXER,  /* =card_readmixer() */
 hda_mixerset     /* =card_mixerchans */
};
