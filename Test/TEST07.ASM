
;--- play 8bit single-cycle, highspeed ( DSP cmd 0x91 )
;--- emits a zigzag wave; DMA buffer size is 512.

	.286
	.MODEL tiny
	.dosseg
	.STACK 400h
	option casemap:none

;--- SoundBlaster constants

BASEADDR           EQU 0220h       ;SoundBlaster base address
SBIRQ              EQU 5           ;SoundBlaster IRQ
DMAchannel         EQU 1           ;SoundBlaster DMA channel

	include DMA.INC
	include SB.INC

;--- PIC MASKS FOR MASK/DEMASK IRQ
PICORMASK          EQU 1 shl SBIRQ
PICANDMASK         EQU 0FFh xor PICORMASK

;--- DMA CONTROLLER REGISTERS
WRITEMASK          EQU 00ah         ; Mask register
WRITEMODE          EQU 00bh         ; Mode register
CLEARFLIPFLOP      EQU 00ch
PAGE_CHN           EQU DMAPageReg(DMAchannel) ;Page register for DMAchannel
BASE_CHN           EQU DMABaseReg(DMAchannel) ;Base address register
COUNT_CHN          EQU DMACntReg(DMAchannel)  ;Count register

;--- DMA MODE ( singlemode, nonautoinit, readmode )
WANTEDMODE         EQU DMA_MODE_SINGLE or DMA_MODE_AUTOINIT or DMA_MODE_READ

CStr macro text:vararg
local sym
	.const
sym db text,0
	.code
	exitm <offset sym>
endm

	.const

CreateData macro
SMPVAL = 80h
	repeat 64	  ;80-FF
	db SMPVAL
SMPVAL = SMPVAL + 2
	endm
SMPVAL = 0FEh
	repeat 128    ;FF-00
	db SMPVAL
SMPVAL = SMPVAL - 2
	endm
SMPVAL = 0
	repeat 64     ;00-7F
	db SMPVAL
SMPVAL = SMPVAL + 2
	endm
endm

SampleBuffer LABEL BYTE
	CreateData
SAMPLEBUFFERLENGTH equ $ - offset SampleBuffer
	CreateData

information label byte
	db 'TEST07 plays a 8bit mono single-cycle highspeed sound',10
	db 'Usage: TEST07 [frequency; def ~%u] [SB buffer length-1; def %u]',10
	db 0

sberror db 'No SoundBlaster at base address 220h.',13,10,'$'
rateerror db 'Invalid rate entered.',13,10,'$'
lengtherror db 'Invalid SB buffer length entered.',13,10,'$'

	.data

oldInterrupt        dd 0
dmabase             dw 0
sblength            dw 65532-1
rate                dw 11000
ready               db 0
timeconst           db 0
oldpic              db 0

	.CODE
	.386
	include PRINTF.INC
	.286

dectest proc near
	cmp al,'0'
	jc dectst1
	cmp al,'9' + 1
	jnc dectst1
	sub al,'0'
	and al,al
	ret
dectst1:
	stc
	ret
dectest endp

;--- in: bx->string
;--- out: number in AX
;--- out: bx->behind number
;--- digits in CH

getdec proc uses di
	mov ch,0
	mov di,0
nextdigit:
	mov al,[bx]
	call dectest
	jc done
	inc ch
	mov ah,0
	push ax
	mov ax,di
	mov di,10
	mul di
	mov di,ax
	pop ax
	add di,ax
	jc exit
	cmp dx,0
	stc
	jnz exit
	inc bx
	jmp nextdigit
done:
	cmp ch,1
	mov ax,di
exit:
	ret
getdec endp

main proc c argc:word, argv:ptr

	RESET_DSP

	cmp argc, 2
	jb options_done
	mov si, argv
	mov bx, [si+2]
	call getdec
	jc rate_invalid
	mov rate, ax
	cmp argc, 3
	jb options_done
	mov bx, [si+4]
	call getdec
	jc length_invalid
	mov sblength, ax

options_done:

;--- start msg
	invoke printf, addr information, rate, sblength

;--- calculate time constant

	mov ax,lowword 1000000
	mov dx,highword 1000000
	mov bx,rate
	cmp bx,100
	jb rate_invalid
	div bx
	cmp ah,00h
	jnz rate_invalid
	neg al
	mov timeconst,al

;--- enable SB SPEAKERS (for all SBs < SB16)

	mov dx,BASEADDR+00Ch			;DX = DSP Write Data or Command
	WAITWRITE
	mov al,DSP_ENABLESPEAKER
	out dx,al

;--- SETUP IRQ
	xor ax,ax
	mov es,ax
	mov si,(SBIRQ+8)*4
	mov ax,es:[si+0]
	mov word ptr [oldInterrupt+0],ax
	mov ax,es:[si+2]
	mov word ptr [oldInterrupt+2],ax
	cli
	mov ax,OFFSET SB_IRQ
	mov es:[si+0],ax				; set offset of new interrupt routine
	mov es:[si+2],cs				; set segment of new interrupt routine
	sti

