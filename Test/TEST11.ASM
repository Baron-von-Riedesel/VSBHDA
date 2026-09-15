
;--- Play 16-bit mono unsigned (DSP command B6h) using 8bit DMA channel.

	.286
	.model small
	.dosseg
	.stack 2048
	option casemap:none

;--- default values
BASEADDR           EQU 220h
SBIRQ              EQU 5
DMACHANNEL         EQU 1
RATE               EQU 11025
DMABUFFERSIZE      EQU 8*1024	; DMA buffer size

	include DMA.INC
	include SB.INC

;--- DMA controller registers
WRITEMASK          EQU LDMABASE + DMAMASKREG
WRITEMODE          EQU LDMABASE + DMAMODEREG
CLEARFLIPFLOP      EQU LDMABASE + DMAFLIPFLOP
;--- DMA mode
WANTEDMODE         EQU 01011000b    ; singlemode, autoinit, readmode

ReadMixer MACRO register
	mov dx,baseaddr
	add dx,4
	mov al,register
	out dx,al
	inc dx
	in al,dx
ENDM

WriteMixer MACRO register
	mov dx,baseaddr
	add dx,4
	mov al,register
	out dx,ax
	inc dx
	mov al,ah
	out dx,al
ENDM

	.const

SampleBuffer LABEL BYTE
    INCBIN <TEST04.VOC>,20h
SAMPLEBUFFERLENGTH equ $ - offset SampleBuffer

information     db 'TEST11 - play 16bit mono unsigned data with 8bit DMA (SB16 only)',13,10
                db 'Stop playing with <ESC>.',13,10,'$'
memerror        db 'Not enough memory to create the DMA buffer',13,10,'$'
sberror         db 'No SoundBlaster at base address 220h.',13,10,'$'
szBlaster       db 'BLASTER',0

	.data

OldInterrupt    dd ?
rate            dw RATE
baseaddr        dw BASEADDR  ; SB base port
sbirq           dw SBIRQ
dmachn          dw DMACHANNEL

DosBufferSeg    dw ?	;segment of 16-bit destination buffer
position        dw offset SampleBuffer	; current position in 8-bit sample buffer 
;--- 20-bit DMA address of destination buffer
DMABufferOfs    dw ?	;address A0-A15
DMABufferPage   db ?	;address A16-A19
part            db 0
OldMixer81      db 0
oldpic          db 0

	.data?

buffer db 128 dup (?)

	.code

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

main proc c

	mov dx,offset information
	mov ah,9
	int 21h

	invoke getenv, addr szBlaster, addr buffer
	.if ax
		invoke scanvar, 'A', addr buffer, 16, addr baseaddr
		invoke scanvar, 'I', addr buffer, 10, addr sbirq
		invoke scanvar, 'D', addr buffer, 10, addr dmachn
	.endif

;--- alloc sound buffer ( size * 2 )

	mov bx,SAMPLEBUFFERLENGTH shr 3
	mov ah,48h
	int 21h
	jnc @F
	mov dx,offset memerror
	mov ah,9
	int 21h
	jmp done
@@:

;--- AX = segment of DMA buffer

;--- calculate page and offset for DMAcontroller;

	rol ax,4	; * 16 - higher 4 bits in al
	mov bl,al
	and bl,00fh ; BL - higher 4 bits
	and al,0f0h ; clear higher 4bits in AL
	adc bl,0	; BL = page

	mov cx,SAMPLEBUFFERLENGTH
	neg ax
	cmp ax,cx
	ja nooverride
	neg ax
	add ax,cx
	inc ax
	inc bl
nooverride:
	mov [DMABufferOfs],ax
	mov [DMABufferPage],bl

;--- calculate segment address of destination buffer

	shl bl,4
	mov bh,bl
	mov bl,0	;bx=segment address of A16-A19   
	shr ax,4	;ax=segment address of A4-A15
	add ax,bx
	mov [DosBufferSeg],ax

;--- clear the destination buffer, because ConvertHalf will write the hibytes only.

	mov es,ax
	mov cx,DMABUFFERSIZE/2
	xor di,di
	rep stosw

;--- fill the whole buffer by calling ConvertHalf 2 times.

	call ConvertHalf	;modifies es,si,di!
	call ConvertHalf

;--- SB setup

;--- first setup DMA for 16bit sound with 8bit dma;
;--- means to clear 16-bit DMA channel in mixer register;

	ReadMixer 81h
	mov [OldMixer81],al
	and al,0fh
	mov ah,al
	WriteMixer 81h

	RESET_DSP baseaddr

;--- enable SB SPEAKERS
	mov dx,baseaddr
	add dx,SB_DSPWRITE
	WAITWRITE
	mov al,DSP_ENABLESPEAKER
	out dx,al

