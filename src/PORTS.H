
tportx( 0x388, &VOPL3_388, OPL3_IODT )
tport( 0x389, &VOPL3_389 )
tport( 0x38A, &VOPL3_38A )
tport( 0x38B, &VOPL3_38B )
tportx(  0x20, &VIRQ_IRQ, IRQ_IODT )
//tport(  0x21, &VIRQ_IRQ )
//tportx(  0xA0, &VIRQ_IRQ, IRQH_IODT ) /* no need to trap SPIC, since currently just IRQ 5/7 are accepted */
//tport(  0xA1, &VIRQ_IRQ )
tportx(  0x02, &VDMA_DMA, DMA_IODT ) /* ch 1; will be modified if LDMA != 1 */
tport(  0x03, &VDMA_DMA )            /* ch 1; will be modified if LDMA != 1 */
tport(  0x08, &VDMA_DMA )
tport(  0x09, &VDMA_DMA )
tport(  0x0A, &VDMA_DMA )
tport(  0x0B, &VDMA_DMA )
tport(  0x0C, &VDMA_DMA )
tport(  0x0D, &VDMA_DMA )
tport(  0x0E, &VDMA_DMA )
tport(  0x0F, &VDMA_DMA )
tportx( 0x83, &VDMA_DMA, DMAPG_IODT ) /* ch 1; will be modified if LDMA != 1 */
#if SB16
tport(  0x8B, &VDMA_DMA )             /* ch 5; will be modified if HDMA != 5 */
#endif
#if SB16
tportx( 0xC4, &VDMA_DMA, HDMA_IODT ) /* ch 5; will be modified if HDMA != 5 */
tport(  0xC6, &VDMA_DMA )            /* ch 5; will be modified if HDMA != 5 */
tport(  0xD0, &VDMA_DMA )
tport(  0xD2, &VDMA_DMA )
tport(  0xD4, &VDMA_DMA )
tport(  0xD6, &VDMA_DMA )
tport(  0xD8, &VDMA_DMA )
tport(  0xDA, &VDMA_DMA )
tport(  0xDC, &VDMA_DMA )
tport(  0xDE, &VDMA_DMA )
#endif
tportx( 0x220, &VOPL3_388, SB_IODT )
tport(  0x221, &VOPL3_389 )
tport(  0x222, &VOPL3_38A )
tport(  0x223, &VOPL3_38B )
tport(  0x224, &VSB_MixerAddr )
tport(  0x225, &VSB_MixerData )
tport(  0x226, &VSB_DSP_Reset )
tport(  0x228, &VOPL3_388 )
tport(  0x229, &VOPL3_389 )
tport(  0x22A, &VSB_DSP_Read )
tport(  0x22C, &VSB_DSP_Write )
tport(  0x22E, &VSB_DSP_ReadStatus )
tport(  0x22F, &VSB_DSP_ReadINT16BitACK )
