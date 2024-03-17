
;--- __brk for 16-bit
;--- assemble with -zcw

	.386
	.model small
	option casemap:none

ENOMEM equ 5

	.data

externdef errno:dword
externdef _curbrk:dword
externdef _dynend:dword
externdef _DSBase:dword

	.code

ifdef _DEBUG
CStr macro text:vararg
local sym
	.const
sym db text,0
	.code
	exitm <offset sym>
endm
__dprintf proto c :ptr, :vararg
endif

;--- expand DGROUP
;--- returns -1 on error
;--- updates _curbrk

__brk proc c uses esi edi ebx brk_value:dword

local seg_size:dword

ifdef _DEBUG
	invoke __dprintf, CStr("brk: brk_value=%X, curbrk=%X",10), brk_value, _curbrk
endif
	mov eax, brk_value
	add eax, 16-1
	and al, 0f0h
	mov seg_size, eax

;--- DPMI realloc block function cannot be used, since
;--- the base must not change.

	push eax
	pop cx
	pop bx
	mov ax,501h
	int 31h
	jc error
	push bx
	push cx
	pop eax
	sub eax, _DSBase
	mov [_dynend], eax
	add eax, seg_size
	xchg eax, _curbrk	; return old value of _curbrk
	ret
error:
	mov errno, ENOMEM
	xor eax, eax
	ret
__brk endp

sbrk proc c increment:dword
	mov eax, _curbrk
	add eax, increment
	invoke __brk, eax
	ret
sbrk endp


end

