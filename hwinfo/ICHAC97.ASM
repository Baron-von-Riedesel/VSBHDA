
;--- get SBLive register contents

	.286
	.MODEL small, stdcall
	option casemap:none
	.dosseg
	.stack 2048
	.386

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

	.data

names label word
	dw DStr("Reset")
	dw DStr("Master Vol")
	dw DStr("AUX Out Vol")
	dw DStr("Mono Vol")
	dw DStr("Master Tone")
	dw DStr("PC Beep Vol")
	dw DStr("Phone Vol")
	dw DStr("Mic Vol")
	dw DStr("Line-In Vol")
	dw DStr("CD Vol")
	dw DStr("Video Vol")
	dw DStr("AUX In Vol")
	dw DStr("PCM Out Vol")
	dw DStr("Record Select")
	dw DStr("Record Gain")
	dw DStr("Record Gain Mic")
	dw DStr("General Purpose")
	dw DStr("3D Control")
	dw DStr("Audio Interrupt & Paging")
	dw DStr("Power Down Ctrl/Stat")
endnames equ $

namesExt label word
	dw DStr("Extended Audio ID")
	dw DStr("Extended Audio Stat/Ctrl")
	dw DStr("PCM Front DAC Rate")
	dw DStr("PCM Surround DAC Rate")
	dw DStr("PCM LFE DAC Rate")
	dw DStr("PCM L/R ADC Rate")
	dw DStr("Mic ADC Rate")
	dw DStr("Center/LFE Vol")
	dw DStr("Surround Vol")
	dw DStr("S/PDIF Ctrl")
endnamesExt equ $

namesExtPg0 label word
	dw DStr("Codec Class/Rev")
	dw DStr("PCI SVID")
	dw DStr("PCI SID")
	dw DStr("Function Select")
	dw DStr("Function Information")
	dw DStr("Sense Details")
	dw DStr("DAC Slot Mapping")
	dw DStr("ADC Slot Mapping")
endnamesExtPg0 equ $

	.data?

buffer dw 128 dup (?)

;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
	.code

	include printf.inc

;--- scan PCI for Multimedia Audio Controller with vendor == Intel

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
	jc err2
	cmp ah,00
	jnz err2
	mov di,0		;get vendor ID
	mov ax,0B109h
	int 1Ah
	cmp ah,0
	jnz err2
	cmp cx,8086h	;Intel?
	jz found
	movzx ax,bh
	movzx bx,bl
	invoke printf, CStr("bus/device %X/%X card has vendor ID %X - ignored",10), ax, bx, cx
	inc si
	jmp nextcard
found:
	ret
err1:
	invoke printf, CStr("No PCI 2.0c BIOS found",10)
	stc
	ret
err2:
	invoke printf, CStr("No PCI Audio Controllers (4/1/0) with vendor Intel (8086) found",10)
	stc
	ret
getpci endp

;--- display registers of a DMA device

bmitem proc wBaseCtrl:word

	mov dx,wBaseCtrl
	add dx,si
	in eax,dx
	invoke printf, CStr("%02X %s: %lX",10),si,CStr("Buffer Descriptor Base Address"),eax
	add si,4

	mov dx,wBaseCtrl
	add dx,si
	in eax,dx
	push eax
	;current index value
	mov ah,0
	invoke printf, CStr("%02X %s: %X",10),si,CStr("CIV - Current Index Value"),ax
	pop eax
	;current last valid index
	push eax
	movzx ax,ah
	add si,1
	invoke printf, CStr("%02X %s: %X",10),si,CStr("LVI - Last Valid Index"),ax
	pop eax
	shr eax,16
	add si,1
	invoke printf, CStr("%02X %s: %X",10),si,CStr("Status"),ax
	add si,2

	mov dx,wBaseCtrl
	add dx,si
	in eax,dx
	push eax
	invoke printf, CStr("%02X %s: %X",10),si,CStr("PICB - Position In Current Buffer"),ax
	;Prefetched index value
	pop eax
	shr eax,16
	push ax
	mov ah,0
	add si,2
	invoke printf, CStr("%02X %s: %X",10),si,CStr("PIV - Prefetched Index Value"),ax
	pop ax
	shr ax,8
	add si,1
	invoke printf, CStr("%02X %s: %X",10),si,CStr("Control"),ax
	ret
bmitem endp

main proc c

local wBaseCodec:word
local wBaseCtrl:word
local dwSubSys:dword

	call getpci
	jc exit

	invoke printf, CStr("PCI Config Space:",10)

;--- bx holds bus/device/func

	mov di,2		;device ID
	mov ax,0B109h
	int 1Ah
	cmp ah,0
	jnz err1
	invoke printf, CStr("02 Device ID: %X",10),cx

	mov di,4h		;command
	mov ax,0b109h
	int 1Ah
	cmp ah,0
	jnz err1
	mov si,cx
	invoke printf, CStr("04 Command: %X",10),cx
	mov ax,si
	and ax,1+2+4
	cmp ax,1+2+4
	jz @F
	invoke printf, CStr("I/O, Memory or BusMaster access are disabled, trying to enable",10)
	mov cx,si
	or cx,1+2+4		;enable I/O, Memory & busmaster access
	mov ax,0b10Ch	;write word
	int 1Ah
	cmp ah,0
	jnz err1
