
;--- test FM synthesizer
;--- create binary: jwasm -mz test05.asm

	.286
	.model tiny, stdcall
	.stack 2048
	.dosseg
	option casemap:none

SBBASE equ 220h
SBPORT equ SBBASE+8	;FM out, both left & right (SB16 only?)

	include SB.INC

	.const

snd1    label byte
;--- channel 1
	db 020h,001h	;Set the modulator's multiple to 1
	db 040h,010h	;Set the modulator's level to about 40 dB
	db 060h,0F0h	;Modulator attack: quick; decay: long
	db 080h,077h	;Modulator sustain: medium; release: medium

	db 023h,001h	;Set the carrier's multiple to 1
	db 043h,000h	;Set the carrier to maximum volume (about 47 dB)
	db 063h,0F0h	;Carrier attack: quick; decay: long
	db 083h,077h	;Carrier sustain: medium; release: medium

	db 0A0h,098h	;Set voice frequency's LSB (it'll be a D#)

;--- channel 2
	db 021h,001h	;Set the modulator's multiple to 1
	db 041h,010h	;Set the modulator's level to about 40 dB
	db 061h,0F0h	;Modulator attack: quick; decay: long
	db 081h,077h	;Modulator sustain: medium;	release: medium

	db 024h,001h	;Set the carrier's multiple to 1
	db 044h,000h	;Set the carrier to maximum volume (about 47 dB)
	db 064h,0F0h	;Carrier attack: quick; decay: long
	db 084h,077h	;Carrier sustain: medium; release: medium

	db 0A1h,045h	;Set voice frequency's LSB

;--- Bx: bits 0-1: high bits frequency; 2-4: octave, 5: key on
	db 0B0h,02Ah	;Turn voice on;
	db 0B1h,02Ah	;Turn voice on;
	db 00

	.code

wait1 proc
	push ax
	mov  cx,10
	in al, 61h
	mov ah, al
@@:
	in al, 61h
	xor al,ah
	test al,10h
	jz @B
	loop @B
	pop ax
	ret
wait1 endp

wait2 proc
	push ax
	mov  cx,10
	in al, 61h
	mov ah, al
@@:
	in al, 61h
	xor al,ah
	test al,10h
	jz @B
	loop @B
	pop ax
	ret
wait2 endp

reset proc
	mov ax,00h
sm1:
	mov dx,SBPORT
	out dx,al
	call wait1
	inc dx
	xchg al,ah
	out dx,al
	xchg al,ah
	call wait2
	inc al
	cmp al,0F6h
	jnz sm1
	ret
reset endp

writebytes proc uses si pStream:ptr BYTE

	mov si, pStream
	cld
nextitem:
	mov dx,SBPORT
	lodsb
	and al,al
	jz done
	out dx,al
	call wait1
	inc dx
	lodsb
	out dx,al
	call wait2
	jmp nextitem
done:
	ret
writebytes endp

main proc c

	call reset
if 1
	mov dx,SBBASE+00Ch			;DX = DSP Write Data or Command
	WAITWRITE
	mov al,DSP_ENABLESPEAKER
	out dx,al
endif

	invoke writebytes, addr snd1
	push 40h
	pop es
	.386
	mov ecx, es:[6ch]
	add ecx, 48
@@:
	cmp ecx, es:[6ch]
	jae @B
	.286

if 1
	mov dx,SBBASE+00Ch			;DX = DSP Write Data or Command
	WAITWRITE
	mov al,DSP_DISABLESPEAKER
	out dx,al
endif

	call reset
	ret

main endp

start:
	mov ax, cs
	mov ds, ax
	mov bx, ss
	sub bx, ax
	shl bx, 4
	mov ss, ax
	add sp, bx
	call main
	mov ax,4c00h
	int 21h

	END start
