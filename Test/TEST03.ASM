
; short dma buffer, 16-bit mono data, signed, SB16 only

	.286
	.MODEL small
	.STACK 400h
	.dosseg
	option casemap:none

BASEADDR           EQU 0220h       ;SoundBlaster base address
DMAchannel         EQU 5           ;SoundBlaster DMA channel
SBIRQ              EQU 5           ;SoundBlaster IRQ

;--- PIC MASKS FOR MASK/DEMASK IRQ
PICORMASK          EQU 1 shl SBIRQ
PICANDMASK         EQU 0FFh xor PICORMASK

	include DMA.INC
	include SB.INC

;--- DMA CONTROLLER REGISTERS (16bit)
WRITEMASK          EQU DMABase + 10 * DMAWidth ; Mask register
WRITEMODE          EQU DMABase + 11 * DMAWidth ; Mode register
CLEARFLIPFLOP      EQU DMABase + 12 * DMAWidth
PAGE16_CHN         EQU DMAPageReg(DMAchannel) ;Page register for DMAchannel
BASE16_CHN         EQU DMABaseReg(DMAchannel) ;Base address register
COUNT16_CHN        EQU DMACntReg(DMAchannel)  ;Count register

;--- DMA MODE
WANTEDMODE         EQU DMA_MODE_SINGLE or DMA_MODE_AUTOINIT or DMA_MODE_READ

	.DATA

oldInterrupt    dd ?
rate            dw 22050
sblength        dw 65532-1
oldpicmask      db ?
	align 2

	.const

	db "start data >"
SampleBuffer LABEL BYTE
	incbin <sine880_1601.wav>,2Ch
SAMPLEBUFFERLENGTH equ $ - offset SampleBuffer
	incbin <sine880_1601.wav>,2Ch
	db "< end data"

information db 'TEST03 plays sine wave, 16bit autoinit mono signed (SB16 only).',10
            db 'Usage: TEST03 [frequency; def %u] [SB buffer length-1; def %u]',10
            db 0
info2       db 'Press ESC to exit!',13,10,'$'
sberror     db 'No SoundBlaster at base address 220h.',13,10,'$'
rateerror   db 'Invalid rate entered.',13,10,'$'
lengtherror db 'Invalid SB buffer length entered.',13,10,'$'

	.CODE

	.386
	include PRINTF.INC
	.286
;------------------------------------------------
; Setup DMA-controller :

;------------------------------------------------
; calculate page and offset for 16bit DMAcontroller :
;
; segment*16+offset=20bit memory location-> upper 3 bits *2 = page
;                                           next 16 bits = offset
;                                           last 1 bit - lost in space :)
;                                           (because of word access)
;------------------------------------------------
setupdma proc
;------------------------------------------------
; check for DMApage override :
; ... problem: DMA controller separates memory into 64KB pages, you can only
; transfer data that's placed in one page - no page overrides are allowed
;------------------------------------------------
; To solve that :
; create a DMA buffer with double size you want - if the first part is placed
; on a page border the second part is for sure not

	mov si, offset SampleBuffer

	mov ax, ds
	rol ax, 4 	; * 16 - higher 4 bits in al
	mov bl, al
	and bl, 00fh	; BL - higher 4 bits
	and al, 0f0h	; clear higher 4bits in AL
	add si, ax	; SI = offset
	adc bl, 0 	; BL = page

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
	mov al,DMAchannel-4			   ; channels 0..3 for the 2nd DMAC
	or al,DMA_MASK_DISABLE_CHN
	out WRITEMASK,al
;------------------------------------------------
; CLEAR FLIPFLOP
;
	out CLEARFLIPFLOP,al
;------------------------------------------------
; WRITE TRANSFER MODE
;
	mov al,WANTEDMODE
	add al,DMAchannel-4
	out WRITEMODE,al
;------------------------------------------------
; WRITE PAGE NUMBER
;
	mov al,bl
	out PAGE16_CHN,al
