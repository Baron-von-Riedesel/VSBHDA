
;--- display pci devices that use an irq.

	.286
	.model tiny
	.stack 4096
	.dosseg
	.386

DISPVENDOR equ 1	;1=display vendor ID/device code
DISPSSID   equ 1	;1=display subsystem vendor/version

PCIPORT equ 0cf8h

lf equ 10

CStr macro text:vararg
local sym
	.const
sym db text,0
	.code
	exitm <offset sym>
endm

@dstrng macro text
local sym
	.const
sym db text,0
	.data
	dw offset sym
endm

	.data

classes label word
	@dstrng "unclassified"
	@dstrng "mass storage controller"
	@dstrng "network controller"
	@dstrng "display controller"
	@dstrng "multimedia controller"
	@dstrng "memory controller"
	@dstrng "bridge"
	@dstrng "communication controller"
	@dstrng "generic system peripheral"
	@dstrng "input device controller"
	@dstrng "docking station"
	@dstrng "processor"
	@dstrng "serial bus controller"
	@dstrng "wireless controller"
	@dstrng "intelligent controller"
	@dstrng "satellite communication controller"
	@dstrng "encryption controller"
	@dstrng "signal processing controller"
	@dstrng "processing accelerators"
	@dstrng "non-essential instrumentation"
NUMCLASSES equ ($ - offset classes) / sizeof word

cls01 label word
	@dstrng "SCSI"
	@dstrng "IDE"
	@dstrng "Floppy"
	@dstrng "IPI Bus"
	@dstrng "RAID"
	@dstrng "ATA with ADMA"
	@dstrng "SATA"
	@dstrng "SAS"
	@dstrng "NVM"
	@dstrng "UFS"

cls03 label word
	@dstrng "VGA comp."

cls04 label word
	@dstrng "Video"
	@dstrng "Audio"
	@dstrng "Telephony"
	@dstrng "HDA"

cls06 label word
	@dstrng "Host"
	@dstrng "ISA"
	@dstrng "EISA"
	@dstrng "MCA"
	@dstrng "PCI to PCI"
	@dstrng "PCMCIA"
	@dstrng "NuBus"
	@dstrng "CardBus"
	@dstrng "RaceWAY"
	@dstrng "PCI to PCI semi-transparent"
	@dstrng "Infiniband to PCI"
	@dstrng "Advanced Switching to PCI"

cls0C label word
	@dstrng "IEEE 1394 FireWire"
	@dstrng "ACCESS"
	@dstrng "SSA"
	@dstrng "USB"
	@dstrng "Fibre Channel"
	@dstrng "SMBus"
	@dstrng "InfiniBand"
	@dstrng "IPMI"
	@dstrng "SERCOS"
	@dstrng "CANbus"
	@dstrng "MIPI"

masks label dword
	dd 0
	dd 1111111111b
	dd 0
	dd 1b
	dd 1111b
	dd 0
	dd 111111111111b
	dd 5 dup (0)
	dd 11111111111b
	dd 7 dup (0)

strings label word
	dw 0
	dw offset cls01
	dw 0
	dw offset cls03
	dw offset cls04
	dw 0
	dw offset cls06
	dw 5 dup (0)
	dw offset cls0C
	dw 7 dup (0)

	.code

	include printf.inc

getclass proc
	mov cx, CStr("unknown class")
	cmp al, NUMCLASSES
	jnc exit
	push bx
	movzx bx, al
	shl bx, 1
	mov cx, [bx][classes]
	pop bx
exit:
	ret
getclass endp

	.const

coltext label byte
	db "bus/dev/fn cls/sub/if rv "
if DISPVENDOR
	db "vend/dev  "
 if DISPSSID
	db "subsysID  "
 endif
endif
	db "INT",lf
	db "------------------------------------------------------------------------",lf
	db 0

	.code

main proc c

