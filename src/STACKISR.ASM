
;--- handle sound hardware interrupt

	.386
	.MODEL ?MODEL, c
	option casemap:none
	option proc:private

	include config.inc
	include debug.inc

	public dwDS

externdef c __djgpp_stack_top:dword
externdef bIntSnd:byte

STACKCHECK equ 0

ifdef NOTFLAT

STACKCORR equ 2048

	.code

dfOldSndVec PFAR ?	; original vector sound ISR
currSndvec  PFAR ?	; current vector sound ISR
dwDS    dd ?

	.data

else

STACKCORR equ 4096

	assume ds:_DATA

	.data

dfOldSndVec PFAR ?	; original vector sound ISR
	align 4
currSndvec  PFAR ?	; current vector sound ISR
	align 4
dwDS    dd ?

endif

dwISR	dd ?
if STACKCHECK
dwStackBottom dd ?
endif

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
if STACKCHECK
	mov eax, [__djgpp_stack_top]
	cmp eax, [dwStackBottom]
	jc failure
endif
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
	jz prevhdlr
if not SETIF
;--- v1.6: if SETIF is 1, then interrupts were enabled inside the sound ISR;
;---       else they must be reenabled now.
	push eax
	insPUSHF
	mov ax,901h
	call cs:[oldint31]
	pop eax
;	sti
endif
	insIRET
prevhdlr:
	jmp cs:[dfOldSndVec]
if STACKCHECK
failure:
externdef c fatal_error:near
	push 3
	call fatal_error
endif
SwitchStackISR endp

if TRAPINT21

;--- int 21h, ax=25xx,35xx called

_Snd_Notify21 proc public
	cmp ah, 25h
	jz is_25
ifdef NOTFLAT
	mov bx, word ptr cs:[currSndvec+0]
	mov es, word ptr cs:[currSndvec+SEGOFS]
else
	mov ebx, dword ptr cs:[currSndvec+0]
	mov es,   word ptr cs:[currSndvec+SEGOFS]
endif
	clc  ; return NC - call won't be routed to DPMI host
	ret
is_25:
	push es
ifdef NOTFLAT
	mov es, cs:[_csalias]
	assume es:_TEXT
else
	mov es, cs:[dwDS]
endif
	mov dword ptr es:[currSndvec+0], edx
	mov word ptr es:[currSndvec+SEGOFS], ds
	pop es
ifdef NOTFLAT
	assume es:DGROUP
endif
	clc  ; return NC - call won't be routed to DPMI host
	ret
_Snd_Notify21 endp

endif

NOTFR struct
	dd ?	;return
	IRETS <>
NOTFR ends

;--- int 31h, ax=204/205 called with BL=snd interrupt

_Snd_Notify31 proc public
	and byte ptr [esp].NOTFR._fl, not 1
	cmp al, 5
	jz is_05
ifdef NOTFLAT
	mov dx, word ptr cs:[currSndvec+0]
else
	mov edx, dword ptr cs:[currSndvec+0]
endif
	mov cx, word ptr cs:[currSndvec+SEGOFS]
	clc  ; return NC - call won't be routed to DPMI host
	ret

is_05:
	push ds
ifdef NOTFLAT
	mov ds, cs:[_csalias]
	assume ds:_TEXT
else
	mov ds, cs:[dwDS]
endif
	mov dword ptr [currSndvec+0], edx
	mov word ptr [currSndvec+SEGOFS], cx
	pop ds
ifdef NOTFLAT
	assume ds:DGROUP
endif
	clc  ; return NC - call won't be routed to DPMI host
	ret
_Snd_Notify31 endp

;--- install the sound ISR
;--- the idea is to hide this ISR from the ring3 DOS extender;

_SND_InstallISR proc public uses ebx interrupt:byte, isr:dword

	@dprintf CStr("SND_InstallISR enter",10)

	mov eax, isr
	mov dwISR, eax

	mov bl, interrupt
	mov ax, 204h
	int 31h
	jc error
ifdef NOTFLAT
	push ds
	mov ds, cs:[_csalias]
	assume ds:_TEXT
endif
	mov dword ptr [dfOldSndVec + 0], edx
	mov word ptr [dfOldSndVec + SEGOFS], cx
	mov dword ptr [currSndvec + 0], edx
	mov word ptr [currSndvec + SEGOFS], cx

	mov dwDS, ss
	mov [bIntSnd], bl	; activates notification by int 31h handler

ifdef NOTFLAT
	pop ds
	assume ds:DGROUP
endif
	mov ecx, cs
	mov edx, offset SwitchStackISR
	mov ax, 205h
	int 31h
	jc error

if STACKCHECK
	mov eax, [__djgpp_stack_top]
	sub eax, 10000h
	mov [dwStackBottom], eax
endif

	mov eax, 1
	ret
error:
	xor eax, eax
	ret
_SND_InstallISR endp

_SND_UninstallISR proc public uses ebx interrupt:byte

	@dprintf CStr("SND_UninstallISR enter",10)
	mov edx, dword ptr dfOldSndVec
	mov cx, word ptr dfOldSndVec+SEGOFS
	mov bl, interrupt
	mov ax, 205h
	int 31h
	jc error
	mov eax, 1
	ret
error:
	xor eax, eax
	ret

_SND_UninstallISR endp

	END
