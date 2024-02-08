
;--- 16-bit helper code for PTRAP.C, real-mode part.
;--- if IRQs > 8 are to be supported, this code needs adjustments.
;--- must be assembled with JWasm v2.18+

	.286
	.model small
	option proc:private
	option casemap:none

	.errnz __JWASM__ lt 218,<JWasm v2.18+ required>

HANDLE_IN_388H_DIRECTLY equ 1 ; std=1, must match #define in ptrap.c
RMPICTRAPDYN            equ 0 ; std=0, must match #define in ptrap.c

;--- if RMPICTRAPDYN is 0, the PIC port 0020h is permanently trapped.
;--- if RMPICTRAPDYN is 1, the PIC port 0020h is trapped only while an IRQ 5/7
;--- is emulated. This is quite some overhead, currently unsure if it works stable.

	.code

	assume ds:nothing, ss:nothing

;--- 12 bytes data - if this is changed, PTRAP.C must be adjusted!
_0000 dd 0	; rmcb to vsbhda port trap handler
_0004 db 0	; last 388h value written
_0005 db 0
      dw 0	; not used
_0008 dd 0	; QPI entry Jemm/Qemm, for "untrapped io"

;--- port trap proc, called by QPIEmu/Qemm;
;--- this code runs in real-mode.

PTRAP_RM_Wrapper proc c public

ife RMPICTRAPDYN
	cmp dx, word ptr 0020h	; must be the WORD variant, since it's patched - see ptrap.c
	je is20
endif

if HANDLE_IN_388H_DIRECTLY
	cmp dx, 388h
	je is388
	cmp dx, 389h
	je is389
normal:
	jmp [_0000]
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

;--- this code runs when NO IRQ 5/7 is emulated.
;--- since port 0020h is permanently trapped, QPI must be called
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
	call [_0008]		; call QPIEmu/Qemm
	xchg ax, bx
	pop bx
	retf
endif

PTRAP_RM_Wrapper endp

PTRAP_RM_WrapperEnd proc c public
PTRAP_RM_WrapperEnd endp

	end
