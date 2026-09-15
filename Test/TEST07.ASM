
;--- play 8bit single-cycle, highspeed ( DSP cmd 0x91 )
;--- emits a zigzag wave; DMA buffer size is 512.

	.286
	.MODEL tiny
	.dosseg
	.STACK 400h
	option casemap:none

;--- default values

BASEADDR           EQU 220h        ; base address
SBIRQ              EQU 5           ; IRQ
DMACHANNEL         EQU 1           ; low DMA channel

	include DMA.INC
	include SB.INC

;--- DMA registers
WRITEMASK          EQU LDMABASE + DMAMASKREG
WRITEMODE          EQU LDMABASE + DMAMODEREG
CLEARFLIPFLOP      EQU LDMABASE + DMAFLIPFLOP

;--- DMA mode
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

sberror db 'No SoundBlaster at base address %X.',10,0
rateerror db 'Invalid rate entered.',13,10,'$'
lengtherror db 'Invalid SB buffer length entered.',13,10,'$'
szBlaster db 'BLASTER',0

	.data

oldInterrupt        dd 0
dmabase             dw 0
sblength            dw 65532-1
rate                dw 11000
baseaddr            dw BASEADDR  ; SB base port
sbirq               dw SBIRQ
dmachn              dw DMACHANNEL
ready               db 0
timeconst           db 0
oldpic              db 0

	.data?

buffer db 128 dup (?)

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

	include GETENV.INC
	include SCANVAR.INC

getpageport proc uses bx
	mov bx, dmachn
	and bx, 7
	mov dl, [bx+mapport]
	mov dh,0
	ret
mapport db 87h,83h,81h,82h,8Fh,8Bh,89h,8Ah
getpageport endp

getbaseport proc uses bx
	mov bx, dmachn
	and bx, 7
	mov dl, [bx+mapbase]
	mov dh,0
	ret
mapbase db 00h,02h,04h,06h,0C0h,0C4h,0C8h,0CCh
getbaseport endp

getcntport proc uses bx
	mov bx, dmachn
	and bx, 7
	mov dl, [bx+mapcnt]
	mov dh,0
	ret
mapcnt db 01h,03h,05h,07h,0C2h,0C6h,0CAh,0CEh
getcntport endp

main proc c argc:word, argv:ptr

	invoke getenv, addr szBlaster, addr buffer
	.if ax
		invoke scanvar, 'A', addr buffer, 16, addr baseaddr
		invoke scanvar, 'I', addr buffer, 10, addr sbirq
		invoke scanvar, 'D', addr buffer, 10, addr dmachn
	.endif

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

	RESET_DSP baseaddr

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

	mov dx,baseaddr
	add dx,SB_DSPWRITE
	WAITWRITE
	mov al,DSP_ENABLESPEAKER
	out dx,al

;--- SETUP IRQ
	xor ax,ax
	mov es,ax
	mov si,sbirq
	add si,8
	shl si,2
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
	mov cx,sbirq
	mov ah,1
	shl ah,cl
	xor ah,-1
	and al,ah
	out 21h,al

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
	mov al,byte ptr dmachn
	and al,11b
	or al,4
	out WRITEMASK,al
;------------------------------------------------
; CLEAR FLIPFLOP
;
	out CLEARFLIPFLOP,al
;------------------------------------------------
; WRITE TRANSFER MODE
;
	mov al,byte ptr dmachn
	and al,11b
	or al,WANTEDMODE
	out WRITEMODE,al
;------------------------------------------------
; WRITE PAGE NUMBER
;
	call getpageport
	mov al,bl
	out dx,al
;------------------------------------------------
; WRITE BASEADDRESS
;
	call getbaseport
	mov ax,si
	out dx,al
	mov al,ah
	out dx,al
;------------------------------------------------
; WRITE SAMPLELENGTH-1
;
	call getcntport
	mov ax,SAMPLEBUFFERLENGTH-1
	out dx,al
	mov al,ah
	out dx,al
;------------------------------------------------
; DEMASK CHANNEL
;
	mov al,byte ptr dmachn
	and al,11b
	out WRITEMASK,al

;------------------------------------------------
; Setup SoundBlaster
;
; SET TIMECONSTANT
;
	mov dx,baseaddr
	add dx,SB_DSPWRITE
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
	RESET_DSP baseaddr

;--- restore PIC mask
	mov al,oldpic
	out 21h,al

;--- restore IRQ
	xor ax,ax
	mov es,ax
	mov si,sbirq
	add si,8
	shl si,2
	cli
	mov ax,word ptr [oldInterrupt+0]
	mov es:[si+0],ax
	mov ax,word ptr [oldInterrupt+2]
	mov es:[si+2],ax
	sti

	invoke printf, CStr("DMA Buffer Length=%u",10), SAMPLEBUFFERLENGTH
	invoke printf, CStr("SB Buffer Length=%u",10), sblength
	out CLEARFLIPFLOP,al
	call getbaseport
	in al,dx
	mov bl,al
	in al,dx
	mov bh,al
	sub bx,dmabase

	call getcntport
	in al,dx
	mov cl,al
	in al,dx
	mov ch,al
	invoke printf, CStr("DMA channel %u: idx/cnt=%Xh/%Xh",10), dmachn, bx, cx
	invoke printf, CStr("timer constant: %u",10), timeconst
done:
	ret

RESET_ERROR:
;--- error 'no sb found'
	invoke printf, addr sberror, baseaddr
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
	mov dx,cs:baseaddr
	add dx,SB_DSPSTATUS
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
