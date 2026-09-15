
;--- scan BLASTER environment variable for a value

scanvar proc stdcall uses si var:byte, pBuffer:ptr, bBase:byte, pValue:ptr
	mov si, pBuffer
	mov bx, -1
	.while byte ptr [si]
		lodsb
		.if (al == ' ')
			mov bx,-1
			.continue
		.endif
		.if (al >= 'a') && (al <= 'z')
			sub al,20h
		.endif
		.if (bx == -1)
			.if (al == var)
				mov bx, pValue
				mov word ptr [bx], 0
			.endif
			.continue
		.endif
		mov cl, bBase
		.if (cl == 10)
			call chkdec
		.else
			call chkhex
		.endif
		jc numdone
		mov ah, 0
		.if (bx != -1)
			mov dx, [bx]
			.if (cl == 16)
				shl dx, 4
			.else
				push ax
				mov ax,dx
				mov ch,0
				mul cx
				pop dx
			.endif
			add dx, ax
			mov [bx],dx
		.endif
numdone:
	.endw
	mov ax,-1
	sub ax,bx
	ret
chkdec:
	sub al,'0'
	jc err1
	cmp al,9
	ja err1
	clc
	retn
err1:
	stc
	retn
chkhex:
	cmp al,'A'
	jb chkdec
	cmp al,'F'
	ja err1
	sub al,37h
	retn
scanvar endp