local dwEcx:dword
local dwEax:dword
local bIrq:byte

	mov ax,0b101h
	int 1Ah
	cmp ah,00
	jnz err1
	cmp edx," ICP"
	jnz err1

	invoke printf, CStr("PCIIRQ v1.2, displays all PCI devices that use an Interrupt.",lf)

	invoke printf, CStr("%s"), addr coltext

;--- use port 0CF8h/0CFCh to read config space 

	mov ecx, 80000008h	; read offset 8 (cls/subclass/programming interface/rev
nextport:
	mov dx, PCIPORT
	mov eax, ecx
	out dx, eax
	add dx, 4
	in eax, dx		; get data at port cfch
	mov dwEcx, ecx
	mov dwEax, eax
	cmp eax, -1
	jz skipaddr

;--- get interrupt line/pin
	mov eax, dwEcx
	mov al, 3Ch
	mov dx, PCIPORT
	out dx, eax
	add dx, 4
	in eax, dx
	.if al == 0 || al == -1
		jmp skipaddr
	.endif

	mov bIrq, al
    
	movzx cx, byte ptr dwEcx+2	; bits 16-23=bus#
	movzx dx, byte ptr dwEcx+1
	movzx bx, byte ptr dwEcx+1
	and dl, 7		; bits 10-8=function
	shr bx, 3
	and bx, 1Fh		; bits 11-15=device#

	push bp
	mov si, word ptr dwEax+2
	mov di, word ptr dwEax+2
	mov bp, word ptr dwEax+0
	mov ax, bp
	shr si, 8		; class
	shr bp, 8		; programming interface
	and di, 0ffh
	and ax, 0ffh	; revision
	invoke printf, CStr("%2X/%2X/%2X    %2X/%2X/%2X  %2X"), cx, bx, dx, si, di, bp, ax
	pop bp

if DISPVENDOR
	mov eax, dwEcx
	mov al, 0
	mov dx, PCIPORT
	out dx, eax
	add dx, 4
	in eax, dx
	movzx ecx, ax
	shr eax, 16
	invoke printf, CStr(" %4X/%4X"), cx, ax
 if DISPSSID
	mov eax, dwEcx
	mov al, 2Ch
	mov dx, PCIPORT
	out dx, eax
	add dx, 4
	in eax, dx
	movzx ecx, ax
	shr eax, 16
	mov edx, eax
	or edx, ecx
	.if ZERO?
		invoke printf, CStr("          ")
	.else
		invoke printf, CStr(" %4X/%4X"), cx, ax
	.endif
 endif
endif
	invoke printf, CStr("%4u"),bIrq

;--- display class & subclass names
	movzx esi, si
	mov eax, esi
	call getclass
	invoke printf, CStr(" %s"), cx
	.if si < NUMCLASSES
		mov eax, dwEax
		.if [esi*4][masks]
			shr eax, 8
			movzx eax, ah
			xor ecx, ecx
			bt [esi*4][masks], eax
			.if CARRY?
				mov bx, [esi*2][strings]
				add ax, ax
				add bx, ax
				mov ax, [bx]
				invoke printf, CStr(" (%s)"), ax
			.endif
		.endif
	.endif

	invoke printf, CStr(lf)

skipaddr:
	mov ecx, dwEcx
	add ecx, 100h
	cmp ecx, 81000008h
	jb nextport
	ret
err1:
	.const
err1s db "no PCI BIOS",13,lf,'$'
	.code
	mov dx,offset err1s
	mov ah,9
	int 21h
	ret

main endp

start:
	push cs		; setup a tiny memory model (CS=SS=DS=DGROUP)
	pop ds
	mov ax, ss
	mov dx, cs
	sub ax, dx
	shl ax, 4
	push cs
	pop ss
	add sp, ax
	mov bx, sp
	shr bx, 4
	add bx, 10h
	mov ah, 4Ah	; free unused memory
	int 21h
	invoke main
	mov ah,4ch
	int 21h

	END start
