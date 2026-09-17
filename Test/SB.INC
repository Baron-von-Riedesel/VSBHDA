
;--- offsets for SBBASE

SB_DSPRESET     equ 06h
SB_DSPREAD      equ 0Ah
SB_DSPWRITE     equ 0Ch ;bit 7 indicates write buffer status
SB_DSPSTATUS    equ 0Eh ;bit 7 indicates read buffer status, int ack
SB16_DSPINTACK  equ 0Fh ;int ack 16bit

;--- DSP commands

DSP_DMADAC8BIT          equ 014h
DSP_SETTIMECONST        equ 040h
DSP_SETOUTSAMPLERATE    equ 041h ;send rate with high byte first
DSP_SETINPSAMPLERATE    equ 042h
DSP_SETSIZE             equ 048h
DSP_ADPCM4BIT           equ 074h ;4 bit single-cycle ADPCM
DSP_ADPCM4BITREF        equ 075h ;4 bit single-cycle ADPCM with Ref
DSP_ADPCM4BITAUTO       equ 07Dh ;4 bit autoinit ADPCM (with Ref)
DSP_DMADAC8BITHSAUTO    equ 090h ;8 bit autoinit highspeed
DSP_DMADAC8BITHS        equ 091h ;8 bit single-cycle highspeed
DSP_SINGLECYCLEOUT16    equ 0B0h ;16 bit single-cycle for output
DSP_AUTOINITOUT16       equ 0B6h ;16 bit autoinit for output
DSP_SINGLECYCLEOUT8     equ 0C0h ;8 bit single-cycle for output
DSP_AUTOINITOUT8        equ 0C6h ;8 bit autoinit for output
DSP_PAUSE8BIT           equ 0D0h
DSP_ENABLESPEAKER       equ 0D1h
DSP_DISABLESPEAKER      equ 0D3h
DSP_CONTINUE8BIT        equ 0D4h
DSP_PAUSE16BIT          equ 0D5h
DSP_CONTINUE16BIT       equ 0D6h

;--- DSP mode setting (0Bx, 0Cx) commands mode bits
;--- bit 4: 1=signed, 0=unsigned
;--- bit 5: 1=stereo, 0=mono

DSP_MODE_UNSIGNED       equ 00000b
DSP_MODE_SIGNED         equ 10000b
DSP_MODE_MONO           equ 000000b
DSP_MODE_STEREO         equ 100000b

;--- In : DX = cmd port (BASEADDR+0Ch)
;--- Destroys  : AL

WaitWrite macro
local loopWait

if @Model eq 7
	push ecx
else
	push cx
endif
	xor cx,cx
loopWait:
	in al,dx           ; AL = WRITE COMMAND STATUS
	and al,80h
	loopnz loopWait    ; Jump if bit7=1 - writing not allowed
if @Model eq 7
	pop ecx
else
	pop cx
endif
endm


;--- In : DX = Status port (BASEADDR+0Eh)
;--- Destroys  : AL

WaitRead macro
local loopWait

if @Model eq 7
	push ecx
else
	push cx
endif
	xor cx,cx
loopWait:
	in al,dx           ; AL = DATA AVAILABLE STATUS
	and al,80h
	loopz loopWait     ; Jump if bit7=0 - no data available
if @Model eq 7
	pop ecx
else
	pop cx
endif                        
ENDM

;--- Arguments : SB base address, error label (optional)
;--- Destroys  : DX,AL

RESET_DSP macro base,errdest
local SBthere
	mov dx,base
	add dx,SB_DSPRESET
	mov al,1
	out dx,al         ; start DSP reset

	in al,dx
	in al,dx
	in al,dx
	in al,dx          ; wait 3 æsec

	xor al,al
	out dx,al         ; end DSP Reset

	add dx,SB_DSPSTATUS-SB_DSPRESET
	WaitRead
	sub dx,SB_DSPSTATUS-SB_DSPREAD
	in al,dx
ifnb <errdest>
	cmp al,0aah       ; if there is a SB then it returns 0AAh
	je SBthere
	jmp errdest       ; No SB - error
SBthere:
endif
endm