;------------------------------------------------
; WRITE BASEADDRESS
;
	mov ax,si
	shr ax, 1		; must be shifted 1 to the right for 16-bit!
	out BASE16_CHN,al
	mov al,ah
	out BASE16_CHN,al
;------------------------------------------------
; WRITE BUFFERLENGTH (in words)-1
;
	mov cx,SAMPLEBUFFERLENGTH/2-1	; count of words we need :)
	mov al,cl
	out COUNT16_CHN,al
	mov al,ch
	out COUNT16_CHN,al
;------------------------------------------------
; DEMASK CHANNEL
;
	mov al,DMAchannel-4
	or al,DMA_MASK_ENABLE_CHN
	out WRITEMASK,al

	ret
setupdma endp

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

	cld
	mov si, argv
	add si, 2
	lodsw
	and ax, ax
	jz @F
	mov bx, ax
	call getdec
	jc rate_invalid
	mov rate, ax
	lodsw
	and ax, ax
	jz @F
	mov bx, ax
	call getdec
	jc length_invalid
	mov sblength, ax

@@:

;--- write logo
	invoke printf, offset information, rate, sblength

	RESET_DSP

;--- enable SB speakers
	mov dx,BASEADDR + SB_DSPWRITE
	WAITWRITE
	mov al,DSP_ENABLESPEAKER
	out dx,al

;--- set SB irq
	cli
	xor ax,ax
	mov es,ax
	mov si,(SBIRQ+8)*4
	mov ax,es:[si+0]
	mov dx,es:[si+2]
	mov word ptr [oldInterrupt+0],ax
	mov word ptr [oldInterrupt+2],dx
	mov es:[si+0],offset OWN_IRQ
	mov es:[si+2],cs
	sti

;--- unmask SB IRQ in PIC
	in al,21h
	mov oldpicmask, al
	and al,PICANDMASK
	out 21h,al

	call setupdma

;------------------------------------------------
; Setup SoundBlaster
;
; set SAMPLERATE
;
	mov dx, BASEADDR + SB_DSPWRITE
	WAITWRITE
	mov al, DSP_SETOUTSAMPLERATE	;AL = 41, Set DAC Samplerate
	out dx, al
	mov cx, rate
	WAITWRITE
	mov al, ch
	out dx, al
	WAITWRITE
	mov al, cl
	out dx, al

;------------------------------------------------
; set 16bit autoinit mode, mono, signed (DSP B6h 10h)

	WAITWRITE
	mov al, DSP_AUTOINITOUT16
	out dx, al
	WAITWRITE
	mov al, DSP_MODE_MONO or DSP_MODE_SIGNED
	out dx, al
	mov cx, sblength   ;SB length = samples - 1
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
	RESET_DSP

;--- restore IRQ
	cli
	xor ax,ax
	mov es,ax
	mov si,(SBIRQ+8)*4
	mov ax,word ptr [oldInterrupt+0]
	mov dx,word ptr [oldInterrupt+2]
	mov es:[si+0],ax
	mov es:[si+2],dx
	sti

;--- restore PIC mask
	mov al,oldpicmask
	out 021h,al
	ret

;--- error if soundblaster wasn't found

RESET_ERROR:
	mov dx, offset sberror
	mov ah, 9
	int 21h
	ret
rate_invalid:
	mov dx, offset rateerror
	mov ah, 9
	int 21h
	ret
length_invalid:
	mov dx, offset lengtherror
	mov ah, 9
	int 21h
	ret
main endp

;------------------------------------------------
; Sound IRQ generated by the SoundBlaster hardware
;------------------------------------------------
OWN_IRQ proc
	push ax
	push dx
	mov dx,BASEADDR+SB16_DSPINTACK	;DX = IRQ ACKNOWLEDGE 16Bit
	in al,dx
	mov al,20h
	out 20h,al
	pop dx
	pop ax
	IRET
OWN_IRQ endp

	include SETARGV.INC

start:
	mov ax,DGROUP
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
	shr ax,4
	add bx,ax
	mov ah,4ah
	int 21h
	call _setargv
	invoke main, [_argc], [_argv]
	mov ax,4c00h
	int 21h

	END start
