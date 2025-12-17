
#ifndef SC_SBLIV_H
#define SC_SBLIV_H

#define EMU_CHIPS_10K1  1 // SB Live 5.1
#define EMU_CHIPS_10K2  2 // SB Audigy
#define EMU_CHIPS_10KX  (EMU_CHIPS_10K1 | EMU_CHIPS_10K2)

#define EMU_CHIPS_0102  4 // Audigy 1,2,4
#define EMU_CHIPS_0106  8 // Live 24, Audigy LS
#define EMU_CHIPS_0108 16 // Audigy2 Value
#define EMU_CHIPS_0151 32 // P16V (Audigy 2,4)

#define EMU_CHIPS_24BIT (EMU_CHIPS_0106 | EMU_CHIPS_0151)

struct emu_card_version_s {
 char *longname;
 uint16_t device;
 uint8_t  revision;
 uint32_t subsystem;     // serial
 uint8_t  chips;         // chips on the card
 uint8_t  max_channels;
};

struct emu10k1_card;

struct emu_driver_func_s{
 unsigned int (*selector)   ( struct emu10k1_card *card, struct audioout_info_s *aui);
 void         (*hw_init)    ( struct emu10k1_card *card, struct audioout_info_s *aui);
 void         (*hw_close)   ( struct emu10k1_card *card);
 unsigned int (*buffer_init)( struct emu10k1_card *card, struct audioout_info_s *aui);
 void         (*setrate)    ( struct emu10k1_card *card, struct audioout_info_s *aui);
 void         (*start_playback)( struct emu10k1_card *card);
 void         (*stop_playback) ( struct emu10k1_card *card);
 unsigned int (*pcm_pointer_playback)( struct emu10k1_card *card, struct audioout_info_s *aui);
 void         (*clear_cache)( struct emu10k1_card *card);
 int          (*interrupt_isr)( struct emu10k1_card *card); /* vsbhda */
 unsigned int (*mixer_read) ( struct emu10k1_card *card, unsigned int reg);
 void         (*mixer_write)( struct emu10k1_card *card, unsigned int reg, unsigned int value);
 const struct aucards_mixerchan_s **mixerset;
};

struct emu10k1_card {
 unsigned long   iobase;
 unsigned int    irq;
 unsigned short  model;
 unsigned char   chiprev;
 unsigned long   serial;
 unsigned char   chips;
 unsigned char   chip_select;
 struct pci_config_s pci_dev;
 const struct emu_card_version_s *card_capabilities;
 const struct emu_driver_func_s *driver_funcs;

 struct cardmem_s dm;     /* size should be 4+4+32+4 */
 uint32_t *virtualpagetable;
 void *silentpage;
 unsigned long period_size;
 char *pcmout_buffer;
 long pcmout_bufsize;
 //unsigned long pcmout_dmasize;

 unsigned int voice_initial_pitch;
 unsigned int voice_pitch_target;
};

#endif // SC_SBLIV_H
