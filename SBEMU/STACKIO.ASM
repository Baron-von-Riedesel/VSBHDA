
;--- switch stack, call port trap function, update client registers

	.386
	.MODEL small, c
	option casemap:none
	option proc:private

TRIGGERIRQ equ 0

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

if TRIGGERIRQ
externdef SBEMU_TriggerIRQ:byte
SBEMU_GetIRQ proto
VIRQ_Invoke proto :dword
endif

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
	push eax
	push ecx
	push edx
	call ebp
	lss esp, [ebx+2*4]
if TRIGGERIRQ
	cmp [SBEMU_TriggerIRQ],0
	jz @F
	test byte ptr [esp+2*4].EXCFRAME.dwEfl+1, 2	;IF set?
	jz @F
	lss esp, [ebx]
	call callirq
	lss esp, [ebx+2*4]
@@:
endif
	jmp switchdone
noswitch:
	push eax
	push ecx
	push edx
	call [ebp].EXCFRAME.pProc
	add esp, 3*4
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

if TRIGGERIRQ
callirq:
	push eax
	mov [SBEMU_TriggerIRQ],0
	invoke SBEMU_GetIRQ
	invoke VIRQ_Invoke, eax
	pop eax
	retn
endif

SwitchStackIO endp

	END
