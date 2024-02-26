
;--- Int 31h/21h hook; purposes:
;--- 1. to hide the sound hw IRQ from client
;--- 2. clear ISR_DMA_MappedAddr at DOS exit, since it will be released by hdpmi

	.386
	.MODEL small, c
	option casemap:none
	option proc:private

	.data

externdef dfOldISR:fword
externdef c ISR_DMA_MappedAddr:dword

oldint31 df ?
	dw ?
oldint21 df ?
	dw ?
currsndint df ?	; current vector for sound ISR
	dw ?
dsseg dw ?
intno db ?

	.code

	assume ds:_DATA

myint31 proc

	cmp ax, 204h
	jz is204
	cmp ax, 205h
	jz is205
prevint:
	jmp cs:[oldint31]
is204:
is205:
	cmp bl, cs:[intno]
	jnz prevint
	cmp al, 5
	jz is05
	mov edx, dword ptr cs:[currsndint+0]
	mov cx, word ptr cs:[currsndint+4]
	and byte ptr [esp+2*4], 0feh
	iretd
is05:
	push eax
	push ds
	mov ds, cs:[dsseg]
	mov dword ptr [currsndint+0], edx
	mov word ptr [currsndint+4], cx
	pop ds
	pop eax
	and byte ptr [esp+2*4], 0feh
	iretd
	align 4
myint31 endp

myint21 proc

	cmp ah, 25h
	jz is25
	cmp ah, 35h
	jz is35
	cmp ah, 4Ch
	jz is4C
prevint:
	jmp cs:[oldint21]
is4C:
	push ds
	mov ds, cs:[dsseg]
	mov [ISR_DMA_MappedAddr], 0
	pop ds
	jmp prevint
is25:
is35:
	cmp al, cs:[intno]
	jnz prevint
	cmp ah, 25h
	jz isset
	mov ebx, dword ptr cs:[currsndint+0]
	mov es, word ptr cs:[currsndint+4]
	and byte ptr [esp+2*4], 0feh
	iretd
isset:
	push eax
	push ds
	mov ds, cs:[dsseg]
	mov dword ptr [currsndint+0], edx
	mov word ptr [currsndint+4], cx
	pop ds
	pop eax
	and byte ptr [esp+2*4], 0feh
	iretd
	align 4
myint21 endp

;--- install int 31h handler

_hdpmi_InstallInt31 proc public uses ebx interrupt:byte

;--- hook into DPMI int 31h
	mov bl, 31h
	mov ax, 204h
	int 31h
	jc error
	mov dword ptr oldint31+0, edx
	mov word ptr oldint31+4, cx
	mov ecx, cs
	mov edx, offset myint31
	mov ax, 205h
	int 31h
	jc error

	mov bl, 21h
	mov ax, 204h
	int 31h
	jc error
	mov dword ptr oldint21+0, edx
	mov word ptr oldint21+4, cx
	mov ecx, cs
	mov edx, offset myint21
	mov ax, 205h
	int 31h
	jc error

	mov edx, dword ptr dfOldISR+0
	mov cx, word ptr dfOldISR+4
	mov bl, interrupt
	mov dword ptr [currsndint+0], edx
	mov word ptr [currsndint+4], cx
	mov [intno], bl
	mov [dsseg], ds
	mov eax, 1
	ret
error:
	xor eax, eax
	ret
_hdpmi_InstallInt31 endp

_hdpmi_UninstallInt31 proc public uses ebx

	mov edx, dword ptr [oldint21+0]
	mov cx, word ptr [oldint21+4]
	mov bl, 21h
	mov ax, 205h
	int 31h
	jc error

	mov edx, dword ptr [oldint31+0]
	mov cx, word ptr [oldint31+4]
	mov bl, 31h
	mov ax, 205h
	int 31h
	jc error

	mov eax, 1
	ret
error:
	xor eax, eax
	ret
_hdpmi_UninstallInt31 endp

	END
