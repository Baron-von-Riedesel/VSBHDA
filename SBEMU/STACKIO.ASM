
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
pStack 	dd ?	; pNewStack
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

	.code

;--- switch stack, then call protected-mode port trap handler;
;--- handler expects port in dx, flags in cx and value in ax

SwitchStackIO proc public

	pushad
	mov ebp, esp
	mov ecx, [ebp].EXCFRAME.dwErr
	mov ebx, [ebp].EXCFRAME.pStack
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
	cld
	movzx edx, dx
	push ds
	push es

	push eax
	push edx
	mov eax, cs:[ebx+4]
	mov edx, ss
	mov ds, eax
	mov es, eax
	cmp ax, dx
	pop edx
	pop eax
	jz noswitch
	mov dword ptr [ebx+2*4], esp
	mov word ptr  [ebx+3*4], ss
	mov ebp, [ebp].EXCFRAME.pProc
	lss esp, [ebx]
	call ebp
	lss esp, [ebx+2*4]
	jmp switchdone
noswitch:
if 0
	mov ax, 3
	int 10h
	mov ax, 0E58h
	mov bh,0
	int 10h
	jmp $
endif
	call [ebp].EXCFRAME.pProc
switchdone:
	pop es
	pop ds
	mov ebp, esp
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
	test byte ptr [ebp].EXCFRAME.dwMode, 80h	; REP prefix?
	jz done
	mov [ebp].EXCFRAME._ecx, 0
done:
	popad
	retn 4*4	; the caller does not clean the stack!

SwitchStackIO endp

	END
