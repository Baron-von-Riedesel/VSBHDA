
;--- handle SB emulated interrupt.

	.386
	.MODEL ?MODEL, c
	option casemap:none
	option proc:private

;--- HOSTRT:
;--- if 1, SB irq isn't modified - this was the setting in vsbhda v1.7 and v1.6.
;----      However, it's a problem if the SB irq is used by a PCI device.
;--- if 0, SB irq is modified (proc realsbisr) and the client will get/set a virtual
;---       address only.
;---       With this setting, MASKSBIRQ in virq.c should be set to 0.

WATCHSB   equ 1	; 1=watch get/set SB int
HOSTRT    equ 0	; 1=route get/set pm vector calls for irq 2/5/7 to DPMI host.
                ; 0=install own handler and intercept DPMI/DOS calls, so the
                ;   vector isn't modified.
ife HOSTRT
RTIVT     equ 1	; 1=route interrupt requests to (old) real-mode IV directly.
endif

	include config.inc
	include debug.inc

externdef c VIRQ_Irq:byte
externdef c __djgpp_stack_top:dword
externdef c DSBase:dword
externdef dwDS:dword
externdef bIntSB:byte
ifdef DJGPP
externdef c _go32_info_block:_Go32_Info_Block
_my_psp macro
	exitm <_go32_info_block.linear_address_of_original_psp>
endm
else
externdef c _linear_psp:dword
_my_psp macro
	exitm <_linear_psp>
endm
endif
copyrmcode proto c :ptr, :dword

	.data

dwOldIV0F dd 0
ife HOSTRT
 if RTIVT
dwOldIV   dd 0
 endif
endif

ifdef NOTFLAT

	.code

 if WATCHSB
currSBvec  PFAR ?	; current vector SB ISR
 endif
 ife HOSTRT
oldSBvec   PFAR ?	; original vector SB ISR
 endif
else

	assume ds:_DATA

	.data

 if WATCHSB
currSBvec  PFAR ?	; current vector SB ISR
 endif
	align 4
 ife HOSTRT
oldSBvec   PFAR ?	; original vector SB ISR
	align 4
 endif

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
	int 8+2		;irq 2
	jmp exit
is0D:
	int 8+5		;irq 5
	jmp exit
is0F:
	int 8+7		;irq 7
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
	align 4
_SBIsrCall endp

ife HOSTRT

;--- real PL3 IRQ 2/5/7 

realsbisr proc
 ife RTIVT
	jmp cs:[oldSBvec]
 else
	push es
	pushad
	sub esp, 34h
	mov edi, esp
	push ss
	pop es
	xor bx, bx
	xor ecx, ecx
  ifdef NOTFLAT
	push ds
	mov ds, cs:[dwDS]
	mov eax, [dwOldIV]
	pop ds
  else
	mov eax, cs:[dwOldIV]
  endif
	mov es:[edi].RMCS.rFlags, 2
	mov es:[edi].RMCS.rCSIP, eax
	shr eax, 16
	mov es:[edi].RMCS.rES, ax
	mov es:[edi].RMCS.rDS, ax
	mov es:[edi].RMCS.rSSSP, ecx
	mov ax, 302h
	int 31h
	add esp, 34h
	popad
	pop es
	insIRET
 endif
	align 4
realsbisr endp
endif

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
 ife HOSTRT
  ifdef NOTFLAT
	mov bx, word ptr cs:[currSBvec+0]
	mov es, word ptr cs:[currSBvec+SEGOFS]
  else
	mov ebx, dword ptr cs:[currSBvec+0]
	mov es,   word ptr cs:[currSBvec+SEGOFS]
  endif
	clc
 else
	stc	; C - call will be routed to DPMI host
 endif
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
	@dprintf CStr("SB_Notify21: DS:EDX=%X:%X",10),ds,edx
	pop es
 ifdef NOTFLAT
	assume es:DGROUP
 endif
 ife HOSTRT 
	clc
 else
	stc	; C - call will be routed to DPMI host
 endif
	ret
