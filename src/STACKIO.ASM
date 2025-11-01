
;--- switch stack, call port trap function, update client registers;
;--- both from protected-mode and real-mode.

	.386
	.MODEL ?MODEL, c
	option casemap:none
	option proc:private

	include config.inc
	include debug.inc

;--- COPYRMCS must be 1 to make vsbhda fully reentrant.

COPYRMCS   equ 1	; 1=copy RMCS of rmcb to protected-mode stack and use that instance
TRAPF_OUT  equ 4

IRQCOMPAT  equ 0	; 1=generate a compatible stack frame so VIRQ_Invoke can be called

;--- description for what apps the following setting may be needed:
;--- tyrian 2000?

IRETFIX    equ 1	; 1=support cmdline option /CF2 (fix for "interrupts remain disabled after IRET" problem)

if IRETFIX
externdef gvars:GLOBALVARS
endif

PTRAP_PM_TrapHandler proto :word, :dword, :dword

ifdef NOTFLAT

STACKCORR equ 2048
ASSUMEDS textequ <DGROUP>
insRETF  textequ <retw>
PRFXDS   textequ <cs:>

else

STACKCORR equ 4096
ASSUMEDS textequ <_DATA>
insRETF textequ <retd>
PRFXDS  textequ <>

endif

;--- stack frame for StackSwitch()

IOEXCFR struct
		dd ?	; es
		dd ?	; ds
		PUSHADS <>
dwMode  dd ?	; dwMode
		EXCFRAME <>
IOEXCFR ends

externdef dwDS:dword	; it's in _TEXT

	.data

;	comm c __djgpp_stack_top:dword
externdef c __djgpp_stack_top:dword

	.code

SwitchStackIOIn proc public
	push 0
	jmp SwitchStackIO
SwitchStackIOIn endp
SwitchStackIOOut proc public
	push TRAPF_OUT
SwitchStackIOOut endp

;--- switch stack, then call protected-mode port trap handler;
;--- PTRAP_PM_TrapHandler(port, flags, value)

SwitchStackIO proc

	pushad
	push ds
	push es
	mov ebp, esp
ifdef NOTFLAT
	mov cx, [ebp].IOEXCFR._errcode
	and cx, 7
	add [ebp].IOEXCFR._ip, cx
	mov cx, [ebp].IOEXCFR._errcode
else
	mov ecx, [ebp].IOEXCFR._errcode
	and ecx, 7
	add [ebp].IOEXCFR._ip, ecx
	mov ecx, [ebp].IOEXCFR._errcode
endif
	and cl, 0f8h
	or cl, byte ptr [ebp].IOEXCFR.dwMode   ;flags bit 2: 1=out, 0=in
	test cl, 8h	; string?
	jnz @F
	test cl, 40h; port in DX?
	jz @F
	movzx dx, ch
@@:
	cld
	mov ds, cs:[dwDS]

	assume ds:ASSUMEDS

;--- v1.8: IRETFIX now for both 32- and 16-bit;
;---       also: to become active, /CF2 has to be set!
if IRETFIX
	cmp dx, 20h ; master PIC?
	jnz @F
	test cl, TRAPF_OUT	; OUT instr?
	jz @F
 if 0
	cmp al, 20h	; just the unspecific EOI is detected!
	jnz @F
 else
	test al, 20h; v1.8: all EOIs are detected!
	jz @F
 endif
	test byte ptr gvars.compatflags, CF_STIONEOI; auto STI on EOIs (for both master & slave)?
	jz @F
	or byte ptr [ebp].IOEXCFR._fl+1, 2	; set IF=1
@@:
endif

	mov ch, byte ptr [ebp].IOEXCFR._fl+1   ;v1.7: CH[1] holds IF

	mov ebx, ss
	sub [__djgpp_stack_top], STACKCORR
	mov ss, PRFXDS[dwDS]
	mov esp, [__djgpp_stack_top]
	push ds
	pop es
	push eax
	push ecx
	push edx
	call PTRAP_PM_TrapHandler
	mov ss, ebx
	mov esp, ebp
	add [__djgpp_stack_top], STACKCORR
	test byte ptr [ebp].IOEXCFR._errcode, 8h	; string?
	jnz isstring
	test byte ptr [ebp].IOEXCFR.dwMode, TRAPF_OUT	; OUT?
	jnz done
	mov byte ptr [ebp].IOEXCFR._eax, al
	test byte ptr [ebp].IOEXCFR._errcode, 30h	; word/dword?
	jz done
	mov word ptr [ebp].IOEXCFR._eax, ax
	test byte ptr [ebp].IOEXCFR._errcode, 20h	; dword?
	jz done
	mov [ebp].IOEXCFR._eax, eax
	jmp done
