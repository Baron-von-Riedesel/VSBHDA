;--------------------------------------------------------
; Play 8bit DMA mode with DSP cmd 0x14

; this variant uses the EMS page frame as sound buffer.
; The sound buffer address isn't translated to a physical address -
; this is supposed to be done by the v86 monitor program.
;
; + DSP command 14h  ... play 8bit mono no autoinit
; + DSP command 40h  ... set sample rate
; + DSP command D1h  ... Enable Speaker
;

	.286
	.MODEL SMALL
	.dosseg
	.STACK 400h
	option casemap:none

;--- default values

BASEADDR           EQU 220h        ; base address
SBIRQ              EQU 5           ; IRQ
DMACHANNEL         EQU 1           ; DMA channel

	include DMA.INC
	include SB.INC

;--- DMA registers
WRITEMASK          EQU LDMABASE + DMAMASKREG
WRITEMODE          EQU LDMABASE + DMAMODEREG
CLEARFLIPFLOP      EQU LDMABASE + DMAFLIPFLOP

;--- DMA mode
WANTEDMODE         EQU DMA_MODE_SINGLE or DMA_MODE_READ ; singlemode, nonautoinit, readmode

TIMECONST          EQU 165          ; = 10989 Hz (256-1000000/11000)

CStr macro text:vararg
local sym
	.const
sym db text,0
	.code
	exitm <offset sym>
endm

	.const

SampleBuffer LABEL BYTE
	INCBIN <TEST04.VOC>,20h

SAMPLEBUFFERLENGTH equ $ - offset SampleBuffer

information label byte 
	db 'TEST04 plays a 8bit mono sample one time using DSP cmd 0x14.',10
	db 'EMS page frame is used as sound buffer!',10
	db 0
szBlaster db 'BLASTER',0

sberror     db 'No SoundBlaster at base address %X.',10,0
emserror    db 'No EMM installed.',13,10,'$'
emserror2   db 'No EMS page frame set.',13,10,'$'
emserror3   db 'EMS alloc failed.',13,10,'$'
emserror4   db 'EMS map failed.',13,10,'$'

	.data

oldInterrupt dd 0
wFrame      dw 0	;EMS page frame segment
wHandle     dw 0	;EMS handle
baseaddr    dw BASEADDR  ; SB base port
sbirq       dw SBIRQ
dmachn      dw DMACHANNEL
ready       db 0
oldpic      db 0
bPage       db 0
wAddr       dw 0

	.data?

buffer db 128 dup (?)

	.CODE
	.386
	include PRINTF.INC
	.286

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

	invoke printf, addr information

	invoke getenv, addr szBlaster, addr buffer
	.if ax
		invoke scanvar, 'A', addr buffer, 16, addr baseaddr
		invoke scanvar, 'I', addr buffer, 10, addr sbirq
		invoke scanvar, 'D', addr buffer, 10, addr dmachn
	.endif

;--- EMS stuff

	xor ax,ax
	mov es,ax
	mov ax,es:[67h*4+0]
	mov dx,es:[67h*4+2]
	mov cx,ax
	or cx,dx
	jnz @F
	mov dx,offset emserror
	mov ah,9
	int 21h
	jmp done
@@:
	mov ah,41h	;get page frame segment
	int 67h
	cmp ah,0
	jz @F
	mov dx,offset emserror2
	mov ah,9
	int 21h
	jmp done
@@:
	mov wFrame,bx
	mov ah,43h	;alloc EMS (BX pages)
	mov bx,4
	int 67h
	cmp ah,0
	jz @F
	mov dx,offset emserror3
	mov ah,9
	int 21h
	jmp done
@@:
	mov wHandle,dx
	mov cx,4
	mov al,0
	xor bx,bx
nextpage:
	mov ah,44h	;map memory
	int 67h
	cmp ah,0
	jz @F
	mov dx,offset emserror4
	mov ah,9
	int 21h
	jmp done
@@:
	inc al
	inc bx
	loop nextpage

	mov es,wFrame
	xor di,di
	mov si,offset SampleBuffer
	mov cx,SAMPLEBUFFERLENGTH
	rep movsb

	RESET_DSP baseaddr

;--- ENABLE SB SPEAKERS (for all SBs <SB16)
	mov dx,baseaddr
	add dx,00Ch
	WAITWRITE
	mov al,DSP_ENABLESPEAKER	; AL = Enable speaker
	out dx,al					; Output: DSP Write Data or Command

;--- SETUP IRQ
	xor ax,ax
	mov es,ax					; es to page 0 (Interrupt table)
	mov si,sbirq
	add si,8
	shl si,2					; si = position in interrupt table
	cli
	mov ax,es:[si+0]
	mov word ptr [oldInterrupt+0],ax
	mov ax,es:[si+2]
	mov word ptr [oldInterrupt+2],ax
	mov es:[si+0],offset SB_IRQ ; set offset of new interrupt routine
	mov es:[si+2],cs 			; set segment of new interrupt routine
	sti