endif
	align 4
_SB_Notify21 endp

endif

;--- _SB_Notify31: called by int 31h handler ( int31.asm ) if SB int is read/modified
;--- in: ax=204/205
;---     bl=int#
;---     cx:edx=new value if al=5
;--- out: cx:edx=old value if al=4

ife HOSTRT
NOTFR struct
	dd ?	;return
	IRETS <>
NOTFR ends
endif

_SB_Notify31 proc public

if WATCHSB
 ife HOSTRT
	and byte ptr [esp].NOTFR._fl, not 1
 endif
	cmp al, 4
	jz is_04
	call savevector
 ife HOSTRT
	clc
 else
	stc ; C - call will be routed to DPMI host
 endif
	ret
is_04:
 ife HOSTRT
  ifdef NOTFLAT
	mov dx, word ptr cs:[currSBvec+0]
  else
	mov edx, dword ptr cs:[currSBvec+0]
  endif
	mov cx, word ptr cs:[currSBvec+SEGOFS]
	clc
 else
	stc ; C - call will be routed to DPMI host
 endif
	ret
	align 4
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
	@dprintf CStr("SB_Notify31: CX:EDX=%X:%X",10),cx,edx

	mov [bIntSB], bl	; activate notifications

	pop ds
 ifdef NOTFLAT
	assume ds:DGROUP
 endif
	retn
endif
	align 4
_SB_Notify31 endp

;--- init the SB interrupt handling.

_SB_InstallISR proc public uses ebx interrupt:byte, pDosHeap:ptr, dwPart:dword

	@dprintf CStr("SB_InstallISR enter",10)
if WATCHSB
	mov bl, interrupt
	mov ax, 204h
	int 31h
	call savevector
 ife HOSTRT
	call saveorgvec
	mov ecx, cs
	mov edx, offset realsbisr
	mov ax, 205h
	int 31h
  if RTIVT
	mov ax, 200h
	int 31h
	mov word ptr [dwOldIV+0], dx
	mov word ptr [dwOldIV+2], cx
  endif
 endif
endif

;--- if SB irq == 7, check if the real-mode interrupt is a plain IRET
;--- if yes, install another routine that sends EOI to PIC.

	mov bl, interrupt
	cmp bl, 8+7
	jnz exit
	mov ax, 200h
	int 31h
	mov eax, ecx
	shl eax, 16
	mov ax, dx
	movzx ecx, cx
	movzx edx, dx
	shl ecx, 4
	add edx, ecx
if @Model ne 7
	sub edx, [DSBase]
endif
	cmp byte ptr [edx], 0CFh
	jnz exit
	mov [dwOldIV0F], eax
	invoke copyrmcode, pDosHeap, dwPart
	mov edx, pDosHeap
if @Model ne 7
	add edx, [DSBase]
endif
	mov ecx, _my_psp()
	sub edx, ecx
	shr ecx, 4
	mov ax, 201h
	int 31h
exit:
	mov eax, 1
	ret
ife HOSTRT
saveorgvec:
 ifdef NOTFLAT
	push ds
	mov ds, cs:[_csalias]
	assume ds:_TEXT
 endif
	mov dword ptr [oldSBvec+0], edx
	mov word ptr [oldSBvec+SEGOFS], cx
 ifdef NOTFLAT
	pop ds
	assume ds:DGROUP
 endif
	retn
endif
	align 4
_SB_InstallISR endp

_SB_UninstallISR proc public uses ebx interrupt:byte

	mov bl, interrupt
ife HOSTRT
	mov edx, dword ptr cs:[oldSBvec]
	mov cx, word ptr cs:[oldSBvec+SEGOFS]
	mov ax, 205h
	int 31h
endif
	mov eax, [dwOldIV0F]
	and eax, eax
	jz exit
	push eax
	pop dx
	pop cx
	mov ax, 201h
	int 31h
exit:
	mov eax, 1
	ret

_SB_UninstallISR endp

	END