isstring:
	mov [ebp].IOEXCFR._edi, edi
	mov [ebp].IOEXCFR._esi, esi
	test byte ptr [ebp].IOEXCFR.dwMode, 80h	; REP prefix?
	jz done
	mov [ebp].IOEXCFR._ecx, 0
done:
	pop es
	pop ds
	popad
	add esp, 4	; skip mode
	insRETF		; return to DPMI host
	align 4

SwitchStackIO endp

;--- another stack switch, this time for io trapping in v86-mode.
;--- this is a realmode callback.

	.data

dwProc dd ?

	.code

SwitchStackIOrmcb proc

	cld
ifdef NOTFLAT
	movzx edi, di
	mov eax, [si]
else
	mov eax, [esi]
endif
	mov es:[edi].RMCS.rCSIP, eax
	add es:[edi].RMCS.rSP, 2+2

ifdef _DEBUG ;v1.6: dwDS may not be set yet, at least in debug version
	mov eax, es
	mov ds, eax
else
	mov ds, cs:[dwDS]
endif

	assume ds:ASSUMEDS

	sub [__djgpp_stack_top], STACKCORR
	mov ebx, ss
if IRQCOMPAT
;--- v1.7: if the SB emulation IRQ is to be called, the (saved) stack is supposed to hold
;---       client ES/DS; that's not the case here, so two NULLs have to be stored.
;---       The current code never calls the SB IRQ during IO trapping, so it's not active.
	push 0
	push 0
endif
	mov ebp, esp
	mov eax, ds
	mov ss, eax
	mov esp, [__djgpp_stack_top]
	push ebx
	push ebp
if COPYRMCS
	mov esi, edi
;	push es
;	pop ds
;	mov es, eax
	sub esp, sizeof RMCS+2
	mov ecx, (sizeof RMCS+2) shr 2
	mov edi, esp
	rep movsd
	mov edi, esp
;	mov ds, eax
endif
ife COPYRMCS
	push es
	mov es, eax
endif
	push edi
	call [dwProc]
ife COPYRMCS
	add esp, 4
	pop es
endif
	add [__djgpp_stack_top], STACKCORR
	mov ss, ebx
if IRQCOMPAT
	lea esp,[ebp+2*4]
else
	mov esp, ebp
endif
	insIRET
	align 4

SwitchStackIOrmcb endp

;--- alloc the realmode callback for realmode port trapping

_hdpmi_rmcbIO proc public uses esi edi cb:dword, regs:ptr, prmcb:ptr

	@dprintf CStr("hdpmi_rmcbIO enter",10)
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

ifndef NOTFLAT	; currently not active for 16-bit

;--- CLI trap handler
;--- will only be called if CR4.PVI is 0.

ifdef NOTFLAT
IRETWS struct
rIp		dw ?
rCS		dw ?
rFl		dw ?
IRETWS ends
else
IRETDS struct
rEip	dd ?
rCS		dd ?
rEfl	dd ?
IRETDS ends
endif

_hdpmi_CliHandler proc public

	assume ds:nothing
	push ds
	push esi

;--- if CLI is to be a NOP, activate next line
;	jmp enable

ifdef NOTFLAT
	lds si, dword ptr [esp+2*4].IRETWS.rIp
	movzx esi, si
else
	lds esi, fword ptr [esp+2*4].IRETDS.rEip
endif
	cmp esi, 3
	jb unchanged
	cmp word ptr [esi-3], 589Ch	; "pushfd" "pop eax"; needed by ID games' DOS/4G
	jz enable
	cmp byte ptr [esi-2], 9Ch	; "pushfd"; needed by Stargunner?
	jnz unchanged
enable:
	push eax
	insPUSHF
	mov ax,901h
	call cs:[oldint31]
	pop eax
unchanged:
	pop esi
	pop ds
	insIRET
_hdpmi_CliHandler endp

endif

	END
