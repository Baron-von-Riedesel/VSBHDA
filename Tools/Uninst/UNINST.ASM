
;--- uninstall VSBHDA

	.286
	.MODEL tiny
	.dosseg
	.stack 400h
	option casemap:none

print macro text
local sym
	.const
sym db text
	.code
	mov dx,offset sym
	mov ah,9
	int 21h
endm

	.data

wBase	dw -1
wIrq	dw -1
wDmaL	dw -1
wDmaH	dw -1
wType	dw -1

	.const

szBlaster db "BLASTER",0
        
	.CODE

scanvar proc stdcall pBlaster:ptr
	mov si, pBlaster
	mov bx, -1
	.while (byte ptr [si])
		lodsb
		.if (al == ' ')
			mov bx,-1
			.continue
		.endif
		.if ((al >= 'a') && (al <= 'z'))
			sub al,20h
		.endif
		.if (bx == -1)
			mov cl,16
			.if (al == 'A')
				mov bx, offset wBase
			.elseif (al == 'I')
				mov bx, offset wIrq
				mov cl,10
			.elseif (al == 'D')
				mov bx, offset wDmaL
			.elseif (al == 'H')
				mov bx, offset wDmaH
			.elseif (al == 'T')
				mov bx, offset wType
			.endif
			.continue
		.endif
		.if (cl == 10)
			call chkdec
		.else
			call chkhex
		.endif
		jc numdone
		mov ah,0
		.if (bx != -1)
			mov dx, [bx]
			.if (dx == -1)
				inc dx
			.endif
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

;--- es=psp

GetEnvironmentVariableA proc stdcall uses si di pVar:ptr, pOut:ptr, wSize:word
	mov es, es:[2Ch]
	xor di, di
	mov si, pVar
	mov bx, si
	.while byte ptr [bx]
		inc bx
	.endw
	sub bx, si
nextvar:
	mov si,pVar
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
	inc di
	mov si, di
	mov di, pOut
	mov cx, wSize
	mov bx, ds
	push es
	pop ds
	mov es, bx
@@:
	lodsb
	stosb
	cmp al,0
	loopnz @B
	mov ds, bx
	mov ax, di
	sub ax, pOut
	ret
GetEnvironmentVariableA endp

main proc c

local szVar[128]:byte

	invoke GetEnvironmentVariableA, offset szBlaster, addr szVar, sizeof szVar
	.if !ax
		print <"no BLASTER environment variable found",13,10,'$'>
		jmp exit
	.endif
	invoke scanvar, addr szVar
	.if (wBase == -1)
		print <"error: address base is invalid",13,10,'$'>
		jmp exit
	.endif

	mov dx, wBase
	add dx, 6
	mov al, 1
	out dx, al		; start DSP reset
	mov al, 0
	out dx, al		; reset - just to check if someone does respond

	mov dx, wBase
	add dx, 0Eh
	in al,dx
	test al,80h
	jz novsbhda		; vsbhda does respond instantly
	mov dx, wBase
	add dx, 0Ah
	in al,dx
	cmp al,0AAh
	jnz novsbhda

	mov dx, wBase
	add dx, 6
	mov al, 1
	out dx, al		; start DSP reset
	mov al,55h		; uninstall vsbhda
	out dx,al
exit:
	ret
novsbhda:
	print <"VSBHDA not loaded",13,10,'$'>
	ret
main endp

start:
	mov ax,cs
	mov ds,ax
	mov bx,ss
	sub bx,ax
	shl bx,4
	mov ss,ax
	add sp,bx
	call main
	mov ax,4c00h
	int 21h

	END start
