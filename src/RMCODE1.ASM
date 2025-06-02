
;--- 16-bit helper code for PTRAP.C, real-mode part.
;--- if IRQs > 8 are to be supported, this code needs adjustments.

	.286
	option proc:private
	option casemap:none

HANDLE_IN_388H_DIRECTLY equ 0 ; std=0, must match #define in ptrap.c
RMPICTRAPDYN            equ 0 ; std=0, must match #define in ptrap.c

;--- if RMPICTRAPDYN is 0, the PIC port 0020h is permanently trapped.
;--- if RMPICTRAPDYN is 1, the PIC port 0020h is trapped only while an IRQ 5/7
;--- is emulated. This is quite some overhead.

;--- this code runs in real-mode.

_TEXT segment word public 'CODE'

	assume ds:nothing, ss:nothing

;--- 12 bytes data - if this is changed, PTRAP.C must be adjusted!
wRmcb dd 0	; rmcb to vsbhda port trap handler
_0004 db 0	; last 388h value written
_0005 db 0
wPICp dw 20h; PIC port to trap ( 0020h or 0ffffh ) - must be at offset 6!
dwQPI dd 0	; QPI entry Jemm/Qemm, for "untrapped io"

ife RMPICTRAPDYN
	cmp dx, [wPICp]
	je is20
endif

if HANDLE_IN_388H_DIRECTLY
	cmp dx, 388h
	je is388
	cmp dx, 389h
	je is389
normal:
endif

	jmp [wRmcb]

if HANDLE_IN_388H_DIRECTLY
is388:
	test cl, 4			; OUT instr?
	jnz is388out
	mov al, [_0005]		; last timer value written to 389h
	test al, 1
	jz @F
	mov al, 0C0h
	retf
@@:
	test al, 2
	jz @F
	mov al, 0A0h
	retf
@@:
	xor al,al
	retf
is388out:
	mov [_0004], al
	jmp normal
is389:
	test cl, 4			; OUT instr?
	jz normal
	cmp [_0004], 4		; timer reg?
	jne normal
	mov [_0005], al
	jmp normal
endif

ife RMPICTRAPDYN

;--- This code runs when NO SB IRQ (2/5/7) is emulated.
;--- Since port 0020h is permanently trapped, QPI must be called
;--- to execute the IN/OUT instruction.
;--- 02/2024: fixed: register AH/AX preserved for IN/OUT

is20:
	push bx
	xchg ax, bx
	test cl, 4			; OUT instr?
	.386
	setnz al			; ax=1a00h is "sim IN", 1a01h = "sim OUT"
	.286
	mov ah, 1ah
	call [dwQPI]		; call QPIEmu/Qemm
	xchg ax, bx
	pop bx
	retf
endif

_TEXT ends

	end
