
;--- switch stack, call port trap function, update client registers

	.386
	.MODEL small, c
	option casemap:none
	option proc:private

TRIGGERIRQ equ 0
COPYRMCS   equ 0

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

;--- another stack switch, this time for io trapping in v86-mode.
;--- this is a realmode callback.
;--- COPYRMCS will save the RMCS onto the protected-mode stack,
;--- then the code becomes reentrant - this would probably allow
;--- to trigger a "virtual interrupt" inside.

	.data

externdef HDPMIPT_NewStack:dword

dwProc dd ?

	.code

RMCS    struct          ;real mode call structure
rEDI    dd ?            ;+0
rESI    dd ?            ;+4
rEBP    dd ?            ;+8
        dd ?            ;+12
rEBX    dd ?            ;+16
rEDX    dd ?            ;+20
rECX    dd ?            ;+24
rEAX    dd ?            ;+28
rFlags  dw ?            ;+32
rES     dw ?            ;+34
rDS     dw ?            ;+36
rFS     dw ?            ;+38    
rGS     dw ?            ;+40  
rIP     dw ?
rCS     dw ?
rSP     dw ?
rSS     dw ?
RMCS    ends

SwitchStackIOrmcb proc

	cld
	lodsw
	mov es:[edi].RMCS.rIP, ax
	lodsw
	mov es:[edi].RMCS.rCS, ax
	add es:[edi].RMCS.rSP, 2+2
	mov eax, esp
	mov edx, ss
	cmp dx, word ptr cs:[HDPMIPT_NewStack+4]
	jz noswitch
	lss esp, fword ptr cs:[HDPMIPT_NewStack]
if COPYRMCS
	mov esi, edi
	push es
	pop ds
	push ss
	pop es
	sub esp, sizeof RMCS+2
	mov ecx, (sizeof RMCS+2) shr 2
	mov edi, esp
	rep movsd
	mov edi, esp
endif
	push edx
	push eax
ife COPYRMCS
	push es
endif
	mov eax, ss
	mov ds, eax
	assume ds:_DATA
	mov es, eax
	push edi
	call [dwProc]
	add esp, 4
ife COPYRMCS
	pop es
endif
	lss esp, [esp]
	iretd
noswitch:
if COPYRMCS
	mov esi, edi
	push es
	pop ds
	push ss
	pop es
	sub esp, sizeof RMCS+2
	mov ecx, (sizeof RMCS+2) shr 2
	mov edi, esp
	rep movsd
	mov edi, esp
else
	push es
endif
	mov eax, ss
	mov ds, eax
	mov es, eax
	push edi
	call [dwProc]
	add esp, 4
ife COPYRMCS
	pop es
endif
	iretd
SwitchStackIOrmcb endp

_hdpmi_rmcbIO proc public uses esi edi cb:dword, regs:ptr, prmcb:ptr
	push ds
	mov esi, offset SwitchStackIOrmcb
	push cs
	pop ds
	mov edi, regs
	mov ax,0303h
	int 31h
	pop ds
	jc error
	mov eax, cb
	mov dwProc, eax
	mov eax, prmcb
	mov [eax+0], dx
	mov [eax+2], cx
	mov eax, 1
	ret
error:
	xor eax, eax
	ret
_hdpmi_rmcbIO endp

	END