;--- unmask SB IRQ
	in al, 21h
	mov oldpic, al
	and al, PICANDMASK
	out 21h, al

;------------------------------------------------
; calculate page and offset for DMAcontroller :
;
; segment*16+offset - 20bit memory location -> upper 4 bits  = page
;                                              lower 16 bits = offset
;------------------------------------------------
	mov si,offset SampleBuffer

	mov ax,ds
	rol ax,4	; * 16 - higher 4 bits in al
	mov bl,al
	and bl,00fh ; BL - higher 4 bits
	and al,0f0h ; clear higher 4bits in AL
	add si,ax	; SI = offset
	adc bl,0	; BL = page

;--- now BL:SI holds 20-bit linear address

	mov cx,SAMPLEBUFFERLENGTH-1
	mov ax, si
	add ax, cx
	jnc nooverride

;--- 2. part must be used
	mov si, ax
	inc si		; si=si+1 - start of second part
	inc bl		; second part is then on the next page
nooverride:
	mov dmabase, si

;------------------------------------------------
; Setup DMA-controller
;
; MASK DMA CHANNEL
;
	mov al,DMAchannel
	add al,4
	out WRITEMASK,al
;------------------------------------------------
; CLEAR FLIPFLOP
;
	out CLEARFLIPFLOP,al
;------------------------------------------------
; WRITE TRANSFER MODE
;
	mov al,WANTEDMODE
	or al,DMAchannel
	out WRITEMODE,al
;------------------------------------------------
; WRITE PAGE NUMBER
;
	mov al,bl
	out PAGE_CHN,al
;------------------------------------------------
; WRITE BASEADDRESS
;
	mov ax,si
	out BASE_CHN,al
	mov al,ah
	out BASE_CHN,al
;------------------------------------------------
; WRITE SAMPLELENGTH-1
;
	mov ax,SAMPLEBUFFERLENGTH-1
	out COUNT_CHN,al
	mov al,ah
	out COUNT_CHN,al
;------------------------------------------------
; DEMASK CHANNEL
;
	mov al,DMAchannel
	out WRITEMASK,al

;------------------------------------------------
; Setup SoundBlaster
;
; SET TIMECONSTANT
;
	mov dx,BASEADDR+00Ch			;DX = DSP Write Data or Command
	WAITWRITE
	mov al,DSP_SETTIMECONST
	out dx,al
	WAITWRITE
	mov al,timeconst
	out dx,al

;--- set length

	WAITWRITE
	mov al,DSP_SETSIZE
	out dx,al
	mov cx,sblength
	WAITWRITE
	mov al,cl
	out dx,al
	WAITWRITE
	mov al,ch
	out dx,al

;--- use 8bit single-cycle highspeed (DSP command 091h)

	WAITWRITE
	mov al,DSP_DMADAC8BITHS
	out dx,al

waitloop:
	cmp [ready],0					; check if IRQ happened
	jnz exit
	mov ah,01
	int 16h
	jz waitloop
	mov ah,00
	int 16h
	cmp ah,1
	jnz waitloop
exit:
	RESET_DSP

;--- restore PIC mask
	mov al,oldpic
	out 21h,al

;--- restore IRQ
	xor ax,ax
	mov es,ax
	mov si,(SBIRQ+8)*4
	cli
	mov ax,word ptr [oldInterrupt+0]
	mov es:[si+0],ax
	mov ax,word ptr [oldInterrupt+2]
	mov es:[si+2],ax
	sti

	invoke printf, CStr("DMA Buffer Length=%u",10), SAMPLEBUFFERLENGTH
	invoke printf, CStr("SB Buffer Length=%u",10), sblength
	out CLEARFLIPFLOP,al
	in al,BASE_CHN
	mov bl,al
	in al,BASE_CHN
	mov bh,al
	sub bx,dmabase

	in al,COUNT_CHN
	mov cl,al
	in al,COUNT_CHN
	mov ch,al
	invoke printf, CStr("DMA channel 1: idx/cnt=%Xh/%Xh",10), bx, cx
	invoke printf, CStr("timer constant: %u",10), timeconst
done:
	ret

RESET_ERROR:
;--- error 'no sb found'
	mov dx, offset sberror
	mov ah, 9
	int 21h
	jmp done
rate_invalid:
	mov dx, offset rateerror
	mov ah, 9
	int 21h
	jmp done
length_invalid:
	mov dx, offset lengtherror
	mov ah, 9
	int 21h
	jmp done
main endp

;--- SB IRQ

SB_IRQ proc
	push ax
	push dx
	mov dx,BASEADDR+00Eh			;DX = DSP DATA AVAILABLE (IRQ ACKNOWLEDGE)
	in al,dx
	mov cs:[ready],1
	mov al,20h
	out 20h,al
	pop dx
	pop ax
	IRET
SB_IRQ endp

	include SETARGV.INC

start:
	mov ax,cs
	mov ds,ax
	mov bx,ss
	sub bx,ax
	shl bx,004h
	mov ss,ax
	add sp,bx
	call _setargv
	invoke main, [_argc], [_argv]
	mov ax,4c00h
	int 21h

	END start
