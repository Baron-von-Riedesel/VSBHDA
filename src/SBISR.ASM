
;--- handle SB emulated interrupt
;--- since this interrupt is emulated, there shouldn't
;--- be any real interrupt hooking.

	.386
	.MODEL ?MODEL, c
	option casemap:none
	option proc:private

;--- currently, WATCHSB = 1 works (for DOS/4GW) with SB IRQ 7
;--- only if notify() returns with C ( meaning the protected-mode
;--- vector will be set ). SB irq 5 has no such limitations.

WATCHSB   equ 1	; 1=watch SB int

	include config.inc
	include debug.inc

externdef c VIRQ_Irq:byte
externdef c __djgpp_stack_top:dword
externdef dwDS:dword
externdef bIntSB:byte

ifdef NOTFLAT

	.code

currSBvec  PFAR ?	; current vector for SB ISR

else

	assume ds:_DATA

	.data

currSBvec  PFAR ?	; current vector for SB ISR
	align 4

endif

	.code

;--- runs the SB IRQ emulation
;--- + switch to the client stack
;--- + restore client's DS,ES regs - since FS/GS are untouched by vsbhda, this finally restores all segment registers.
;--- + clear interrupt flag
;--- + call the SB isr proc
;--- + set interrupt flag

_SBIsrCall proc syscall public uses ds es ebx ebp

	mov ebp, esp
	mov ebx, [__djgpp_stack_top]
	lss esp, [ebx-8]

if WATCHSB
;--- calling the SB isr directly would usually be incorrect,
;--- since there's no switch to the LPMS then. However, since
;--- the SB isr is always called from inside the sound hw isr,
;--- this should pose no problem at all.
	pop es
	pop ds
	insPUSHF

	mov ax, 900h; disable ints
	insPUSHF
	call cs:[oldint31]

	call cs:[currSBvec]
else

	cmp [VIRQ_Irq], 5
	pop es
	pop ds
	ja is0F
	jz is0D
	int 0Ah
	jmp exit
is0D:
	int 0Dh
	jmp exit
is0F:
	int 0Fh
exit:
endif
	push ds
	push es
	mov ss, cs:[dwDS]
	mov esp, ebp
if 1
	pushfd
	pop eax
	test ah, 2	; IF==1
	jnz @F
	mov ax, 901h; enable ints
	insPUSHF
	call cs:[oldint31]
@@:
endif
	ret
_SBIsrCall endp

if TRAPINT21

;--- DOS 25/35
;--- in: ax=25xx
;---     DS:EDX=new vector
;--- ax=35xx
;--- out: ES:EBX=curr vector

_SB_Notify21 proc public

if WATCHSB
	cmp ah, 25h
	jz is_25
  ifdef NOTFLAT
	mov bx, word ptr cs:[currSBvec+0]
	mov es, word ptr cs:[currSBvec+SEGOFS]
  else
	mov ebx, dword ptr cs:[currSBvec+0]
	mov es,   word ptr cs:[currSBvec+SEGOFS]
  endif
	clc
	ret
is_25:
	push es
 ifdef NOTFLAT
	mov es, cs:[_csalias]
	assume es:_TEXT
 else
	mov es, cs:[dwDS]
 endif
	mov dword ptr es:[currSBvec+0], edx
	mov  word ptr es:[currSBvec+SEGOFS], ds
	pop es
 ifdef NOTFLAT
	assume es:DGROUP
 endif
	clc
	ret
endif

_SB_Notify21 endp

endif

;--- _SB_Notify31: called by int 31h handler ( int31.asm ) if SB int is read/modified
;--- in: ax=204/205
;---     bl=int#
;---     cx:edx=new value if al=5
;--- out: cx:edx=old value if al=4

NOTFR struct
	dd ?	;return
	IRETS <>
NOTFR ends

_SB_Notify31 proc public

if WATCHSB
	and byte ptr [esp].NOTFR._fl, not 1
	cmp al, 4
	jz is_04
	call savevector
	stc
	ret
is_04:
 ifdef NOTFLAT
	mov dx, word ptr cs:[currSBvec+0]
 else
	mov edx, dword ptr cs:[currSBvec+0]
 endif
	mov cx, word ptr cs:[currSBvec+SEGOFS]
	stc
	ret

savevector::
	push ds
 ifdef NOTFLAT
	mov ds, cs:[_csalias]
	assume ds:_TEXT
 else
	mov ds, cs:[dwDS]
 endif
	mov dword ptr [currSBvec+0], edx
	mov word ptr [currSBvec+SEGOFS], cx

	mov [bIntSB], bl	; activate notifications

	pop ds
 ifdef NOTFLAT
	assume ds:DGROUP
 endif
	ret

endif

_SB_Notify31 endp

;--- init the SB interrupt handling.

_SB_InstallISR proc public uses ebx interrupt:byte

	@dprintf CStr("SB_InstallISR enter",10)
if WATCHSB
	mov bl, interrupt
	mov ax, 204h
	int 31h
	call savevector
endif
	mov eax, 1
	ret
_SB_InstallISR endp

_SB_UninstallISR proc public interrupt:byte

	mov eax, 1
	ret

_SB_UninstallISR endp

	END
