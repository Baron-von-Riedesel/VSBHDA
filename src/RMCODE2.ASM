
;--- 16-bit helper code for VIRQ.C, real-mode part.

	.286

IRQ0F equ 1 ; 1=IRQ 7 that emits EOI to PIC

if IRQ0F

_TEXT segment word public 'CODE'

;--- IRQ7 in v86-mode

	push ax
	mov al, 0Bh
	out 20h, al
	in al, 20h
	test al, 80h
	jz @F
	mov al, 20h
	out 20h, al
@@:
	pop ax
	iret

_TEXT ends

endif

	end
