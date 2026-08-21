#ifndef dmabuff_h
#define dmabuff_h

#ifdef __cplusplus
extern "C" {
#endif

extern int          MDma_alloc_cardmem( struct cardmem_s *dm, unsigned int bufsize );
extern void         MDma_free_cardmem( struct cardmem_s *dm );
extern unsigned int MDma_get_bufsize( struct audioout_info_s *aui, unsigned int max_bufsize, unsigned int pagesize );
extern unsigned int MDma_initbuf( struct audioout_info_s *aui, unsigned int bufsize );
extern void         MDma_clearbuf( struct audioout_info_s *aui );
extern void         MDma_writedata( struct audioout_info_s *aui, char *src, unsigned int left );

#ifdef __cplusplus
}
#endif

#endif // dmabuff_h
