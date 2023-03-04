
;--- handle sound hardware interrupt

	.386
	.MODEL small, c
	option casemap:none
	option proc:private

	.data

	public dfOldISR

dfOldISR label fword
dwOfsPM	dd ?	; old sound ISR
wSegPM	dw ?,?	; old sound ISR
dwISR	dd ?
dwStack	dd ?
dwDS    dd ?
intno	db ?

	.code

	assume ds:_DATA

;--- ISR for sound hw interrupt occuring in protected-mode

SwitchStackISR proc

	pushad
	push ds
	push es
	mov ds, cs:[dwDS]
	mov es, [dwDS]
	cld
	mov edx, ss
	cmp dx, word ptr [dwDS]
	jz noswitch
	mov ecx, esp
	lss esp, fword ptr [dwStack]
	push edx
	push ecx
	call [dwISR]
	lss esp, [esp]
	jmp switchdone
noswitch:
if 0
	mov ax, 3
	int 10h
	mov ax, 0E5Ah
	mov bh,0
	int 10h
	jmp $
endif
	call [dwISR]
switchdone:
	and eax, eax	; interrupt handled?
	pop es
	pop ds
	popad
	jz @F
	sti
	iretd
@@:
	jmp cs:[dfOldISR]
SwitchStackISR endp

;--- install the sound ISR
;--- the idea is to hide this ISR from the ring3 DOS extender;

_hdpmi_InstallISR proc public uses ebx interrupt:byte, isr:dword, pStack:ptr

	mov bl, interrupt
	mov ax, 204h
	int 31h
	jc error
	mov dwOfsPM, edx
	mov wSegPM, cx
	mov intno, bl
	mov edx, pStack
	mov dwStack, edx
	mov edx, isr
	mov dwISR, edx
	mov dwDS, ds
	mov ecx, cs
	mov edx, offset SwitchStackISR
	mov ax, 205h
	int 31h
	jc error
	mov eax, 1
	ret
error:
	xor eax, eax
	ret
_hdpmi_InstallISR endp

_hdpmi_UninstallISR proc public uses ebx
	mov edx, dwOfsPM
	mov cx, wSegPM
	mov bl, intno
	mov ax, 205h
	int 31h
	jc error
	mov eax, 1
	ret
error:
	xor eax, eax
	ret
_hdpmi_UninstallISR endp

	END
