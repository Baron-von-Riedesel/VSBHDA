; SBPLAY.COM - real mode Sound Blaster playback test (A220 I5 D1)
; plays 1 s of a 444 Hz square wave (8-bit single cycle DMA, 11111 Hz)
; three times and counts the IRQs
	org	100h
BASE	equ	220h
LEN	equ	11111
	mov	si,mstart
	call	puts
	; wait 2 s first: in a new DOS box under Windows the BIOS tick count
	; catches up quickly at the start (would end the IRQ timeouts early)
	push	ds
	xor	ax,ax
	mov	ds,ax
	mov	cx,[46Ch]
.settle:
	mov	ax,[46Ch]
	sub	ax,cx
	cmp	ax,36
	jb	.settle
	pop	ds
	call	dspreset
	cmp	al,0AAh
	je	.ok
	mov	si,mnodsp
	call	puts
	jmp	exit
.ok:	; pick a DMA buffer that doesn't cross a 64K page
	mov	di,buf1
	call	phys
	mov	bx,ax
	add	bx,LEN
	jnc	.b
	mov	di,buf2
.b:	mov	[bufoff],di
	; fill: 25 samples per period (12 high, 13 low)
	mov	cx,LEN
	xor	bl,bl
.f:	mov	al,0C0h
	cmp	bl,12
	jb	.h
	mov	al,40h
.h:	stosb
	inc	bl
	cmp	bl,25
	jb	.n
	xor	bl,bl
.n:	loop	.f
	; hook IRQ5 (INT 0Dh)
	mov	ax,350Dh
	int	21h
	mov	[oldvec],bx
	mov	[oldvec+2],es
	mov	ax,250Dh
	mov	dx,isr
	int	21h
	in	al,21h
	mov	[oldmask],al
	and	al,0DFh
	out	21h,al
	sti
	mov	al,0D1h		; speaker on
	call	dspw
	mov	al,40h		; time constant
	call	dspw
	mov	al,166		; 256-1000000/11111
	call	dspw
	mov	byte [loops],3
.play:	cli
	mov	di,[bufoff]
	call	phys		; DX:AX
	mov	bx,ax
	mov	al,5
	out	0Ah,al		; mask ch1
	xor	al,al
	out	0Ch,al		; clear flip flop
	mov	al,49h
	out	0Bh,al		; single, read, ch1
	mov	al,bl
	out	02h,al
	mov	al,bh
	out	02h,al
	mov	al,dl
	out	83h,al		; page ch1
	mov	ax,LEN-1
	out	03h,al
	mov	al,ah
	out	03h,al
	mov	al,1
	out	0Ah,al		; unmask ch1
	sti
	mov	al,14h		; 8-bit single cycle output
	call	dspw
	mov	al,(LEN-1) & 0FFh
	call	dspw
	mov	al,(LEN-1) >> 8
	call	dspw
	; wait for the IRQ, timeout 3 s
	mov	bl,[irqs]
	push	ds
	xor	ax,ax
	mov	es,ax
	mov	cx,[es:46Ch]
.w:	cmp	bl,[irqs]
	jne	.got
	mov	ax,[es:46Ch]
	sub	ax,cx
	cmp	ax,55
	jb	.w
	pop	ds
	mov	si,mtimeout
	call	puts
	jmp	short .next
.got:	pop	ds
	mov	ax,[es:46Ch]
	sub	ax,cx
	mov	[ticks],ax
	mov	si,mirq
	call	puts
	mov	al,byte [ticks]
	call	hex
	mov	si,mcrlf
	call	puts
.next:	dec	byte [loops]
	jnz	.play
	mov	al,0D3h		; speaker off
	call	dspw
	cli
	mov	al,[oldmask]
	out	21h,al
	sti
	push	ds
	lds	dx,[oldvec]
	mov	ax,250Dh
	int	21h
	pop	ds
	mov	si,mirqs
	call	puts
	mov	al,[irqs]
	call	hex
	mov	si,mcrlf
	call	puts
exit:	mov	ax,4C00h
	int	21h

isr:	push	ax
	push	dx
	mov	dx,BASE+0Eh
	in	al,dx		; acknowledge 8-bit IRQ
	inc	byte [cs:irqs]
	mov	al,20h
	out	20h,al
	pop	dx
	pop	ax
	iret

; DI = offset -> DX:AX = physical address
phys:	mov	ax,ds
	mov	dx,ax
	shl	ax,4
	shr	dx,12
	add	ax,di
	adc	dx,0
	ret
dspreset:
	mov	dx,BASE+6
	mov	al,1
	out	dx,al
	mov	cx,200
.d:	in	al,dx
	loop	.d
	xor	al,al
	out	dx,al
dspr:	mov	dx,BASE+0Eh
	mov	cx,0FFFFh
.w:	in	al,dx
	test	al,80h
	jnz	.r
	loop	.w
.r:	mov	dx,BASE+0Ah
	in	al,dx
	ret
dspw:	push	ax
	mov	dx,BASE+0Ch
	mov	cx,0FFFFh
.w:	in	al,dx
	test	al,80h
	jz	.o
	loop	.w
.o:	pop	ax
	out	dx,al
	ret
puts:	lodsb
	or	al,al
	jz	.e
	mov	dl,al
	mov	ah,2
	int	21h
	jmp	puts
.e:	ret
hex:	push	ax
	shr	al,4
	call	.n
	pop	ax
.n:	and	al,0Fh
	add	al,'0'
	cmp	al,'9'
	jbe	.p
	add	al,7
.p:	mov	dl,al
	mov	ah,2
	int	21h
	ret
mstart	db	'SBPLAY: 3 x 1 s square wave, A220 I5 D1',13,10,0
mnodsp	db	'no DSP at 220h',13,10,0
mtimeout db	'no IRQ within 3 s',13,10,0
mirq	db	'IRQ after ticks: ',0
mirqs	db	'IRQs total: ',0
mcrlf	db	13,10,0
irqs	db	0
loops	db	0
oldmask	db	0
ticks	dw	0
oldvec	dd	0
bufoff	dw	0
	align	16
buf1:	times	LEN db 0
	times	16384-LEN db 0
buf2:	times	LEN db 0
