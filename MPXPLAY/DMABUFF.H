#ifndef dmabuff_h
#define dmabuff_h

#ifdef __cplusplus
extern "C" {
#endif

extern int MDma_alloc_cardmem( struct cardmem_s *dm, unsigned int buffsize );
extern void MDma_free_cardmem( struct cardmem_s *dm );
extern unsigned int MDma_get_max_pcmoutbufsize( struct audioout_info_s *aui, unsigned int max_bufsize, unsigned int pagesize, unsigned int samplesize );
//extern unsigned int MDma_init_pcmoutbuf( struct audioout_info_s *aui, unsigned int maxbufsize, unsigned int pagesize, unsigned int freq_config );
extern unsigned int MDma_init_pcmoutbuf( struct audioout_info_s *aui, unsigned int maxbufsize, unsigned int pagesize );

extern void MDma_clearbuf( struct audioout_info_s *aui );
extern void MDma_writedata( struct audioout_info_s *aui, char *src, unsigned long left );
//extern void MDma_interrupt_monitor( struct audioout_info_s *aui );

#ifdef __cplusplus
}
#endif

#endif // dmabuff_h
