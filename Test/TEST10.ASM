
;--- Play 8bit DMA (DSP cmd 0x90) in stereo on SB Pro.

	.286
	.model small
	.stack 400h
	.dosseg
	option casemap:none

;--- default values

BASEADDR           EQU 220h        ;SoundBlaster base address
SBIRQ              EQU 5           ;SoundBlaster IRQ
DMACHANNEL         EQU 1           ;SoundBlaster DMA channel

	include DMA.INC
	include SB.INC

;--- DMA registers
WRITEMASK          EQU LDMABASE + DMAMASKREG
WRITEMODE          EQU LDMABASE + DMAMODEREG
CLEARFLIPFLOP      EQU LDMABASE + DMAFLIPFLOP

;--- DMA mode
WANTEDMODE         EQU 01011000b    ; singlemode, autoinit, readmode

SBLENGTH equ 04000h

WriteMixer macro index, value
	mov dx,baseaddr
	add dx,4
	mov al, index
	out dx,al
	inc dx
	call wait100mics
	mov al, value
	out dx,al
endm

NODMAMACROS equ 1

	.data

OLDInterrupt    dd ?
rate            dw 22050
irqcnt          dw 0
baseaddr        dw BASEADDR
sbirq           dw SBIRQ
dmachn          dw DMACHANNEL
oldpic          db ?
bPart           db 0
timeconst       db ?

	.const

CreateData macro
SMPVAL = 80h
	repeat 64	  ;80-FF
	dw SMPVAL or (80h shl 8)
SMPVAL = SMPVAL + 2
	endm
SMPVAL = 0FEh
	repeat 128    ;FF-00
	dw SMPVAL or (80h shl 8)
SMPVAL = SMPVAL - 2
	endm
SMPVAL = 0
	repeat 64     ;00-7F
	dw SMPVAL or (80h shl 8) 
SMPVAL = SMPVAL + 2
	endm
endm

SampleBuffer LABEL BYTE
	CreateData
SAMPLEBUFFERLENGTH equ $ - offset SampleBuffer
	CreateData

information label byte
	db 'TEST10 plays a zigzag sound using DMA highspeed mode, stereo',13,10
	db 'Pause playing with key "p".',13,10
	db 'Stop playing with <ESC>.',13,10
	db '$'

sberror   db 'No SoundBlaster at base address 220h.',13,10,'$'
rateerror db 'Invalid rate entered.',13,10,'$'
szBlaster db 'BLASTER',0

	.data?

buffer db 128 dup (?)

	.code

wait100mics proc
	push ax
	push cx
	in al,61h
	and al,10h
	mov ah,al
	mov cx,2
@@:
	in al,61h
	and al,10h
	cmp al,ah
	jz @B
	mov ah,al
	loop @B
	pop cx
	pop ax
	ret
wait100mics endp

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

switchdata proc uses es si di
	push ds
	pop es
	mov cx, SAMPLEBUFFERLENGTH
	mov si, offset SampleBuffer
	cmp bPart, 0
	jz @F
	add si, cx
@@:
	cld
	mov di, si
	shr cx, 1
@@:
	lodsw
	xchg al,ah
	stosw
	loop @B
	ret
switchdata endp

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

	include GETENV.INC
	include SCANVAR.INC

main proc c argc:word, argv:ptr

	mov dx,offset information
	mov ah,9
	int 21h

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

options_done:

;--- calculate time constant
;--- tc = 256 - (1.000.000 / ( rate * channel ))
;--- or: tc = 256 - ( 500.000 / rate )

	mov ax,lowword (1000000 shr 1)
	mov dx,highword (1000000 shr 1)
	mov bx,rate
	cmp bx,8
	jb rate_invalid
	div bx
	cmp ah,00h
	jnz rate_invalid
	neg al
	mov timeconst,al

	RESET_DSP baseaddr

	mov dx,baseaddr
	add dx,0Ch
	WAITWRITE
	mov al,DSP_ENABLESPEAKER
	out dx,al

	xor ax,ax
	mov es,ax
	mov si,sbirq
	add si,8
	shl si,2

	mov ax,es:[si+0]
	mov word ptr [OLDInterrupt+0],ax
	mov ax,es:[si+2]
	mov word ptr [OLDInterrupt+2],ax
	cli
	mov word ptr es:[si+0],offset OWN_IRQ
	mov es:[si+2],cs
	sti

	in  al,21h
	mov oldpic,al
	mov cx,sbirq
	mov ah,1
	shl ah,cl
	xor ah,-1
	and al,ah
	out 021h,al

	mov si,offset SampleBuffer
	mov cx,SAMPLEBUFFERLENGTH-1

