
; short dma buffer, 16-bit mono data, unsigned, SB16 only

	.286
	.MODEL tiny
	.dosseg
	.stack 400h
	option casemap:none

;--- default values

BASEADDR           EQU 0220h       ; SB base address
SBIRQ              EQU 5           ; SB IRQ
DMACHANNEL         EQU 5           ; SB high DMA channel

	include DMA.INC
	include SB.INC
        
;--- DMA registers
WRITEMASK          EQU HDMABASE + DMAMASKREG * 2
WRITEMODE          EQU HDMABASE + DMAMODEREG * 2
CLEARFLIPFLOP      EQU HDMABASE + DMAFLIPFLOP * 2

;--- DMA mode
WANTEDMODE         EQU DMA_MODE_SINGLE or DMA_MODE_AUTOINIT or DMA_MODE_READ

	.DATA

rate            dw 22050
sblength        dw 0fffch - 1
baseaddr        dw BASEADDR
sbirq           dw SBIRQ
dmachn          dw DMACHANNEL
OLDInterrupt    dd ?
oldpic          db ?

	.const

CreateData macro
SMPVAL = 80h
	repeat 64	  ;80-FF
	db 0,SMPVAL
SMPVAL = SMPVAL + 2
	endm
SMPVAL = 0FEh
	repeat 128    ;FF-00
	db 0,SMPVAL
SMPVAL = SMPVAL - 2
	endm
SMPVAL = 0
	repeat 64     ;00-7F
	db 0,SMPVAL
SMPVAL = SMPVAL + 2
	endm
endm

SampleBuffer LABEL BYTE
	CreateData
SAMPLEBUFFERLENGTH equ $ - offset SampleBuffer
	CreateData

information label byte
	db 'TEST02 plays zigzag wave, 16bit autoinit mono unsigned (SB16 only).',10
	db 'Usage: TEST02 [frequency; def %u] [SB buffer length-1; def %u]',10
	db 0

info2           db 'Press ESC to exit!',13,10,'$'
sberror         db 'No SoundBlaster at base address %X.',10,0
rateerror       db 'Invalid rate entered.',13,10,'$'
lengtherror     db 'Invalid SB buffer length entered.',13,10,'$'
szBlaster       db 'BLASTER',0

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

;--- write logo
	invoke printf, offset information, rate, sblength

	invoke getenv, addr szBlaster, addr buffer
	.if ax
		invoke scanvar, 'A', addr buffer, 16, addr baseaddr
		invoke scanvar, 'I', addr buffer, 10, addr sbirq
		invoke scanvar, 'H', addr buffer, 10, addr dmachn
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

;--- enable SB speakers (for all SBs < SB16)
	mov dx,baseaddr
	add dx,SB_DSPWRITE
	WAITWRITE
	mov al,DSP_ENABLESPEAKER
	out dx,al

;--- set SB IRQ
	cli
	xor ax,ax
	mov es,ax
	mov si,sbirq
	add si,8
	shl si,2
	mov ax,es:[si+0]
	mov dx,es:[si+2]
	mov word ptr [OLDInterrupt+0],ax
	mov word ptr [OLDInterrupt+2],dx
	mov es:[si+0],offset SB_IRQ
	mov es:[si+2],cs
	sti

;--- unmask SB IRQ in PIC
	in al,21h
	mov oldpic,al
	mov cx,sbirq
	mov ah,1
	shl ah,cl
	xor ah,-1
	and al,ah
	out 21h,al

;------------------------------------------------
; Setup DMA-controller :

;------------------------------------------------
; calculate page and offset for 16bit DMAcontroller :
;
; check for DMApage override :
; ... problem: DMA controller separates memory into 64KB pages, you can only
; transfer data is placed in one page - no page overrides are allowed
;------------------------------------------------
; To solve that :
; create a DMA buffer with double size you want - if the first part is placed
; on a page border the second part is for sure not

	mov si,offset SampleBuffer
	mov ax,ds
	rol ax,4		; * 16 - higher 4 bits in al
	mov bl,al
	and bl,00fh 	; BL - higher 4 bits
	and al,0f0h 	; clear higher 4bits in AL
	add si,ax		; SI = offset
	adc bl,0		; BL = page

;--- now BL:SI holds 20-bit linear address

	mov cx,SAMPLEBUFFERLENGTH-1
	mov ax, si
	add ax, cx
	jnc nooverride

;--- second part must be used
	mov si, ax
	inc si		; si=si+1 - start of second part
	inc bl		; second part is then on the next page
nooverride:

;------------------------------------------------
; MASK DMA CHANNEL
;
	mov al,byte ptr dmachn
	and al,11b
	or al,DMA_MASK_DISABLE_CHN
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
	shr ax, 1		; must be shifted 1 to the right for 16-bit!
	out dx,al
	mov al,ah
	out dx,al
;------------------------------------------------
; WRITE BUFFERLENGTH (in words)-1
;
	call getcntport
	mov cx,SAMPLEBUFFERLENGTH/2-1		; count of words we need :)
	mov al,cl
	out dx,al
	mov al,ch
	out dx,al
;------------------------------------------------
; DEMASK CHANNEL
;
	mov al,byte ptr dmachn
	and al,3
	or al,DMA_MASK_ENABLE_CHN
	out WRITEMASK,al

;------------------------------------------------
; Setup SoundBlaster :
;
; SET SAMPLERATE
;
	mov dx, baseaddr
	add dx, SB_DSPWRITE
	WAITWRITE
	mov al, DSP_SETOUTSAMPLERATE    ;AL = 41, Set DAC Samplerate
	out dx, al
	mov cx, rate
	WAITWRITE
	mov al, ch
	out dx, al
	WAITWRITE
	mov al, cl
	out dx, al

;------------------------------------------------
; USE 16bit MONO UNSIGNED MODE (DSPC B6h 00h)
;
	WAITWRITE
	mov al, DSP_AUTOINITOUT16
	out dx, al
	WAITWRITE
	mov al, DSP_MODE_MONO + DSP_MODE_UNSIGNED
	out dx, al
	mov cx, sblength
	WAITWRITE
	mov al, cl
	out dx, al
	WAITWRITE
	mov al, ch
	out dx, al

	mov dx,offset info2
	mov ah,9
	int 21h

waitloop:
	mov ah,01
	int 16h
	jz waitloop

	xor ah,ah
	int 16h
	cmp al,27
	jne waitloop
exit:
	RESET_DSP baseaddr

;--- restore PIC mask
	mov al,oldpic
	out 21h,al

;--- restore SB IRQ
	cli
	xor ax,ax
	mov es,ax
	mov si,sbirq
	add si,8
	shl si,2
	mov ax,word ptr [OLDInterrupt+0]
	mov dx,word ptr [OLDInterrupt+2]
	mov es:[si+0],ax
	mov es:[si+2],dx
	sti
done:
	ret

; display error if Soundblaster is not on expected base address
RESET_ERROR:
	invoke printf, offset sberror, baseaddr
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

;--- SB IRQ proc to ack interrupt

SB_IRQ proc
	push ax
	push dx
	mov dx,cs:baseaddr
	add dx,SB16_DSPINTACK ;DX = IRQ ACKNOWLEDGE 16Bit
	in al,dx
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
	shl bx,4
	mov ss,ax
	add sp,bx
;--- release unused DOS memory
	mov bx,ss
	mov ax,es
	sub bx,ax

	mov ax,sp
	add ax,15

	shr ax,4

	add bx, ax
	mov ah, 4ah
	int 21h
	call _setargv
	invoke main, [_argc], [_argv]
	mov ax,4c00h
	int 21h

	END start
