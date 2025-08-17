
;--- very simple memory allocator.
;--- implements a stack.
;--- in 16-bit, the OW memory allocator cannot be used currently.
;--- but it's now also used for 32-bit, because it's so simple
;--- and sufficient for this kind of app.
;--- assemble with -zcw.

	.386
	.model ?MODEL

	include debug.inc

	.data

externdef _curbrk:dword
externdef _dynend:dword

	.code

memset proto c p:ptr, value:dword, size_:dword
__brk proto c :dword

ifdef OW19 ;if Open Watcom v1.9 is to be used; avoids linker error.
	public _nmalloc
_nmalloc:
endif
malloc proc c public amnt:dword

if 0;def _DEBUG
	mov eax, _curbrk
	sub eax, _dynend
	@dprintf CStr("%X: malloc(%X) free=%X (dynend/curbrk=%X/%X",10), cs, amnt, eax, _dynend, _curbrk
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
	@dprintf CStr("%X: malloc(%X) failed",10), cs, amnt
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
	@dprintf CStr("%X: free(%X) failed",10), cs, ecx
	ret
reset:
	mov _dynend, ecx
;	@dprintf CStr("%X: free(%X) ok",10), cs, ecx
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
