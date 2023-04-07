#ifndef dmairq_h
#define dmairq_h

#include "MPXPLAY.H"

#ifdef __cplusplus
extern "C" {
#endif

#define AUCARDS_DMABUFSIZE_NORMAL 32768
#define AUCARDS_DMABUFSIZE_MAX    131072
#define AUCARDS_DMABUFSIZE_BLOCK  4096   // default page (block) size

#define DMAMODE_AUTOINIT_OFF 0
#define DMAMODE_AUTOINIT_ON  0x10

typedef struct au_isadma_s{
 unsigned int dma_low;
 unsigned int dma_high;
 cardmem_t *card_dma_dosmem;
}au_isadma_s;

extern cardmem_t *MDma_alloc_cardmem(unsigned int buffsize);
extern void MDma_free_cardmem(cardmem_t *dm);
extern unsigned int MDma_get_max_pcmoutbufsize(struct mpxplay_audioout_info_s *aui,unsigned int max_bufsize,unsigned int pagesize,unsigned int samplesize,unsigned long freq_config);
extern unsigned int MDma_init_pcmoutbuf(struct mpxplay_audioout_info_s *aui,unsigned int maxbufsize,unsigned int pagesize,unsigned long freq_config);

extern void MDma_clearbuf(struct mpxplay_audioout_info_s *aui);
extern void MDma_writedata(struct mpxplay_audioout_info_s *aui,char *src,unsigned long left);
extern void MDma_interrupt_monitor(struct mpxplay_audioout_info_s *aui);

//extern unsigned int MIrq_Start(unsigned int irq_num,void (*irq_routine)(struct mpxplay_audioout_info_s *),unsigned long *card_infobits);
//extern void MIrq_Stop(unsigned int irq_num,unsigned long *card_infobits);
//extern unsigned int MIrq_autodetect(struct mpxplay_audioout_info_s *aui);

#ifdef __cplusplus
}
#endif

#endif // dmairq_h