;--- setup IRQ
	xor ax,ax
	mov es,ax
	mov si,sbirq
	add si,8
	shl si,2
	cli
	mov ax,es:[si+0]
	mov word ptr [OldInterrupt+0],ax
	mov ax,es:[si+2]
	mov word ptr [OldInterrupt+2],ax
	mov es:[si+0],offset SB_IRQ
	mov es:[si+2],cs
	sti

;--- change PIC mask
	in al,021h
	mov oldpic,al
	mov cx,sbirq
	mov ah,1
	shl ah,cl
	xor ah,-1
	and al,ah
	out 021h,al
;
;--- Setup DMA controller
;
	mov al,byte ptr dmachn
	or al,4
	out WRITEMASK,al

	out CLEARFLIPFLOP,al

	mov al,byte ptr dmachn
	or al,WANTEDMODE
	out WRITEMODE,al

	call getpageport
	mov al,[DMABufferPage]
	out dx,al

	call getbaseport
	mov ax,[DMABufferOfs]
	out dx,al
	mov al,ah
	out dx,al

	call getcntport
	mov cx,DMABUFFERSIZE-1
	mov al,cl
	out dx,al
	mov al,ch
	out dx,al

	mov al,byte ptr dmachn
	out WRITEMASK,al
;
;--- setup SoundBlaster
;
	mov dx,baseaddr
	add dx,SB_DSPWRITE
	WAITWRITE
	mov al,DSP_SETOUTSAMPLERATE
	out dx,al
	WAITWRITE
	mov cx,rate
	mov al,ch
	out dx,al
	WAITWRITE
	mov al,cl
	out dx,al

	WAITWRITE
	mov al,DSP_AUTOINITOUT16
	out dx,al
	WAITWRITE
	mov al,000h 			;AL = mono unsigned data
	out dx,al
	mov cx,DMABUFFERSIZE/4-1
	WAITWRITE
	mov al,cl
	out dx,al
	WAITWRITE
	mov al,ch
	out dx,al

;--- play loop

waitloop:
	mov ah,01
	int 16h
	jz waitloop
	xor ah,ah
	int 16h
	cmp al,'p'
	je pause_
	cmp al,27
	jne waitloop
	jmp exit

pause_:
;--- pause playing
	mov dx,baseaddr
	add dx,SB_DSPWRITE
	WAITWRITE
	mov al,DSP_PAUSE16BIT
	out dx,al
	xor ah,ah
	int 16h
	mov dx,baseaddr
	add dx,SB_DSPWRITE
	WAITWRITE
	mov al,DSP_CONTINUE16BIT
	out dx,al
	jmp waitloop

exit:
	RESET_DSP baseaddr

;--- restore DMA settings in mixer
	mov ah,[OldMixer81]
	WriteMixer 81h

;--- restore PIC mask
	mov al,oldpic
	out 21h,al

;--- restore IV
	cli
	xor ax,ax
	mov es,ax
	mov si,sbirq
	add si,8
	shl si,2
	mov ax,word ptr [OldInterrupt+0]
	mov es:[si+0],ax
	mov ax,word ptr [OldInterrupt+2]
	mov es:[si+2],ax
	sti
done:
	ret
RESET_ERROR:
	mov dx,offset sberror
	mov ah,9
	int 21h
	jmp done
main endp

;--- SB IRQ

SB_IRQ proc
	pusha
	push ds
	push es
	mov ax,@data
	mov ds,ax
	mov dx,baseaddr
	add dx,SB16_DSPINTACK
	in al,dx
	mov ax,0e2eh
	int 10h
	call ConvertHalf	; fill next half...
	mov al,20h
	out 20h,al
	pop es
	pop ds
	popa
	iret
SB_IRQ endp
;
;--- ConvertHalf translates 8-bit data in SampleBuffer to
;--- 16-bit PCM format in dynamically allocated DOS buffer.
;
ConvertHalf proc
	mov cx,DMABUFFERSIZE/2			  ; half buffer size in bytes
	xor di,di
	test [part],1
	je @F
	mov di,cx
@@:
	shr cx,1						; count of words in half buffer
	mov es,[DosBufferSeg]
	mov si,[position]
	cld
nextsample:
	inc di	; skip the low byte ( it's 00 )
	movsb
	cmp si,offset SampleBuffer + SAMPLEBUFFERLENGTH
	jb @F
	mov si,offset SampleBuffer	; reset src pointer
@@:
	loop nextsample
	mov [position],si
	inc [part]
	RET
ConvertHalf endp

start:
	mov ax,@data
	mov ds,ax
	mov bx,ss
	sub bx,ax
	shl bx,4
	mov ss,ax
	add sp,bx
;--- free DOS memory
	mov cx,es
	sub ax,cx
	mov bx,sp
	shr bx,4
	add bx,ax
	mov ah,4ah
	int 21h
	call main
	mov ah,4Ch
	int 21h

	END start
