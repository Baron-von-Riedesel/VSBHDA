
tportx( 0x388, &OPL3EMU_OPL3_388, OPL3_IODT )
tport( 0x389, &OPL3EMU_OPL3_389 )
tport( 0x38A, &OPL3EMU_OPL3_38A )
tport( 0x38B, &OPL3EMU_OPL3_38B )
tportx(  0x00, &VDMA_DMA, VDMA_IODT ) /* ch 0 */
tport(  0x01, &VDMA_DMA )
tport(  0x02, &VDMA_DMA )  /* ch 1 */
tport(  0x03, &VDMA_DMA )
//tport(  0x04, &VDMA_DMA )  /* ch 2 */
//tport(  0x05, &VDMA_DMA )
tport(  0x06, &VDMA_DMA )  /* ch 3 */
tport(  0x07, &VDMA_DMA )
tport(  0x08, &VDMA_DMA )
tport(  0x09, &VDMA_DMA )
tport(  0x0A, &VDMA_DMA )
tport(  0x0B, &VDMA_DMA )
tport(  0x0C, &VDMA_DMA )
tport(  0x0D, &VDMA_DMA )
tport(  0x0E, &VDMA_DMA )
tport(  0x0F, &VDMA_DMA )
//tport(  0x81, &VDMA_DMA ) /* ch 2 */
tportx(  0x82, &VDMA_DMA, VDMAPG_IODT ) /* ch 3 */
tport(  0x83, &VDMA_DMA ) /* ch 1 */
tport(  0x87, &VDMA_DMA ) /* ch 0 */
#if SB16
tport(  0x89, &VDMA_DMA ) /* ch 6 */
tport(  0x8A, &VDMA_DMA ) /* ch 7 */
tport(  0x8B, &VDMA_DMA ) /* ch 5 */
//tport(  0x8F, &VDMA_DMA ) /* ch 4 */
//tport(  0xC0, &VDMA_DMA ) /* ch 4 */
//tport(  0xC2, &VDMA_DMA )
tportx(  0xC4, &VDMA_DMA, VHDMA_IODT ) /* ch 5 */
tport(  0xC6, &VDMA_DMA )
tport(  0xC8, &VDMA_DMA ) /* ch 6 */
tport(  0xCA, &VDMA_DMA )
tport(  0xCC, &VDMA_DMA ) /* ch 7 */
tport(  0xCE, &VDMA_DMA )
tport(  0xD0, &VDMA_DMA )
tport(  0xD2, &VDMA_DMA )
tport(  0xD4, &VDMA_DMA )
tport(  0xD6, &VDMA_DMA )
tport(  0xD8, &VDMA_DMA )
tport(  0xDA, &VDMA_DMA )
tport(  0xDC, &VDMA_DMA )
tport(  0xDE, &VDMA_DMA )
#endif
tportx(  0x20, &VIRQ_IRQ, VIRQ_IODT )
//tport(  0x21, &VIRQ_IRQ )
//tport(  0xA0, &VIRQ_IRQ ) /* no need to trap SPIC, since currently just IRQ 5/7 are accepted */
//tport(  0xA1, &VIRQ_IRQ )
tportx( 0x220, &OPL3EMU_OPL3_388, SB_IODT )
tport( 0x221, &OPL3EMU_OPL3_389 )
tport( 0x222, &OPL3EMU_OPL3_38A )
tport( 0x223, &OPL3EMU_OPL3_38B )
tport( 0x224, &SBEMU_SB_MixerAddr )
tport( 0x225, &SBEMU_SB_MixerData )
tport( 0x226, &SBEMU_SB_DSP_Reset )
tport( 0x228, &OPL3EMU_OPL3_388 )
tport( 0x229, &OPL3EMU_OPL3_389 )
tport( 0x22A, &SBEMU_SB_DSP_Read )
tport( 0x22C, &SBEMU_SB_DSP_Write )
tport( 0x22E, &SBEMU_SB_DSP_ReadStatus )
tport( 0x22F, &SBEMU_SB_DSP_ReadINT16BitACK )
