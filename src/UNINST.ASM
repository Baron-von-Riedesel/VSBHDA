
;--- a) uninstall vsbhda
;--- b) detect if vsbhda is already installed

	.386
	.MODEL ?MODEL
	option casemap:none
	option proc:private

	include config.inc

__dpmi_simulate_real_mode_interrupt proto c intno:byte, pRegs:ptr

ifdef NOTFLAT
ASSUMEDS textequ <DGROUP>
else
ASSUMEDS textequ <_DATA>
endif

	.data

if @Model ne 7
externdef c DSBase:dword
endif

	.code

	assume ds:ASSUMEDS

;--- check if vsbhda is installed already.
;--- since vsbhda hides itself quite good, the simplest
;--- method is to access the SB ports.
;--- to be fixed: currently works with base=220h only.

_is_installed proc c public

	mov dx, 226h
	mov al, 1
	out dx, al
	in al, dx
	xor al, al
	out dx, al
	mov dx, 22Ah
	in al, dx
	cmp al, 0AAh
	setz al
	movzx eax, al
	ret
_is_installed endp

;--- uninstall vsbhda.
;--- this is initiated from real-mode, by accessing
;--- SB ports in an unusual way. This is detected by
;--- VSB.C and then this proc is called. What's done then is:
;--- + set TSR's parent PSP to current PSP;
;--- + set TSR's int 22h to current PSP:0000;
;--- + switch to TSR PSP
;--- + run an int 21h, ah=4Ch in pm

_uninstall_tsr proc c public dwLinPSP:dword

local regs:RMCS

	mov byte ptr regs.rEAX+1, 51h	; get current PSP in BX (segment)
	mov regs.rFlags, 202h
	mov regs.rSSSP, 0
	invoke __dpmi_simulate_real_mode_interrupt, 21h, addr regs

	mov esi, dwLinPSP
if @Model ne 7
	sub esi, DSBase
endif

if 0	; adjust PSP:2Eh? probably not a good idea.
	movzx ebx, word ptr regs.rEBX
	shl ebx, 4
 if @Model ne 7
	sub ebx, DSBase
 endif
	mov eax, [ebx+2Eh]
	mov [esi+2Eh], eax
endif

	mov ax, word ptr regs.rEBX
	mov word ptr [esi+0Ah], 0
	mov word ptr [esi+0Ch], ax
	mov word ptr [esi+16h], ax
	mov eax, dwLinPSP
	shr eax, 4
	mov word ptr regs.rEBX, ax
	mov byte ptr regs.rEAX+1, 50h	; set current PSP
	invoke __dpmi_simulate_real_mode_interrupt, 21h, addr regs
	mov ax, 4C00h
	int 21h

_uninstall_tsr endp

	END
