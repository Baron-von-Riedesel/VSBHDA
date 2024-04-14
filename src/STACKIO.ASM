
;--- switch stack, call port trap function, update client registers;
;--- both from protected-mode and real-mode.

	.386
	.MODEL ?MODEL, c
	option casemap:none
	option proc:private

	include debug.inc

;--- COPYRMCS must be 1 to make vsbhda fully reentrant.

COPYRMCS   equ 1	; 1=copy RMCS of rmcb to protected-mode stack and use that instance
TRIGGERIRQ equ 1	; 1=run virtual SB IRQ triggered by port access
IRETFIX16  equ 1	; 1=hackish fix for "interrupts remain disabled after IRET" problem (16-bit)

PTRAP_PM_TrapHandler proto :word, :dword, :dword

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

ifdef NOTFLAT

STACKCORR equ 2048
ASSUMEDS textequ <DGROUP>
insIRET textequ <iret>
insRETF textequ <retw>
PRFXDS  textequ <cs:>

EXCFRAME struct
		dd ?	; es
		dd ?	; ds
		PUSHADS <>
dwMode  dd ?	; dwMode
		dw ?	; DPMI ret 
		dw ?	; DPMI ret
dwErr	dw ?
dwEip	dw ?
wCS		dw ?
dwEfl	dw ?
dwEsp	dw ?
wSS		dw ?
EXCFRAME ends

else

STACKCORR equ 4096
ASSUMEDS textequ <_DATA>
insIRET textequ <iretd>
insRETF textequ <retd>
PRFXDS  textequ <>

EXCFRAME struct
		dd ?	; es
		dd ?	; ds
		PUSHADS <>
dwMode  dd ?	; dwMode
		dd ?	; DPMI ret 
		dd ?	; DPMI ret
dwErr	dd ?
dwEip	dd ?
wCS		dw ?,?
dwEfl	dd ?
dwEsp	dd ?
wSS		dw ?,?
EXCFRAME ends
endif

externdef dwDS:dword	; it's in _TEXT

	.data

if TRIGGERIRQ
externdef VSB_TriggerIRQ:dword
VIRQ_Invoke proto c
endif

;	comm c __djgpp_stack_top:dword
externdef c __djgpp_stack_top:dword

	.code

SwitchStackIOIn proc public
	push 0
	jmp SwitchStackIO
SwitchStackIOIn endp
SwitchStackIOOut proc public
	push 1
SwitchStackIOOut endp

;--- switch stack, then call protected-mode port trap handler;
;--- handler expects port in dx, flags in cx and value in ax

SwitchStackIO proc

	pushad
	push ds
	push es
	mov ebp, esp
ifdef NOTFLAT
	mov cx, [ebp].EXCFRAME.dwErr
	and cx, 7
	add [ebp].EXCFRAME.dwEip, cx
	mov cx, [ebp].EXCFRAME.dwErr
else
	mov ecx, [ebp].EXCFRAME.dwErr
	and ecx, 7
	add [ebp].EXCFRAME.dwEip, ecx
	mov ecx, [ebp].EXCFRAME.dwErr
endif
	and cl, 0f8h
	or cl, byte ptr [ebp].EXCFRAME.dwMode
	test cl, 8h	; string?
	jnz @F
	test cl, 40h; port in DX?
	jz @F
	movzx dx, ch
@@:
	cld
	mov ds, cs:[dwDS]

ifdef NOTFLAT
 if IRETFIX16
	cmp dx, 20h
	jnz @F
	cmp al, 20h
	jnz @F
	test cl, 1	; so it's an EOI sent to master PIC
	jz @F
	or byte ptr [ebp].EXCFRAME.dwEfl+1, 02
@@:
 endif
endif

	assume ds:ASSUMEDS

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
if TRIGGERIRQ
	cmp [VSB_TriggerIRQ], 0
	jz @F
	test byte ptr [ebp].EXCFRAME.dwEfl+1, 2		; interrupts enabled?
	jz @F
	sub [__djgpp_stack_top], STACKCORR
	mov ss, PRFXDS[dwDS]
	mov esp, [__djgpp_stack_top]
	call VIRQ_Invoke
	mov ss, ebx
	mov esp, ebp
	add [__djgpp_stack_top], STACKCORR
@@:
endif
	pop es
	pop ds
	popad
	add esp, 4	; skip mode
	insRETF		; return to DPMI host
	align 4

SwitchStackIO endp

;--- another stack switch, this time for io trapping in v86-mode.
;--- this is a realmode callback.
;--- COPYRMCS will save the RMCS onto the protected-mode stack,
;--- then the code becomes reentrant - this would probably allow
;--- to trigger a "virtual interrupt" inside.

	.data

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

	mov ds, cs:[dwDS]

	assume ds:ASSUMEDS

	sub [__djgpp_stack_top], STACKCORR
	mov ebx, ss
	mov ebp, esp
	mov eax, ds
	mov ss, eax
	mov esp, [__djgpp_stack_top]
if COPYRMCS
	mov esi, edi
	push es
	pop ds
	mov es, eax
	sub esp, sizeof RMCS+2
	mov ecx, (sizeof RMCS+2) shr 2
	mov edi, esp
	rep movsd
	mov edi, esp
	mov ds, eax
endif
ife COPYRMCS
	push es
	mov es, eax
endif
	push edi
	call [dwProc]
if TRIGGERIRQ
	cmp [VSB_TriggerIRQ], 0
	jz @F
	test byte ptr es:[edi].RMCS.rFlags+1, 2	; interrupts enabled?
	jz @F
	call VIRQ_Invoke
@@:
endif
	add [__djgpp_stack_top], STACKCORR
ife COPYRMCS
	add esp, 4
	pop es
endif
	mov ss, ebx
	mov esp, ebp
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
	mov ax,901h
	int 31h
	pop eax
unchanged:
	pop esi
	pop ds
	insIRET
_hdpmi_CliHandler endp

endif

	END
