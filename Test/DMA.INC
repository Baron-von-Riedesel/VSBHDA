
LDMABASE    equ 0
HDMABASE    equ 0C0h

;--- 8 bit DMA regs
DMAMASKREG  equ 0Ah
DMAMODEREG  equ 0Bh
DMAFLIPFLOP equ 0Ch

;--- DMA mode register bits
;--- 0-1: channel (0-3)
;--- 2-3: 00 verify, 01 write, 02 read, 11 invalid
DMA_MODE_VERIFY equ 0000b
DMA_MODE_WRITE  equ 0100b
DMA_MODE_READ   equ 1000b
;--- 4: 0 single-cycle, 1 auto-init
DMA_MODE_SINGLECYCLE equ 00000b
DMA_MODE_AUTOINIT    equ 10000b
;--- 5: 0 address increment, 1 address decrement
DMA_MODE_INCREMENT   equ 000000b
DMA_MODE_DECREMENT   equ 100000b
;--- 6-7: 00 demand mode, 01 single mode, 10 block mode, 11 cascade mode
DMA_MODE_DEMAND      equ 00000000b
DMA_MODE_SINGLE      equ 01000000b
DMA_MODE_BLOCK       equ 10000000b
DMA_MODE_CASCADE     equ 11000000b

DMA_MASK_DISABLE_CHN equ 100b
DMA_MASK_ENABLE_CHN  equ 000b

