
;--- switch stack, call port trap function, update client registers

	.386
	.MODEL small, c
	option casemap:none
	option proc:private

PUSHADS struct
_edi	dd ?
_esi	dd ?
_ebp	dd ?
		dd ?
_ebx	dd ?
_edx	dd ?
_ecx	dd ?
_eax	dd ?
PUSHADS ends

;--- stack frame for StackSwitch()

EXCFRAME struct
		PUSHADS <>
		dd ?	; ret addr
pProc   dd ?	; pProc
dwMode  dd ?	; dwMode
pDS 	dd ?	; pDS
		dd ?	; ??? GCC has created a "push eax" ???
		dd ?	; DPMI ret 
		dd ?	; DPMI ret
dwErr	dd ?
dwEip	dd ?
wCS		dw ?,?
dwEfl	dd ?
dwEsp	dd ?
wSS		dw ?,?
EXCFRAME ends


	.data

externdef c HDPMIPT_OldESP:fword
externdef c HDPMIPT_NewStack:fword

	.code

	assume ds:_DATA

SwitchStack proc public

	pushad
	mov ebp, esp
	mov ecx, [ebp].EXCFRAME.dwErr
	mov ebx, [ebp].EXCFRAME.pDS
	and ecx, 7
	add [ebp].EXCFRAME.dwEip, ecx
	mov ecx, [ebp].EXCFRAME.dwErr
	and cl, 0f8h
	or cl, byte ptr [ebp].EXCFRAME.dwMode
	test cl, 8h	; string?
	jnz @F
	test cl, 40h; port in DX?
	jz @F
	movzx dx, ch
@@:
	push ds
	push es
	mov ds, cs:[ebx]
	mov es, cs:[ebx]
	mov dword ptr HDPMIPT_OldESP+0, esp
	mov word ptr  HDPMIPT_OldESP+4, ss
	mov ebx, [ebp].EXCFRAME.pProc
	lss esp, HDPMIPT_NewStack
	movzx edx, dx
	call ebx
	lss esp, HDPMIPT_OldESP
	pop es
	pop ds
	test byte ptr [ebp].EXCFRAME.dwErr, 8h	; string?
	jnz isstring
	cmp [ebp].EXCFRAME.dwMode, 0			; IN?
	jnz done
	mov byte ptr [ebp].EXCFRAME._eax, al
	test byte ptr [ebp].EXCFRAME.dwErr, 30h	; word/dword?
	jz done
	mov word ptr [ebp].EXCFRAME._eax, ax
	test byte ptr [ebp].EXCFRAME.dwErr, 20h	; dword?
	jz done
	mov [ebp].EXCFRAME._eax, eax
	jmp done
isstring:
	mov [ebp].EXCFRAME._edi, edi
	mov [ebp].EXCFRAME._esi, esi
	mov [ebp].EXCFRAME._ecx, ecx
done:
	popad
	retn 4*4	; the caller does not clean the stack!

SwitchStack endp

if 0

;--- display 32-bit number on screen
;--- low-level.

	.data

flatdesc dd 0

	.code

	assume ds:_DATA

DispNumber proc public number:dword, pos:dword

	pushad
	lar eax, [flatdesc]
	jz @F
	mov ax,0
	mov cx,1
	int 31h
	jc exit
	mov ebx,eax
	mov [flatdesc], eax
	or cx,-1
	or dx,-1
	mov ax,8
	int 31h
@@:
	push ds
	mov ds, [flatdesc]
	mov ebx, pos
	shl ebx, 1
	add ebx, 0B8000h
	mov eax, number
	mov edi, 10
@@nextdigit:
	xor edx, edx
	div edi
	add dl,'0'
	cmp dl,'9'
	jbe @F
	add dl,7+20h
@@:
	mov [ebx],dl
	sub ebx, 2
	and eax, eax
	jne @@nextdigit
	pop ds
exit:
	popad
	ret

DispNumber endp

endif

	END
