
;--- save/load SB Live/Audigy register contents.

	.286
	.MODEL small
	option casemap:none
	option proc:private
	.dosseg
	.stack 2048
	.386

M_SAVE equ 0
M_LOAD equ 1

INC00FF1 equ 0	;1=save/load ptr1 regs 0-ff
INC007F2 equ 0	;1=save/load ptr2 regs 0-7f

ifndef MODE
MODE equ M_SAVE
endif

?32BIT equ 1	;run as 32-bit client
?LOADSERVER equ 0	;don't load hdpmi

L_FXGPREGBASE   equ 100h
L_FXGPREGSIZE   equ 256
L_MICROCODEBASE equ 400h
L_MICROCODESIZE equ 512 

A_TANKMEMCTLREGBASE equ 100h
A_TANKMEMCTLSIZE    equ 256
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

	.data?

wBase  dw ?
wOfs   dw ?
wModel dw ?
pData  dw ?
buffer db 8000h dup (?)

	.const
regsizesLive label byte    
	db 4,4,4,4,4,4,1,1,2,2,1,0
regsizesAudigy label byte    
	db 4,4,4,4,4,4,2,2,2,1,1
	db 4,4,4,4,4,4,4,4,0

	.data

liveregs label word
	dw DStr(10,"FX GPRs",10), L_FXGPREGBASE, L_FXGPREGSIZE
	dw DStr(10,"TankMemDataRegs TankMemAddrRegs",10), TANKMEMREGBASE, TANKMEMSIZE
	dw DStr(10,"Microcode",10), L_MICROCODEBASE, L_MICROCODESIZE
	dw 0

audigyregs label word
	dw DStr(10,"Audigy only: TankMemCtlRegs",10), A_TANKMEMCTLREGBASE, A_TANKMEMCTLSIZE
	dw DStr(10,"TankMemDataRegs TankMemAddrRegs",10), TANKMEMREGBASE, TANKMEMSIZE
	dw DStr(10,"FX GPRs",10), A_FXGPREGBASE, A_FXGPREGSIZE
	dw DStr(10,"Microcode",10), A_MICROCODEBASE, A_MICROCODESIZE
	dw 0

_TEXT32 segment dword use32 public 'CODE'

;--- PL0 32-bit proc
;--- in: CS=FLAT, DS=DGROUP, FS=FLAT data

rwregs proc far

if 0
	mov esi,offset regsizesLive
	cmp wModel,2
	jz @F
	mov esi,offset regsizesAudigy
@@:
	mov dx,wBase
	mov bx,pData
nextreg:
	lodsb
	cmp al,0
	jz regsdone
	mov cl,al
	mov ch,0
	.if cl==1
if MODE eq M_SAVE
		in al,dx
		mov [bx],al
else
		mov al,[bx]
		out dx, al
endif
	.elseif cl==2
if MODE eq M_SAVE
		in ax,dx
		mov [bx],ax
else
		mov ax,[bx]
		out dx, ax
endif
	.else
if MODE eq M_SAVE
		in eax,dx
		mov [bx],eax
else
		mov eax,[bx]
		out dx, eax
endif
	.endif
	add bx,cx
	add dx,cx
	add di,2
	jmp nextreg
regsdone:
	mov pData,bx
endif

	mov wOfs,0

;--- get the 256 Emu10kx regs

if INC00FF1
	mov si,0
	lea di,[si+256]
	call regsio
endif

;--- get the FX, TankMem, Microcode regs

	mov bx, offset liveregs
	cmp wModel, 2
	jz @F
	mov bx, offset audigyregs
@@:
	.while word ptr [bx]
		mov si, [bx+2]
		mov di, [bx+4]
		add di, si
		push bx
		call regsio
		pop bx
		add bx, 3*2
	.endw

;--- Audigy 2: get the 128 Emu10k2 regs

if INC007F2
	cmp wModel,2
	jz @F
	mov wOfs,20h
	mov si,0
	lea di,[si+128]
	call regsio
@@:
endif

	ret

regsio:
	mov bx,pData
nextptrreg:
	movzx eax,si
	shl eax,16
	mov dx,wBase
	add dx,wOfs
	out dx,eax
	add dx,4
if MODE eq M_SAVE
	in eax,dx
	mov [bx],eax
else
	mov eax,[bx]
	out dx, eax
endif
	add bx,4
	inc si
	cmp si,di
	jb nextptrreg
	mov pData,bx
	retn

rwregs endp

_TEXT32 ends

;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
	.CODE

	include printf.inc
	include dpmihlp.inc

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

local wSel32: word
local dwSubSys: dword
local rc: dword

	call getpci
	jc exit
	mov wModel,cx
	mov ax, CStr("SB Live")
	cmp cx, 2
	jz @F
	mov ax, CStr("SB Audigy")
@@:
	invoke printf, CStr("%s found",10), ax

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

if MODE eq M_LOAD
	mov edx, CStr("EMU10K.DAT")
	xor cx, cx
	mov ax, 3D00h
	int 21h
	jc err3a
	mov bx, ax
	mov edx, offset buffer
	mov ecx, sizeof buffer
	mov ax, 3F00h
	int 21h
	jc err4a
	mov ah, 3Eh
	int 21h
endif

	mov pData,offset buffer

	invoke GetDesc32, _TEXT32
	and ax, ax
	jz err5
	mov wSel32,ax
	mov bx,ax
	add ax,8
	mov fs,ax
	mov edx, offset rwregs
	invoke CallRing0Proc, bx::edx, addr rc
	push ax
	invoke FreeDesc32, wSel32
	pop ax
	and ax, ax
	jz err6

if MODE eq M_SAVE
	mov edx, CStr("EMU10K.DAT")
	xor cx, cx
	mov ax, 3C00h
	int 21h
	jc err3b
	mov bx, ax
	mov edx, offset buffer
	movzx ecx, pData
	sub ecx, edx
	mov ax, 4000h
	int 21h
	jc err4b
	mov ah, 3Eh
	int 21h
endif

exit:
	ret
err2:
	invoke printf, CStr("no I/O base address",10)
	jmp exit
err1:
	invoke printf, CStr("PCI config space access error",10)
	jmp exit
err3a:
	invoke printf, CStr("open file failed",10)
	ret
err3b:
	invoke printf, CStr("create file failed",10)
	ret
err4a:
	invoke printf, CStr("read file failed",10)
	ret
err4b:
	invoke printf, CStr("write file failed",10)
	ret
err5:
	invoke printf, CStr("out of descriptors",10)
	ret
err6:
	invoke printf, CStr("calling PL0 proc failed",10)
	ret

main endp

	include initpm16.inc

start:
	mov ax, @data
	mov ds, ax
	mov bx, ss
	sub bx, ax
	shl bx, 4
	mov ss, ax
	add sp, bx
	mov bx, sp
	shr bx, 4
	mov cx, es
	sub ax, cx
	add bx, ax
	mov ah, 4Ah
	int 21h
	movzx ebp,bp
	movzx esi,si
	call initpm
	call main
	mov ax,4c00h
	int 21h

	END start
