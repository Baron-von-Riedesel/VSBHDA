
;--- simple __brk()
;--- assemble with -zcw
;--- DPMI memory allocation must be done while program is current client!

	.386
	.model ?MODEL
	option casemap:none

ENOMEM equ 5

MINCHUNK equ 8000h

	.data

externdef errno:dword
externdef _curbrk:dword
externdef _dynend:dword
if @Model ne 7
externdef _DSBase:dword
endif

	include debug.inc

	.code

;--- updates _curbrk, _dynend

__brk proc c uses esi edi ebx brk_value:dword

local seg_size:dword

	mov eax, brk_value
	add eax, 16-1
	and al, 0f0h
	cmp eax, MINCHUNK
	jae @F
	mov eax, MINCHUNK
@@:
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
if @Model ne 7
	sub eax, _DSBase
endif
	mov [_dynend], eax
	add eax, seg_size
	xchg eax, _curbrk	; return old value of _curbrk
	@dprintf CStr("%X, brk(%X): new/old _curbrk %X/%X, _dynend=%X",10), cs, brk_value, _curbrk, eax, _dynend
	ret
error:
	@dprintf CStr("%X, brk(%X): int 31h, ax=501 failed, bx:cx=%X",10), cs, brk_value, bx::cx
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