;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; calculate page and offset for DMAcontroller
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
	mov ax,ds
	rol ax,4				; * 16 - higher 4 bits in al
	mov bl,al
	and bl,00fh 			; BL - higher 4 bits
	and al,0f0h 			; clear higher 4bits in AL
	add si,ax				; SI = offset
	adc bl,0				; BL = page
;---
; check for DMApage override
;---
	neg si
	cmp si,cx
	ja  nooverride
	neg si
	add si,cx
	inc si
	inc bl
	neg si
	inc bPart
nooverride:
	neg si
;
; Setup DMA-controller: BL=addr 16-19, SI=addr 0-15, CX=length-1
;
	mov al,byte ptr dmachn
	and al,3
	or al,DMA_MASK_DISABLE_CHN
	out WRITEMASK,al
	out CLEARFLIPFLOP,al
	mov al,WANTEDMODE
	add al,byte ptr dmachn
	out WRITEMODE,al
	call getpageport
	mov al,bl
	out dx,al
	call getbaseport
	mov ax,si
	out dx,al
	mov al,ah
	out dx,al
	call getcntport
	mov al,cl
	out dx,al
	mov al,ch
	out dx,al
	mov al,byte ptr dmachn
	and al,3
	out WRITEMASK,al

	WriteMixer 14,2			;enable stereo
;
; Setup SoundBlaster :
;
	mov dx,baseaddr
	add dx,0Ch
	WAITWRITE
	mov al,DSP_SETTIMECONST
	out dx,al
	WAITWRITE
	mov al,timeconst
	out dx,al

	WAITWRITE
	mov al,DSP_SETSIZE
	out dx,al
	mov cx,SBLENGTH-1
	WAITWRITE
	mov al,cl
	out dx,al
	WAITWRITE
	mov al,ch
	out dx,al
	WAITWRITE
	mov al,DSP_DMADAC8BITHSAUTO
	out dx,al

	mov irqcnt, 0

waitloop:
	cmp irqcnt, 5
	jnz @F
	call switchdata
	mov irqcnt, 0
@@:
	mov ah,01
	int 16h
	jz  waitloop
	xor ah,ah
	int 16h
	cmp al,'p'
	je  do_pause
	cmp al,27
	jne waitloop
	jmp exit
do_pause:
	mov dx,baseaddr
	add dx,0Ch
	WAITWRITE
	mov al,DSP_PAUSE8BIT
	out dx,al
	xor ah,ah
	int 16h
	mov dx,baseaddr
	add dx,0Ch
	WAITWRITE
	mov al,DSP_CONTINUE8BIT
	out dx,al
	jmp waitloop

exit:
	RESET_DSP baseaddr

;	WriteMixer 0,0		; reset mixer
	WriteMixer 14,0		; reset stereo bit in SB Pro mixer

	mov al,oldpic
	out 021h,al

	xor ax,ax
	mov es,ax
	mov si,sbirq
	add si,8
	shl si,2
	cli
	mov ax,word ptr [OLDInterrupt+0]
	mov es:[si+0],ax
	mov ax,word ptr [OLDInterrupt+2]
	mov es:[si+2],ax
	sti
done:
	ret

RESET_ERROR:
	mov dx,offset sberror
	mov ah,9
	int 21h
	jmp done
rate_invalid:
	mov dx, offset rateerror
	mov ah, 9
	int 21h
	jmp done

main endp

;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; Our own IRQ for detecting buffer half SB currently plays
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
OWN_IRQ proc
	pusha
	push ds
	mov ax,@data
	mov ds,ax
	inc [irqcnt]
	mov dx,baseaddr
	add dx,SB_DSPSTATUS
	in  al,dx
	mov al,20h
	out 20h,al
	pop ds
	popa
	IRET
OWN_IRQ endp

	include SETARGV.INC

start:
	mov ax,@data
	mov ds,ax
	mov bx,ss
	sub bx,ax
	shl bx,4
	mov ss,ax
	add sp,bx
	call _setargv
	invoke main, [_argc], [_argv]
	mov ax,4c00h
	int 21h

	END start
