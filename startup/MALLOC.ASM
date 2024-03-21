
;--- 16-bit only: very simple memory allocator.
;--- implements a stack.
;--- currently the OW memory allocator cannot be used.
;--- assemble with -zcw.

	.386
	.model small

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

	.data

externdef _curbrk:dword
externdef _dynend:dword

	.code

memset proto c p:ptr, value:dword, size_:dword
__brk proto c :dword

malloc proc c public amnt:dword

ifdef _DEBUG
	mov eax, _curbrk
	sub eax, _dynend
	invoke __dprintf, CStr("%X: malloc(%X) free=%X (dynend/curbrk=%X/%X",10), cs, amnt, eax, _dynend, _curbrk
endif
tryagain:
	mov ecx, amnt
	add ecx, 4+4-1
	and cl, 0fch
	mov eax, _curbrk
	sub eax, _dynend
	jc outofmem
	cmp eax, ecx
	jc outofmem
	mov eax, _dynend
	mov [eax], ecx
	lea eax, [eax+4]
	add _dynend, ecx
	ret
outofmem:
	invoke __brk, ecx	; __brk() will update _curbrk AND _dynend -
	and eax, eax		; meaning that a new "stack" has begun.
	jnz tryagain
ifdef _DEBUG
	invoke __dprintf, CStr("%X: malloc(%X) failed",10), cs, amnt
endif
	ret
malloc endp

free proc c public pMem:ptr
	mov ecx, pMem
	cmp ecx, 0
	jz @F
	sub ecx, 4
	mov eax, [ecx]
	add eax, ecx
	cmp eax, _dynend
	jz reset
@@:
ifdef _DEBUG
	invoke __dprintf, CStr("%X: free(%X) failed",10), cs, ecx
endif
	ret
reset:
	mov _dynend, ecx
ifdef _DEBUG
	invoke __dprintf, CStr("%X: free(%X) ok",10), cs, ecx
endif
	ret
free endp

calloc proc c public items:dword, size_:dword
	mov eax, size_
	mul items
	push eax
	invoke malloc, eax
	pop edx
	.if eax
		push eax
		invoke memset, eax, 0, edx
		pop eax
	.endif
	ret
calloc endp

	end
