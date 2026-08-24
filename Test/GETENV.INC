
getenv proc c uses ds bx si di pVar:ptr, pBuffer:ptr

	mov ah, 51h
	int 21h
	mov es, bx
	mov es, es:[2Ch]
	xor di, di

	mov si, pVar
	mov bx, si
	.while byte ptr [bx]
		inc bx
	.endw
	sub bx, si
nextvar:
	mov si, pVar
	mov cx, bx
	repz cmpsb
	jz found
	mov al, 0
	or cx,-1
	lea di,[di-1]
	repnz scasb
	cmp byte ptr es:[di],0
	jnz nextvar
	xor ax, ax
	ret
found:
	cmp byte ptr es:[di],'='
	jnz nextvar
	lea si, [di+1]
	mov di, pBuffer
@@:
	lodsb es:[si]
	mov [di], al
	inc di
	and al,al
	jnz @B
	sub di, pBuffer
	mov ax, di
	ret
getenv endp

