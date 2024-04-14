
/* the trapped ports are arranged in ranges, starting with tportx();
 */

tportx( 0x388, &VOPL3_388, OPL3 )
tport( 0x389, &VOPL3_389 )
tport( 0x38A, &VOPL3_38A )
tport( 0x38B, &VOPL3_38B )
tportx(  0x20, &VPIC_Acc, PIC )
//tport(  0x21, &VPIC_Acc )
//tportx(  0xA0, &VPIC_Acc, PICH )   /* no need to trap SPIC, since currently just IRQ 5/7 are accepted */
//tport(  0xA1, &VPIC_Acc )
tportx(  0x02, &VDMA_Acc, DMA )      /* ch 1; will be modified if LDMA != 1 */
tport(  0x03, &VDMA_Acc )            /* ch 1; will be modified if LDMA != 1 */
tport(  0x08, &VDMA_Acc )
tport(  0x09, &VDMA_Acc )
tport(  0x0A, &VDMA_Acc )
tport(  0x0B, &VDMA_Acc )
tport(  0x0C, &VDMA_Acc )
tport(  0x0D, &VDMA_Acc )
tport(  0x0E, &VDMA_Acc )
tport(  0x0F, &VDMA_Acc )
tportx( 0x83, &VDMA_Acc, DMAPG )     /* ch 1; will be modified if LDMA != 1 */
#if SB16
tport(  0x8B, &VDMA_Acc )            /* ch 5; will be modified if HDMA != 5 */
#endif
#if SB16
tportx( 0xC4, &VDMA_Acc, HDMA )      /* ch 5; will be modified if HDMA != 5 */
tport(  0xC6, &VDMA_Acc )            /* ch 5; will be modified if HDMA != 5 */
tport(  0xD0, &VDMA_Acc )
tport(  0xD2, &VDMA_Acc )
tport(  0xD4, &VDMA_Acc )
tport(  0xD6, &VDMA_Acc )
tport(  0xD8, &VDMA_Acc )
tport(  0xDA, &VDMA_Acc )
tport(  0xDC, &VDMA_Acc )
tport(  0xDE, &VDMA_Acc )
#endif
tportx( 0x220, &VOPL3_388, SB )
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
#if VMPU
tportx( 0x330, &VMPU_MPU, MPU )
tport(  0x331, &VMPU_MPU )
#endif
