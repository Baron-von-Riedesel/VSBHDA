
;--- get SB Live/Audigy register contents.

	.286
	.MODEL small
	option casemap:none
	.dosseg
	.stack 2048
	.386

L_FXGPREGBASE   equ 100h
L_FXGPREGSIZE   equ 256
L_MICROCODEBASE equ 400h
L_MICROCODESIZE equ 512 

A_TANKMEMCTLREGBASE equ 100h
A_TANKMEMCTLSIZE    equ 1*256
A_FXGPREGBASE   equ 400h
A_FXGPREGSIZE   equ 512
A_MICROCODEBASE equ 600h
A_MICROCODESIZE equ 1024

TANKMEMREGBASE equ 200h
TANKMEMSIZE    equ 2*256

CStr macro text:vararg
local sym
	.const
sym db text,0
	.code
	exitm <offset sym>
endm

DStr macro text:vararg
local sym
	.const
sym db text,0
	.data
	exitm <offset sym>
endm

	.const

sPtr       db "PTR",0
sData      db "DATA",0
sIPR       db "IPR",0
sIntEnable db "INTEnable",0
sWC        db "WC",0
sHCFG      db "HCFG",0
sMPUData   db "MPUData",0
sMPUCmd    db "MPUCmd",0
sIOCFG     db "IOCFG",0
sTimer     db "Timer",0
sAC97Data  db "AC97Data",0
sAC97Addr  db "AC97Addr",0
sPtr2      db "PTR2",0
sData2     db "DATA2",0
sIPR2      db "IPR2",0
sP16VIntEnable db "P16VINTEnable",0
sUnknown   db "???",0
sHCFG2     db "HCFG2",0
sIPR3      db "IPR3",0
sINTE3     db "INTE3",0

regsizesLive label byte    
	db 4,4,4,4,4,4,1,1,2,2,1,0
regsizesAudigy label byte    
	db 4,4,4,4,4,4,2,2,2,1,1
	db 4,4,4,4,4,4,4,4,0

	align 2

regnamesLive label word
	dw offset sPtr
	dw offset sData
	dw offset sIPR
	dw offset sIntEnable
	dw offset sWC
	dw offset sHCFG
	dw offset sMPUData
	dw offset sMPUCmd
	dw offset sTimer
	dw offset sAC97Data
	dw offset sAC97Addr
regnamesAudigy label word
	dw offset sPtr
	dw offset sData
	dw offset sIPR
	dw offset sIntEnable
	dw offset sWC
	dw offset sHCFG
	dw offset sIOCFG
	dw offset sTimer
	dw offset sAC97Data
	dw offset sAC97Addr
	dw 0
	dw offset sPtr2
	dw offset sData2
	dw offset sIPR2
	dw offset sP16VIntEnable
	dw offset sUnknown
	dw offset sHCFG2
	dw offset sIPR3
	dw offset sINTE3

	.data

liveregs label word
	dw DStr(10,"FX GPRs",10), L_FXGPREGBASE, L_FXGPREGSIZE
	dw DStr(10,"TankMemDataRegs TankMemAddrRegs",10), TANKMEMREGBASE, TANKMEMSIZE
	dw DStr(10,"Microcode",10), L_MICROCODEBASE, L_MICROCODESIZE
	dw 0

audigyregs label word
	dw DStr(10,"Audigy only: TankMemCtlRegs",10), A_TANKMEMCTLREGBASE, A_TANKMEMCTLSIZE
	dw DStr(10,"TankMemDataRegs TankMemAddrRegs",10), TANKMEMREGBASE, TANKMEMSIZE
	dw DStr(10,"FX GPRs",10), A_FXGPREGBASE, L_FXGPREGSIZE
	dw DStr(10,"Microcode",10), A_MICROCODEBASE, A_MICROCODESIZE
	dw 0

	.code

	include printf.inc

;--- check for vendor Creative, devices SB Live or SB Audigy
;--- BX holds bus/device/function

checkvendor proc
	mov di,0		;get vendor ID
	mov ax,0B109h
	int 1Ah
	jc err2
	cmp cx,1102h	;creative Labs?
	jnz err1
	mov di,2		;get device ID
	mov ax,0B109h
	int 1Ah
	jc err2
	cmp cx,2
	jz is_live
	cmp cx,4
	jz is_audigy
	mov dx,CStr("device ID %X, card ignored",10)
errx:
	call dispbdf
	invoke printf, dx, cx
	stc
	ret
err1:
	mov dx,CStr("vendor ID %X, card ignored",10)
	jmp errx
err2:
	mov dx,CStr("PCI config space access error",10)
	jmp errx
