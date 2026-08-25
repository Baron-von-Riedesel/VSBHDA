
;--- play ADPCM 4bit single-cycle ( DSP cmd 0x75 )

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
WANTEDMODE         EQU DMA_MODE_SINGLE or DMA_MODE_SINGLECYCLE or DMA_MODE_READ

CStr macro text:vararg
local sym
	.const
sym db text,0
	.code
	exitm <offset sym>
endm

	.data

SampleBuffer LABEL BYTE
	.radix 16
 db 080
 db 0f2,07b,0a2,03c,093,01c
 db 087,01d,004,08f,087,08f
 db 014,0ab,014,0ba,023,0c9
 db 050,0d0,038,0f0,078,0f1
 db 03a,0c3,029,0c4,019,0c3
 db 028,0f0,070,0f9,048,0cd
 db 041,0ab,032,09c,023,08f
 db 087,01c,094,01b,0a3,019
 db 0b2,028,0f0,070,0f9,041
 db 0ca,042,0ac,032,08f,011
 db 01c,084,059,0c0,072,0c9
 db 007,00f,092,03a,099,031
 db 08f,087,01a,0b2,011,0ba
 db 023,08f,093,02a,0a8,071
 db 0b9,013,08c,0d2,038,0f9
 db 040,09f,004,05b,0a0,062
 db 0aa,093,020,0fa,022,00d
 db 0a1,012,0ab,004,069,09b
 db 023,00d,0b1,022,0ab,085
 db 039,09b,023,00c,0e0,070
 db 09f,084,068,0f9,014,08b
 db 0f1,031,09b,090,071,099
 db 0a1,030,09e,0a2,022,0ab
 db 081,071,09a,0a2,012,09b
 db 091,031,09a,0a2,012,09b
 db 080,077,0a9,0a1,031,09b
 db 090,071,09a,091,030,09e
 db 0a1,013,00a,0ea,021,020
 db 0ea,083,051,08d,0a0,025
 db 01a,09a,012,010,0be,081
 db 061,00d,0b0,016,019,0b8
 db 007,060,0be,091,022,08b
 db 0e9,021,028,0ca,0b1,051
 db 019,0a9,092,021,08b,0d9
 db 011,030,0ac,0d0,007,068
 db 09e,091,005,019,09a,082
 db 034,01c,0e8,007,060,08f
 db 0f8,025,020,0cd,098,045
 db 018,0ac,0c0,037,068,08f
 db 0d8,007,051,08b,0cb,093
 db 051,009,0cd,082,034,018
 db 0ed,08a,044,022,0bc,0ba
 db 024,041,08f,0d8,093,050
 db 019,0d8,0b1,041,040,0ea
 db 0ca,002,075,009,09c,0b9
 db 023,042,08b,0fd,00a,034
 db 022,09b,0ac,093,014,038
 db 09f,0ca,003,074,029,0bb
 db 0c0,007,051,00b,0ca,0a8
 db 062,032,009,0fc,0a9,013
 db 031,020,0ec,09b,003,047
 db 031,08e,0cb,090,054,021
 db 00b,0bf,0ba,013,033,001
 db 0cf,0c9,093,031,031,0a8
 db 0fc,090,047,040,019,0da
 db 0ba,083,037,040,03a,0eb
 db 0aa,003,017,032,00b,0db
 db 09a,0a2,022,041,080,0dc
 db 09a,092,013,031,009,0db
 db 0c0,0b2,062,032,00c,0cf
 db 0b9,091,021,032,010,0ad
 db 0b9,0a9,010,037,031,081
 db 0ce,09b,0a0,014,073,011
 db 08c,09e,0ba,080,046,013
 db 011,09a,09b,0b0,0b3,052
 db 031,010,0ce,0b9,0a8,020
 db 046,030,031,09b,0cd,0b9
 db 080,026,013,011,009,0cd
 db 09b,099,002,052,012,020
 db 0ba,0da,09b,089,041,063
 db 011,009,0c9,0ea,0a8,092
 db 043,062,029,019,099,0aa
 db 0a0,0b1,031,063,011,009
 db 0aa,0bc,0ea,00a,003,043
 db 062,010,09a,0ac,0da,099
 db 001,005,022,020,001,0ab
 db 0bd,0b0,0d8,021,046,021
 db 010,091,0ac,0ca,0d9,0a0
 db 002,024,035,021,000,09c
 db 00c,0da,099,000,003,044
 db 033,033,009,09c,0ac,0da
 db 090,092,042,045,020,029
 db 090,0cb,0ca,0ca,0b1,010
 db 023
	.radix 10t
SAMPLEBUFFERLENGTH equ $ - offset SampleBuffer
	db SAMPLEBUFFERLENGTH dup (0)

	.const
information     db 'TEST08 [frequency]',13,10
                db 'plays a 4bit ADPCM single-cycle sound',13,10
                db 'frequency: default ~4000)',13,10
                db '$'
sberror         db 'No SoundBlaster at base address 220h.',13,10,'$'
rateerror       db 'Invalid rate entered.',13,10,'$'

	.data

	align 2
oldInterrupt        dd 0
dmabase             dw 0
sblength            dw SAMPLEBUFFERLENGTH-1
rate                dw 4000
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

;--- start msg
		mov dx,offset information
		mov ah,9
		int 21h

		cmp argc, 2
		jb options_done
		mov si, argv
		mov bx, [si+2]
		call getdec
		jc rate_invalid
		mov rate, ax
if 0
		cmp argc, 3
		jb options_done
		mov bx, [si+4]
		call getdec
		jc length_invalid
		mov sblength, ax
endif
options_done:

		RESET_DSP

;--- ENABLE SB SPEAKERS (for all SBs < SB16)
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
		lea di,[si+SAMPLEBUFFERLENGTH]
		mov cx,SAMPLEBUFFERLENGTH
		push si
		push ds
		pop es
		rep movsb
		pop si

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

		mov cx,dx
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
		mov dx,cx
		out dx,al
;------------------------------------------------
; use 8bit mono single-cycle (DSP command 014h XXXXh)
;
		WAITWRITE
		mov al,DSP_ADPCM4BITREF
		out dx,al
		mov cx,sblength
		WAITWRITE
		mov al,cl						;AL = LOWER PART SAMPLELENGTH
		out dx,al
		WAITWRITE
		mov al,ch						;AL = HIGHER PART SAMPLELENGTH
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

;--- error 'no sb found'
RESET_ERROR:
		mov dx, offset sberror
		mov ah, 9
		int 21h
		jmp done
rate_invalid:
		mov dx, offset rateerror
		mov ah, 9
		int 21h
		jmp done
if 0
length_invalid:
		mov dx, offset lengtherror
		mov ah, 9
		int 21h
		jmp done
endif
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