;--- unmask SB IRQ in PIC
	in al, 21h
	mov oldpic, al
	mov cx,sbirq
	mov ah,1
	shl ah,cl
	xor ah,-1
	and al,ah
	out 21h,al

;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; calculate page and offset for DMAcontroller :
;
; segment*16+offset - 20bit memory location -> upper 4 bits  = page
;                                              lower 16 bits = offset
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
	mov cx,SAMPLEBUFFERLENGTH-1
	mov si,wFrame
	mov bx,si
	shr bx,12	 ;bits 16-19 into bl
	shl si,4
	mov bPage,bl
	mov wAddr,si

;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; Setup DMA-controller :
;
; 1st  MASK DMA CHANNEL
;
	mov al,byte ptr dmachn
	and al,11b
	or al,4
	out WRITEMASK,al
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; 2nd  CLEAR FLIPFLOP
;
	out CLEARFLIPFLOP,al
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; 3rd  WRITE TRANSFER MODE
;
	mov al,byte ptr dmachn
	and al,11b
	or al,WANTEDMODE
	out WRITEMODE,al
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; 4th  WRITE PAGE NUMBER
;
	call getpageport
	mov al,bl
	out dx,al
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; 5th  WRITE BASEADDRESS
;
	call getbaseport
	mov ax,si
	out dx,al
	mov al,ah
	out dx,al
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; 6th  WRITE SAMPLELENGTH-1
;
	call getcntport
	mov al,cl
	out dx,al
	mov al,ch
	out dx,al
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; 7th  DEMASK CHANNEL
;
	mov al,byte ptr dmachn
	and al,11b
	out WRITEMASK,al

;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; Setup SoundBlaster :
;
; 1st  SET TIMECONSTANT
;
	mov dx,baseaddr
	add dx,SB_DSPWRITE
	WAITWRITE
	mov al,040h					;AL = Set timeconstant
	out dx,al
	WAITWRITE
	mov al,TIMECONST
	out dx,al
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; 2nd  use 8bit mono nonautoinit (DSPcommand 014h XXXXh)
;
	WAITWRITE
	mov al,014h					;AL = DMA DAC 8bit
	out dx,al
	mov cx,SAMPLEBUFFERLENGTH-1
	WAITWRITE
	mov al,cl
	out dx,al
	WAITWRITE
	mov al,ch
	out dx,al

waitloop:
	cmp [ready],0				; check if sound sent
	jnz exit
	mov ah,01					;AH = Check for character function
	int 16h
	jz waitloop
	xor ah,ah					;Read character, flush keypress
	int 16h
	cmp ah,1
	jnz waitloop

exit:
;--- mask DMA channel
	mov al,byte ptr dmachn
	and al,11b
	or al,4
	out WRITEMASK,al

	RESET_DSP baseaddr

;--- RESTORE PIC MASK
	mov al, oldpic
	out 21h,al

;--- RESTORE IRQ
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

	invoke printf, CStr("SampleBuffer Address=%X%04X",10), bPage, wAddr
	invoke printf, CStr("SampleBuffer Length=%X",10), SAMPLEBUFFERLENGTH
	out CLEARFLIPFLOP,al
	call getcntport
	in al,dx
	mov ah,al
	in al,dx
	xchg al,ah
	invoke printf, CStr("DMA channel %u, cnt=%X",10), dmachn, ax

done:
;--- unmap pages and release EMS handle
	mov dx,wHandle
	and dx,dx
	jz noems
	mov cx,4
	mov al,0
nextpage2:
	mov bx,-1
	mov ah,44h
	int 67h
	inc al
	loop nextpage2
	mov dx,wHandle
	mov ah,45h	;release EMS handle
	int 67h
noems:
	ret

RESET_ERROR:
;--- display information if Soundblaster is not on this baseaddress
	invoke printf, addr sberror, baseaddr
	jmp done

main endp

SB_IRQ proc
	push ax
	push dx
	push ds
	mov ax,@data
	mov ds,ax
	mov dx,baseaddr
	add dx,00Eh
	in al,dx
	mov [ready],1
	mov al,020h
	out 020h,al					;ACKNOWLEDGE HARDWARE INTERRUPT
	pop ds
	pop dx
	pop ax
	IRET
SB_IRQ endp

start:
	mov ax,DGROUP
	mov ds,ax
	mov bx,ss
	sub bx,ax
	shl bx,004h
	mov ss,ax
	add sp,bx
	call main
	mov ax,4c00h
	int 21h

	END start