@@:

	mov ax,0b10Ah
	mov di,8h		;revision/class code (3 bytes)
	int 1Ah
	cmp ah,0
	jnz err1
	mov ch,0
	invoke printf, CStr("08 Revision: %X",10),cx

	mov ax,0b10Ah
	mov di,10h		;base address 0
	int 1Ah
	cmp ah,0
	jnz err1
	test cx,1
	jz err2
	and cx,0fffch
	mov wBaseCodec,cx
	invoke printf, CStr("10 NAMBAR (Mixer I/O Base): %X",10),wBaseCodec

	mov ax,0b10Ah
	mov di,14h		;base address 1
	int 1Ah
	cmp ah,0
	jnz err1
	test cx,1
	jz err2
	and cx,0fffch
	mov wBaseCtrl,cx
	invoke printf, CStr("14 NABMBAR (Bus Master I/O Base): %X",10),wBaseCtrl

	mov ax,0b10Ah
	mov di,18h		;mem address 1
	int 1Ah
	cmp ah,0
	jnz err1
	invoke printf, CStr("18 MMBAR (Mixer Memory Base): %lX",10),ecx
	mov ax,0b10Ah
	mov di,1Ch		;mem address 2
	int 1Ah
	cmp ah,0
	jnz err1
	invoke printf, CStr("1C MBBAR (Bus Master Memory Base): %lX",10),ecx

	mov ax,0b10Ah
	mov di,2Ch		;subsys vendor ID/device ID
	int 1Ah
	cmp ah,0
	jnz err1
	mov dwSubSys,ecx
	invoke printf, CStr("2C SubSystem vendor/device: %X/%X",10),word ptr dwSubSys+0, word ptr dwSubSys+2

	mov ax,0b108h
	mov di,3Ch		;interrupt
	int 1Ah
	cmp ah,0
	jnz err1
	mov ch,0
	invoke printf, CStr("3C Interrupt: %X",10),cx

	mov ax,0b108h
	mov di,40h		;byte at 40h
	int 1Ah
	cmp ah,0
	jnz err1
	mov ch,0
	invoke printf, CStr("40 PCID: %X",10),cx

	mov ax,0b108h
	mov di,41h		;byte at 41h
	int 1Ah
	cmp ah,0
	jnz err1
	mov ch,0
	invoke printf, CStr("41 CFG: %X",10),cx

	invoke printf, CStr(10,"Mixer",10)
	invoke printf, CStr(10,"Base Regs:",10)
	push ds
	pop es
	mov si,0
	mov bx,offset names
	mov di,offset buffer
	cld
nextregBase:
	mov dx,wBaseCodec
	add dx,si
	in ax,dx
	stosw
	mov dx,[bx]
	invoke printf, CStr("%2X %s: %X",10),si,dx,ax
	add si,2
	add bx,2
	cmp bx,offset endnames
	jb nextregBase

	invoke printf, CStr(10,"Extended Regs:",10)
	mov si,28h
	mov bx,offset namesExt
	mov cx,1111100b
nextregExt:
	mov dx,wBaseCodec
	add dx,si
	in ax,dx
	stosw
	mov dx,[bx]
	shr cx,1
	push cx
	.if CARRY?
		invoke printf, CStr("%2X %s: %X (dec=%u)",10),si,dx,ax,ax
	.else
		invoke printf, CStr("%2X %s: %X",10),si,dx,ax
	.endif
	pop cx
	add si,2
	add bx,2
	cmp bx,offset endnamesExt
	jb nextregExt

	mov ax,[buffer+28h]	;extended audio ID
	shr ax,10
	and ax,3
	cmp ax,2		;rev. 2.3?
	jnz exit

	mov ax,[buffer+24h]	;get audio interrupt & paging
	and al,0F0h
	or al,1			;select page 1
	mov dx,wBaseCodec
	add dx,24h
	out dx,ax

	invoke printf, CStr(10,"Extended Regs Page 1:",10)
	mov si,60h
	mov bx,offset namesExtPg0
	lea di,[si+offset buffer]
nextregExtPg0:
	mov dx,wBaseCodec
	add dx,si
	in ax,dx
	stosw
	mov dx,[bx]
	invoke printf, CStr("%2X %s: %X",10),si,dx,ax
	add si,2
	add bx,2
	cmp bx,offset endnamesExtPg0
	jb nextregExtPg0

;--- DMA controller registers

	invoke printf, CStr(10,"DMA Controller",10)

	invoke printf, CStr(10,"PCM Out:",10)
	mov si,10h		;PCM out offset
	invoke bmitem, wBaseCtrl
	invoke printf, CStr(10,"S/PDIF Out:",10)
	mov si,60h		;S/PDIF out offset
	invoke bmitem, wBaseCtrl

	mov dx,wBaseCtrl
	add dx,2ch
	in eax,dx
	invoke printf, CStr(10,"2C Global Control: %lX",10),eax
	mov dx,wBaseCtrl
	add dx,30h
	in eax,dx
	invoke printf, CStr("30 Global Status: %lX",10),eax

exit:
	ret
err2:
	invoke printf, CStr("no I/O base address (%X)",10), di
	ret
err1:
	invoke printf, CStr("PCI config space access error (%X)",10), di
	ret
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
