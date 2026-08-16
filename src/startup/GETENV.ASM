
;--- replace OW's getenv() runtime - it's quite large.

	.386
	.MODEL ?MODEL
	option casemap:none

	.CODE

getenv proc c public uses ebx esi edi pVar:ptr

	mov ah, 51h
	int 21h
	push ds
	mov ds, ebx
	mov bx, ds:[2Ch]
	pop ds
	mov ax,6
	int 31h
	push cx
	push dx
	pop edi

if @Model ne 7
	mov ebx, ds
	mov ax, 6
	int 31h
	push cx
	push dx
	pop eax
	sub edi, eax
endif

	mov esi, pVar
	mov ebx, esi
	.while byte ptr [ebx]
		inc ebx
	.endw
	sub ebx, esi
nextvar:
	mov esi,pVar
	mov ecx, ebx
	repz cmpsb
	jz found
	mov al, 0
	or ecx,-1
	lea edi,[edi-1]
	repnz scasb
	cmp byte ptr [edi],0
	jnz nextvar
	xor eax, eax
	ret
found:
	cmp byte ptr [edi],'='
	jnz nextvar
	lea eax, [edi+1]
	ret
getenv endp

	END
