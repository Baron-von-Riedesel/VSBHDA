
;--- handle sound hardware interrupt

	.386
	.MODEL ?MODEL, c
	option casemap:none
	option proc:private

	public dfOldISR
	public dwDS

	include debug.inc

SETIF equ 1	;must match SETIF in config.h

externdef c __djgpp_stack_top:dword

ifdef NOTFLAT

STACKCORR equ 2048

externdef c _csalias:dword

SEGOFS equ 2
insIRET textequ <iret>
insPUSHF textequ <pushf>
PF16 typedef ptr far16

	.code

externdef oldint31:PF16

dfOldISR PF16 ?
dwDS    dd ?

else

STACKCORR equ 4096

	assume ds:_DATA
SEGOFS equ 4
insIRET textequ <iretd>
insPUSHF textequ <pushfd>

	.data

externdef oldint31:FWORD

dwDS    dd ?
dfOldISR df ?

endif

	.data

	align 4
dwISR	dd ?
intno	db ?

	.code

PUSHADS struct
_Edi	dd ?
_Esi	dd ?
_Ebp	dd ?
		dd ?
_Ebx	dd ?
_Edx	dd ?
_Ecx	dd ?
_Eax	dd ?
PUSHADS ends

;--- this is supposed to be the status of the client stack
;--- it's valid for SwitchStackISR and SwitchStackIO, but
;--- not for SwitchStackIOrmcb!

CLFRAME struct
_Es     dd ?
_Ds     dd ?
        PUSHADS <>
CLFRAME ends

;--- this variant runs the SB IRQ emulation on the client stack.

_FastCall proc syscall public uses ds es ebx esi edi bIrq:byte

	movzx ecx, bIrq
	mov ebx, [__djgpp_stack_top]
	mov edi, ss
	mov esi, esp
	lss esp, [ebx-8]
	pop es
	pop ds
	cmp cl,5
	jz is5
	int 0Fh
	jmp exit
is5:
	int 0Dh
exit:
	push ds
	push es
	mov ss, edi
	mov esp, esi
	ret
_FastCall endp

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
ife SETIF
;--- if SETIF==1, interrupts were enabled inside SNDISR. But
;--- it might still be a good idea to enable ints here, in case
;--- VIRQ_Invoke() did disable interrupts.
 if 0
	sti
 else
	push eax
	mov ax, 901h
	insPUSHF
	call cs:[oldint31]
	pop eax
 endif
endif
	insIRET
@@:
	jmp cs:[dfOldISR]
SwitchStackISR endp

;--- install the sound ISR
;--- the idea is to hide this ISR from the ring3 DOS extender;

_hdpmi_InstallISR proc public uses ebx interrupt:byte, isr:dword

	@dprintf CStr("hdpmi_InstallISR enter",10)
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

	@dprintf CStr("hdpmi_UninstallISR enter",10)
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
