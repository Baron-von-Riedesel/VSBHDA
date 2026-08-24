
;--- test MIDI synthesizer

	.286
	.model tiny, stdcall
	.stack 2048
	.dosseg
	option casemap:none

INTCB equ 1Ch

NOTEON  equ 0
NOTEOFF equ 1
DOWAIT  equ 2
PROGCHG equ 3
CTRLCHG equ 4
ENDDATA equ -1

CStr macro text:vararg
local sym
	.const
sym db text,0
	.code
	exitm <offset sym>
endm

@MpuSendCmd macro value
local wait
	mov dx, wPort
	inc dx
	mov cx, -1
wait:
	in al, dx
	test al, 40h
	loopnz wait
	mov al,value
	out dx, al
endm

@MpuSendData macro value
local wait
	mov dx, wPort
	inc dx
	mov cx, -1
wait:
	in al, dx
	test al, 40h
	loopnz wait
	dec dx
	mov al,value
	out dx, al
endm

@MpuReadData macro
local wait
	mov dx, wPort
	inc dx
	mov cx, -1
wait:
	in al, dx
	test al, 80h
	loopnz wait
	dec dx
	in al, dx
endm

@ProgChg macro channel, preset
	or channel, 0C0h
	@MpuSendData channel
	@MpuSendData preset
endm

@CtrlChg macro channel, controller, data
	or channel, 0B0h
	@MpuSendData channel
	@MpuSendData controller
	@MpuSendData data
endm

@NoteOn macro channel, note, velocity
	or channel,90h
	@MpuSendData channel
	@MpuSendData note
 ifnb <velocity>
	@MpuSendData velocity
 else
	@MpuSendData 7Fh
 endif
endm

@NoteOff macro channel, note, velocity
	or channel, 80h
	@MpuSendData channel
	@MpuSendData note
 ifnb <velocity>
	@MpuSendData velocity
 else
	@MpuSendData 0
 endif
endm

	.data

oldi1C   dd 0
wPort    dw 330h
pData    dw 0
bExit    db 0
bWait    db 0

	.const

mididata label byte
	db PROGCHG, 0, 24
	db PROGCHG, 1, 25


	db NOTEON, 0, 3ch
	db NOTEON, 0, 40h
	db NOTEON, 0, 43h
	db NOTEON, 1, 3ch
	db NOTEON, 1, 40h
	db NOTEON, 1, 43h
	db DOWAIT, 18
	db NOTEOFF, 0, 3ch
	db NOTEOFF, 0, 40h
	db NOTEOFF, 0, 43h
	db NOTEOFF, 1, 3ch
	db NOTEOFF, 1, 40h
	db NOTEOFF, 1, 43h

	db NOTEON, 0, 3Ch
	db NOTEON, 0, 41h
	db NOTEON, 0, 45h
	db NOTEON, 1, 3Ch
	db NOTEON, 1, 41h
	db NOTEON, 1, 45h
	db DOWAIT, 18
	db NOTEOFF, 0, 3Ch
	db NOTEOFF, 0, 41h
	db NOTEOFF, 0, 45h
	db NOTEOFF, 1, 3Ch
	db NOTEOFF, 1, 41h
	db NOTEOFF, 1, 45h

	db NOTEON, 0, 3Eh
	db NOTEON, 0, 43h
	db NOTEON, 0, 47h
	db NOTEON, 1, 3Eh
	db NOTEON, 1, 43h
	db NOTEON, 1, 47h
	db DOWAIT, 18
	db NOTEOFF, 0, 3Eh
	db NOTEOFF, 0, 43h
	db NOTEOFF, 0, 47h
	db NOTEOFF, 1, 3Eh
	db NOTEOFF, 1, 43h
	db NOTEOFF, 1, 47h

	db NOTEON, 0, 3Ch
	db NOTEON, 0, 40h
	db NOTEON, 0, 43h
	db NOTEON, 1, 3Ch
	db NOTEON, 1, 40h
	db NOTEON, 1, 43h
	db DOWAIT, 18
	db NOTEOFF, 0, 3Ch
	db NOTEOFF, 0, 40h
	db NOTEOFF, 0, 43h
	db NOTEOFF, 1, 3Ch
	db NOTEOFF, 1, 40h
	db NOTEOFF, 1, 43h

	db CTRLCHG, 0, 7Bh, 0
	db CTRLCHG, 1, 7Bh, 0
	db DOWAIT, 18
	db ENDDATA

	.code