is_live:
is_audigy:
	push cx
	mov ax, CStr("SB Live")
	cmp cx, 2
	jz @F
	mov ax, CStr("SB Audigy")
@@:
	call dispbdf
	invoke printf, CStr("%s found",10), ax
	pop cx
	clc
	ret
dispbdf:
	pusha
	movzx ax,bh
	movzx cx,bl
	and cl,7
	shr bl,3
	mov bh,0
	invoke printf, CStr("bus/device/func %u/%u/%u: "), ax, bx, cx
	popa
	retn
checkvendor endp

;--- check PCI for SB Live! or Audigy

getpci proc
	mov ax,0b101h
	int 1ah
	jc err1
	cmp ah,0
	jnz err1
	cmp edx," ICP"
	jnz err1
	xor si,si
nextcard:
	mov ecx,40100h	;multimedia controller, audio
	mov ax,0B103h
	int 1ah
	cmp ah,00
	jnz err2
	call checkvendor
	jnc found
	inc si
	jmp nextcard
found:
	ret
err1:
	invoke printf, CStr("No PCI 2.0c BIOS found",10)
	stc
	ret
err2:
	invoke printf, CStr("No SB Live/Audigy found",10)
	stc
	ret
getpci endp

main proc c

local wModel:word
local wBase:word
local wOfs:word
local dwSubSys:dword

	call getpci
	jc exit
	mov wModel,cx

;--- bx holds bus/device/func

	mov ax,0b10Ah
	mov di,8h		;revision/class code (3 bytes)
	int 1Ah
	cmp ah,0
	jnz err1
	mov ch,0
	invoke printf, CStr("Revision: %X",10),cx

	mov ax,0b10Ah
	mov di,2Ch		;subsys vendor ID/device ID
	int 1Ah
	cmp ah,0
	jnz err1
	mov dwSubSys,ecx
	invoke printf, CStr("SubSystem vendor/device: %X/%X",10),word ptr dwSubSys+0, word ptr dwSubSys+2

	mov ax,0b10Ah
	mov di,10h		;base address 0
	int 1Ah
	cmp ah,0
	jnz err1
	test cx,1
	jz err2
	and cx,0fffch
	mov wBase,cx
	invoke printf, CStr("I/O Base: %X",10),wBase

	mov si,offset regsizesLive
	mov di,offset regnamesLive
	cmp wModel,2
	jz @F
	mov si,offset regsizesAudigy
	mov di,offset regnamesAudigy
@@:
	mov dx,wBase
nextreg:
	lodsb
	cmp al,0
	jz regsdone
	mov ah,0
	mov bx,ax
	mov cx,[di]
	jcxz @F
	.if al==1
		in al,dx
		movzx eax,al
	.elseif al==2
		in ax,dx
		movzx eax,ax
	.else
		in eax,dx
	.endif
	push dx
	invoke printf, CStr("%X (%s): %lX",10), dx, cx, eax
	pop dx
@@:
	add dx,bx
	add di,2
	jmp nextreg
regsdone:

	mov wOfs,0

;--- display the 256 Emu10kx regs

	invoke printf, CStr(10,"PTR1 registers",10)
	mov si,0
	lea di,[si+256]
	call regsout

;--- display the FX, TankMem, Microcode regs

	mov bx, offset liveregs
	cmp wModel, 2
	jz @F
	mov bx, offset audigyregs
@@:
	.while word ptr [bx]
		invoke printf, CStr("%s"), word ptr [bx+0]
		mov si, [bx+2]
		mov di, [bx+4]
		add di, si
		push bx
		call regsout
		pop bx
		add bx, 3*2
	.endw

;--- Audigy: display the 128 Emu10k2 regs

	cmp wModel,2
	jz noptr2
	mov wOfs,20h
	invoke printf, CStr(10,"PTR2 registers",10)
	mov si,0
	lea di,[si+128]
	call regsout
noptr2:

exit:
	ret
err2:
	invoke printf, CStr("no I/O base address",10)
	ret
err1:
	invoke printf, CStr("PCI config space access error",10)
	ret

regsout:
nextptrreg:
	test si,7
	jnz @F
	invoke printf, CStr(10,"%4X:"), si
@@:
	movzx eax,si
	shl eax,16
	mov dx,wBase
	add dx,wOfs
	out dx,eax
	add dx,4
	in eax,dx
	invoke printf, CStr(" %8lX"), eax
	inc si
	cmp si,di
	jb nextptrreg
	invoke printf, CStr(10)
	retn

main endp

start:
	mov ax, @data
	mov ds, ax
	mov bx, ss
	sub bx, ax
	shl bx, 4
	mov ss, ax
	add sp, bx
	call main
	mov ax,4c00h
	int 21h

	END start
