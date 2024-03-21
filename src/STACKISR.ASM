
;--- handle sound hardware interrupt

	.386
ifdef ?FLAT
	.MODEL flat, c
else
	.MODEL small, c
endif
	option casemap:none
	option proc:private

	public dfOldISR
	public dwDS

externdef c __djgpp_stack_top:dword

ifdef NOTFLAT

STACKCORR equ 2048

externdef c _csalias:dword

SEGOFS equ 2
insIRET textequ <IRET>
PF16 typedef ptr far16

	.code

dfOldISR PF16 ?
dwDS    dd ?

else

STACKCORR equ 4096

	assume ds:_DATA
SEGOFS equ 4
insIRET textequ <IRETD>

	.data

dfOldISR df ?
dwDS    dd ?

endif

	.data

dwISR	dd ?
intno	db ?

	.code

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
	sub [__djgpp_stack_top], STACKCORR
ifdef _DEBUG
	mov eax, [__djgpp_stack_top]	; test if the new stack is writable before ss is changed
	mov [eax-4], eax
endif
	mov ss, [dwDS]
	mov esp, [__djgpp_stack_top]
	push edx
	push ecx
	call [dwISR]
	lss esp, [esp]
	add [__djgpp_stack_top], STACKCORR
	and eax, eax	; interrupt handled?
	pop es
	pop ds
	popad
	jz @F
	sti
	insIRET
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
ifdef NOTFLAT
	push ds
	mov ds, cs:[_csalias]
	assume ds:_TEXT
endif
	mov dword ptr dfOldISR+0, edx
	mov word ptr dfOldISR+SEGOFS, cx
	mov dwDS, ss
ifdef NOTFLAT
	pop ds
	assume ds:DGROUP
endif
	mov intno, bl
	mov edx, isr
	mov dwISR, edx

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
	mov edx, dword ptr dfOldISR
	mov cx, word ptr dfOldISR+SEGOFS
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
