
;--- 16-bit helper code for PTRAP.C, real-mode part.
;--- if IRQs > 8 are to be supported, this code needs adjustments.
;--- must be assembled with JWasm v2.18+

	.286
	.model small
	option proc:private
	option casemap:none

	.errnz __JWASM__ lt 218,<JWasm v2.18+ required>

HANDLE_IN_388H_DIRECTLY equ 1 ; must match #define in ptrap.c
RMPICTRAPDYN            equ 0 ; must match #define in ptrap.c

	.code

	assume ds:nothing, ss:nothing

_0000 dd 0	; rmcb to sbemu port trap handler
_0004 db 0 
_0005 db 0
      dw 0
_0008 dd 0	; entry Jemm/Qemm "untrapped io"

;--- port trap proc, called by Jemm/Qemm

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
	mov al, [_0005]		; IN 388h
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
is20:
	push bx
	test cl, 4			; OUT instr?
	jnz is20out
	mov ax, 1a00h
	call [_0008]  
	mov al, bl
	pop bx
	retf
is20out:
	mov bl, al
	mov ax, 1a01h
	call [_0008]
	pop bx
	retf
endif

PTRAP_RM_Wrapper endp

PTRAP_RM_WrapperEnd proc c public
PTRAP_RM_WrapperEnd endp

	end
