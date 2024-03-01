
;--- handle sound hardware interrupt

	.386
ifdef ?FLAT
	.MODEL flat, c
else
	.MODEL small, c
endif
	option casemap:none
	option proc:private

	.data

	public dfOldISR
	public dwDS

dfOldISR label fword
dwOfsPM	dd ?	; old sound ISR
wSegPM	dw ?,?	; old sound ISR
dwISR	dd ?
dwDS    dd ?
intno	db ?

;	comm c __djgpp_stack_top:dword
externdef c __djgpp_stack_top:dword

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
	mov ecx, esp
	sub [__djgpp_stack_top], 4096
	mov ss, [dwDS]
	mov esp, [__djgpp_stack_top]
	push edx
	push ecx
	call [dwISR]
	lss esp, [esp]
	add [__djgpp_stack_top], 4096
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

_hdpmi_InstallISR proc public uses ebx interrupt:byte, isr:dword

	mov bl, interrupt
	mov ax, 204h
	int 31h
	jc error
	mov dwOfsPM, edx
	mov wSegPM, cx
	mov intno, bl
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