oldi15 dd 0

	.386
	include printf.inc
	include getenv.inc
	.286

;--- int 1Ch; emit midi data

int1cproc proc

	push ds
	pusha
	push cs
	pop ds
	cmp bWait,0
	jz @F
	dec bWait
	jmp done
@@:
	mov si, pData
	cld
	lodsb
	.if al == NOTEON
		lodsb
		mov bh,al
		lodsb
		mov bl,al
		@NoteOn bh,bl
	.elseif al == NOTEOFF
		lodsb
		mov bh,al
		lodsb
		mov bl,al
		@NoteOff bh,bl
	.elseif al == PROGCHG
		lodsb
		mov bh,al
		lodsb
		mov bl,al
		@ProgChg bh,bl
	.elseif al == CTRLCHG
		lodsb
		mov bh,al
		lodsb
		mov bl,al
		lodsb
		mov ah,al
		@CtrlChg bh,bl,ah
	.elseif al == DOWAIT
		lodsb
		mov bWait,al
	.elseif al == ENDDATA
		mov si, offset mididata
		mov cs:[bExit],1
	.endif
	mov pData, si
done:
	popa
	pop ds
	iret

int1cproc endp

int15 proc

	cmp ah,4Fh
	jnz nokey
	cmp al,01h
	jnz noesc
	mov cs:[bExit],1
noesc:
nokey:
	stc
	jmp cs:[oldi15]
int15 endp

;--- scan BLASTER environment variable

scanvar proc uses si var:byte, pBuffer:ptr, pValue:ptr
	mov si, pBuffer
	mov bx, -1
	.while byte ptr [si]
		lodsb
		.if (al == ' ')
			mov bx,-1
			.continue
		.endif
		.if (al >= 'a') && (al <= 'z')
			sub al,20h
		.endif
		.if (bx == -1)
			.if (al == var)
				mov cl,16
				mov bx, pValue
				mov word ptr [bx], 0
			.endif
			.continue
		.endif
		.if (cl == 10)
			call chkdec
		.else
			call chkhex
		.endif
		jc numdone
		mov ah, 0
		.if (bx != -1)
			mov dx, [bx]
			.if (cl == 16)
				shl dx, 4
			.else
				push ax
				mov ax,dx
				mov ch,0
				mul cx
				pop dx
			.endif
			add dx, ax
			mov [bx],dx
		.endif
numdone:
	.endw
	ret
chkdec:
	sub al,'0'
	jc err1
	cmp al,9
	ja err1
	clc
	retn
err1:
	stc
	retn
chkhex:
	cmp al,'A'
	jb chkdec
	cmp al,'F'
	ja err1
	sub al,37h
	retn
scanvar endp

main proc c

local buffer[256]:byte

	invoke getenv, CStr("BLASTER"), addr buffer
	.if ax
		invoke scanvar, 'P', addr buffer, addr wPort
	.endif
	mov ax,3515h
	int 21h
	mov word ptr oldi15+0,bx
	mov word ptr oldi15+2,es

	mov dx,offset int15
	mov ax,2515h
	int 21h

	@MpuSendCmd 0FFh
	@MpuReadData
	.if (al != 0FEh)
		invoke printf, CStr("no mpu-401 available at 0x%X",10), wPort
		jmp exit
	.endif

;--- set uart mode
	@MpuSendCmd 03Fh
	@MpuReadData
	.if (al != 0FEh)
		invoke printf, CStr("no acknowledge from set UART mode",10)
		jmp exit
	.endif

	mov pData, offset mididata

	mov ax,3500h or INTCB
	int 21h
	mov word ptr oldi1C+0,bx
	mov word ptr oldi1C+2,es
	mov dx, offset int1cproc
	mov ax,2500h or INTCB
	int 21h

nextloop:
	mov ah,1
	int 16h
	jz @F
	mov ah,0
	int 16h
@@:
	cmp bExit,1
	jnz nextloop

	push ds
	lds dx, oldi1C
	mov ax,2500h + INTCB
	int 21h
	pop ds

	@MpuSendData 0FFh

exit:
	push ds
	lds dx, cs:oldi15
	mov ax,2515h
	int 21h
	pop ds
	mov al,0
	ret
main endp    

start:
	mov ax,cs
	mov ds,ax
	mov bx,ss
	sub bx,ax
	shl bx,4
	mov ss,ax
	add sp,bx
	call main
	mov ah, 4Ch
	int 21h
    
	end start
