
;--- 16-bit helper code for QEMM.C

	.286
	.model small
	option proc:private
	option casemap:none

HANDLE_IN_388H_DIRECTLY equ 1 ; must match #define in qemm.c
RMPICTRAPDYN            equ 0 ; must match #define in qemm.c

	.code

_0000 dd 0	; rmcb to sbemu port trap handler
_0004 db 0 
_0005 db 0
      dw 0
_0008 dd 0	; entry Jemm "untrapped io"

QEMM_RM_Wrapper proc c public

ife RMPICTRAPDYN
	cmp dx, word ptr 0020h	; must be the WORD variant, since it's patched - see qemm.c
	je is20
endif

if HANDLE_IN_388H_DIRECTLY
	cmp dx, 388h
	je is388
	cmp dx, 389h
	je is389
normal:
	jmp cs:[_0000]
is388:
	test cl, 4			; OUT instr?
	jnz is388out
	mov al, cs:[_0005]	; in 388h
	and al, 3
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
	mov cs:[_0004], al
	jmp normal
is389:
	test cl, 4			; OUT instr?
	jz normal
	cmp cs:[_0004], 4	; timer reg?
	jne normal
	mov cs:[_0005], al
	jmp normal
endif

ife RMPICTRAPDYN
is20:
	push bx
	test cl, 4			; OUT instr?
	jnz is20out
	mov ax, 1a00h
	call cs:[_0008]  
	mov al, bl
	pop bx
	retf
is20out:
	mov bl, al
	mov ax, 1a01h
	call cs:[_0008]
	pop bx
	retf
endif

QEMM_RM_Wrapper endp

QEMM_RM_WrapperEnd proc c public
QEMM_RM_WrapperEnd endp

	end

