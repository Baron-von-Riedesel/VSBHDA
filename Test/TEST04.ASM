;--------------------------------------------------------
; Play 8bit DMA mode with DSP cmd 0x14

; this variant uses the EMS page frame as sound buffer.
; Nothing is done to translate the address to a physical one.
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

;--- SoundBlaster SETUP
BASEADDR           EQU 0220h       ;SoundBlaster base address
SBIRQ              EQU 5           ;SoundBlaster IRQ
DMAchannel         EQU 1           ;SoundBlaster DMA channel

;--- PIC MASKS FOR MASK/DEMASK IRQ
PICORMASK          EQU 1 shl SBIRQ
PICANDMASK         EQU 0FFh xor PICORMASK

;--- DMA CONTROLLER REGISTERS :
WRITEMASK          EQU 00ah         ;WRITE MASK REGISTER
WRITEMODE          EQU 00bh         ;WRITE MODE REGISTER
CLEARFLIPFLOP      EQU 00ch
PAGE_CHN           EQU 083h         ;PAGE REGISTER FOR DMAchannel 1
BASE_CHN           EQU 002h         ;BASEADDRESS REGISTER DMA 1
COUNT_CHN          EQU 003h         ;COUNT REGISTER DMAchannel 1

;--- SAMPLERATE : (if you change it pay attention to maximum samplerate)
TIMECONST          EQU 165          ; = 10989 Hz (256-1000000/11000)

;--- DMA WRITE MODE
WANTEDMODE         EQU DMA_MODE_SINGLE or DMA_MODE_READ ; singlemode, nonautoinit, readmode

	include DMA.INC
	include SB.INC

CStr macro text:vararg
local sym
	.const
sym db text,0
	.code
	exitm <offset sym>
endm

	.const

SampleBuffer LABEL BYTE
    INCLUDE PCMDATA.INC

SAMPLEBUFFERLENGTH equ $ - offset SampleBuffer

information db 'TEST04 - play a 8bit mono sample one time using DSP cmd 0x14.',13,10
            db 'EMS page frame is used as sound buffer!',13,10
            db '$'

info2       db 'Stop playing with ESC.',13,10,'$'
sberror     db 'No SoundBlaster at base address 220h.',13,10,'$'
emserror    db 'No EMM installed.',13,10,'$'
emserror2   db 'No EMS page frame set.',13,10,'$'
emserror3   db 'EMS alloc failed.',13,10,'$'
emserror4   db 'EMS map failed.',13,10,'$'

	.data

oldInterrupt dd 0
wFrame      dw 0	;EMS page frame segment
wHandle     dw 0	;EMS handle
ready       db 0
oldpic      db 0

	.CODE
	.386
	include printf.inc
	.286

main proc c
		mov dx,offset information
		mov ah,9
		int 21h

		RESET_DSP

		; ENABLE SB SPEAKERS (for all SBs <SB16)
		mov dx,BASEADDR+00Ch 		;DX = DSP Write Data or Command
		WAITWRITE
		mov al,0D1h					; AL = Enable speaker
		out dx,al					; Output: DSP Write Data or Command

		; SETUP IRQ
		xor ax,ax
		mov es,ax					; es to page 0 (Interrupt table)
		mov si,(SBIRQ+8)*4			; si = position in interrupt table
		cli
		mov ax,es:[si+0]
		mov word ptr [oldInterrupt+0],ax
		mov ax,es:[si+2]
		mov word ptr [oldInterrupt+2],ax
		mov es:[si+0],offset SB_IRQ ; set offset of new interrupt routine
		mov es:[si+2],cs 			; set segment of new interrupt routine
		sti

		; unmask SB IRQ in PIC
		in al, 21h
		mov oldpic, al
		and al,PICANDMASK
		out 21h,al

;--- EMS stuff

		mov ax,es:[67h*4+0]
		mov dx,es:[67h*4+2]
		mov cx,ax
		or cx,dx
		jnz @F
		mov dx,offset emserror
		mov ah,9
		int 21h
		jmp exit2
@@:
		mov ah,41h	;get page frame segment
		int 67h
		cmp ah,0
		jz @F
		mov dx,offset emserror2
		mov ah,9
		int 21h
		jmp exit
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
		jmp exit
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
		jmp exit
@@:
		inc al
		inc bx
		loop nextpage

		mov es,wFrame
		xor di,di
		mov si,offset SampleBuffer
		mov cx,SAMPLEBUFFERLENGTH
		rep movsb

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

;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; Setup DMA-controller :
;
; 1st  MASK DMA CHANNEL
;
		mov al,DMAchannel
		or al,4
		out WRITEMASK,al
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; 2nd  CLEAR FLIPFLOP
;
		out CLEARFLIPFLOP,al
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; 3rd  WRITE TRANSFER MODE
;
		mov al,WANTEDMODE
		add al,DMAchannel
		out WRITEMODE,al
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; 4th  WRITE PAGE NUMBER
;
		mov al,bl
		out PAGE_CHN,al
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; 5th  WRITE BASEADDRESS
;
		mov ax,si
		out BASE_CHN,al
		mov al,ah
		out BASE_CHN,al
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; 6th  WRITE SAMPLELENGTH-1
;
		mov al,cl
		out COUNT_CHN,al
		mov al,ch
		out COUNT_CHN,al
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; 7th  DEMASK CHANNEL
;
		mov al,DMAchannel
		out WRITEMASK,al

;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; Setup SoundBlaster :
;
; 1st  SET TIMECONSTANT
;
		mov dx,BASEADDR+SB_DSPWRITE
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
		mov al,cl					;AL = LOWER PART SAMPLELENGTH
		out dx,al
		WAITWRITE
		mov al,ch					;AL = HIGHER PART SAMPLELENGTH
		out dx,al

		mov dx,offset info2
		mov ah,9
		int 21h

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
		mov al,DMAchannel
		or al,4
		out WRITEMASK,al
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

exit2:
		RESET_DSP

		; RESTORE PIC MASK
		mov al, oldpic
		out 21h,al

		; RESTORE IRQ
		cli
		xor ax,ax
		mov es,ax
		mov si,(SBIRQ+8)*4
		mov ax,word ptr [oldInterrupt+0]
		mov es:[si+0],ax
		mov ax,word ptr [oldInterrupt+2]
		mov es:[si+2],ax
		sti

		invoke  printf, CStr("SampleBufferLength=%X",10), SAMPLEBUFFERLENGTH
		out CLEARFLIPFLOP,al
		in al,COUNT_CHN
		mov ah,al
		in al,COUNT_CHN
		xchg al,ah
		invoke printf, CStr("DMA channel 1, cnt=%X",10), ax
done:
		ret

; display information if Soundblaster is not on this baseaddress
RESET_ERROR:
		mov dx,offset sberror
		mov ah,9
		int 21h
		jmp done

main endp

;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
; SB IRQ for detecting end of playing
; It's generated by the SoundBlaster hardware
;컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴
SB_IRQ proc
		push ax
		push dx
		push ds
		mov dx,BASEADDR+00Eh 		;DX = DSP DATA AVAILABLE (IRQ ACKNOWLEDGE)
		in al,dx
		mov ax,@data
		mov ds,ax
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
