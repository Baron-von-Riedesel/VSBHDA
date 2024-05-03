#ifndef _DMA_H_
#define _DMA_H_

//words
#define DMA_REG_CH0_ADDR       0x00
#define DMA_REG_CH0_COUNTER    0x01
#define DMA_REG_CH1_ADDR       0x02
#define DMA_REG_CH1_COUNTER    0x03
#define DMA_REG_CH2_ADDR       0x04
#define DMA_REG_CH2_COUNTER    0x05
#define DMA_REG_CH3_ADDR       0x06
#define DMA_REG_CH3_COUNTER    0x07
//bytes
#define DMA_REG_STATUS_CMD     0x08 //status (R) / command (W)
#define DMA_REG_REQUEST        0x09
#define DMA_REG_SINGLEMASK     0x0A
#define DMA_REG_MODE           0x0B
#define DMA_REG_FLIPFLOP       0x0C
#define DMA_REG_IMM_RESET      0x0D //immediate (R) / master reset (W, masks all 4 channels))
#define DMA_REG_MASK_RESET     0x0E //a write unmasks all 4 channels
#define DMA_REG_MULTIMASK      0x0F

/* bits in DMA_REG_MODE */
#define DMA_REG_MODE_OPERATION 0x0C
#define DMA_REG_MODE_OP_VERIFY 0x00
#define DMA_REG_MODE_OP_WRITE  0x04
#define DMA_REG_MODE_OP_READ   0x08
#define DMA_REG_MODE_AUTO      0x10

#define DMA_REG_CH0_PAGEADDR   0x87
#define DMA_REG_CH1_PAGEADDR   0x83
#define DMA_REG_CH2_PAGEADDR   0x81
#define DMA_REG_CH3_PAGEADDR   0x82

//16bit
#define DMA_REG_CH4_ADDR       0xC0
#define DMA_REG_CH4_COUNTER    0xC2
#define DMA_REG_CH7_ADDR       0xCC
#define DMA_REG_CH7_COUNTER    0xCE
#define DMA_REG_STATUS_CMD16   0xD0
#define DMA_REG_MULTIMASK16    0xDE
#define DMA_REG_CH4_PAGEADDR   0x8F
#define DMA_REG_CH7_PAGEADDR   0x8A

#endif//_DMA_H_
