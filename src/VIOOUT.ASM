
;--- low-level output for dbgprintf()
;--- may be used in ISR; however, it's
;--- assumed selector of ds/es is data alias of cs.

	.386
ifdef ?FLAT
	.model flat
else
	.model small
endif
	option casemap:none

	.data

if @Model ne 7
externdef c DSBase:dword
endif

	.code

@getcursorpos macro reg
	movzx e&reg, byte ptr [edi+62h];page
	mov reg, [edi+e&reg*2+50h]
endm

@setcursorpos macro reg
	movzx e&reg, byte ptr [edi+62h];page
	mov [edi+e&reg*2+50h], ax
endm

;--- print a char
;--- no registers modified

VioPutChar proc

local	wCols:word
local	bChar:byte
local	bRows:byte

	pushad

	mov edi, 400h

if @Model ne 7
	sub edi, cs:[DSBase]
endif
	mov bChar, al
	mov ch, [edi+84h]		; rows-1
	mov cl, [edi+4Ah]		; cols
	mov bRows, ch
	@getcursorpos bx		; bh=row, bl=col
	movzx eax, bh			; eax=row
	movzx ecx, cl
	mov wCols, cx
	mul cl
	add eax, eax
	movzx ebx, bl			; ebx=col
	add ebx, ebx
	add ebx, eax
	mov si, [edi+04Eh]		; page offset
	cmp word ptr [edi+63H], 3B4h
	jz @F
	add si, 8000h
@@:
	movzx esi, si
	add esi, 0B0000h
if @Model ne 7
	sub esi, cs:[DSBase]
endif
	mov al, bChar

	cmp al, 13
	jnz @F
	mov eax, ebx
	shr eax, 1
	div cl
	mov al, ah
	xor ah, ah
	add eax, eax
	sub ebx, eax
	jmp char_done
@@:
	cmp al, 10
	jnz @F
	add ebx, ecx
	add ebx, ecx
	jmp char_done
@@:
	cmp al, 8
	jnz @F
	sub ebx, 2
	jmp char_done
@@:
	mov [ebx+esi], al
	inc ebx
	inc ebx
char_done:
	mov al, bRows
	inc al
	mul cl
	add eax, eax
	cmp ebx, eax
	jc @F
	call scrollup
	mov ebx, eax
@@:
	mov eax, ebx
	push eax
	mov cx, wCols
	shr eax, 1
	div cl
	xchg al, ah
	@setcursorpos bx
	pop eax
	call cursorset
	popad
	ret

cursorset:
	add ax, [edi+4EH]	;offset page
	mov dx, [edi+63H]
	shr ax, 1			;the CRT offset is one plane only, no attribute bytes 
	mov cl, al			;first high byte
	mov al, 0eh
	out dx, ax
	mov ah, cl			;then low byte
	mov al, 0fh
	out dx, ax
	retn

scrollup:			;scroll up one line
	mov edi, esi
	push edi
	movzx esi, wCols
	lea esi, [esi*2+edi]
	mov cl, byte ptr wCols
	mov al, bRows
	mul cl
	movzx ecx, ax
	shr cx,1
	rep movsd [edi], [esi]
	push edi
	mov cx, wCols
	mov eax,07200720h
	shr cx, 1
	rep stosd [edi]
	pop eax
	pop edi
	sub eax, edi
	retn

VioPutChar endp

if 0
VioGetCsrPos proc
	push edi
	mov edi, 400h
if @Model ne 7
	sub edi, cs:[DSBase]
endif
	@getcursorpos ax
	pop edi
	ret
VioGetCsrPos endp

;--- set cursor pos
;--- usage: invoke VioSetCsrPos, wPos
;--- col: lobyte wPos
;--- row: hibyte wPos

VioSetCsrPos proc
	mov eax, [esp+4]
	push edi
	mov edi, 400h
if @Model ne 7
	sub edi, cs:[DSBase]
endif
	@setcursorpos dx
	pop edi
	ret 4
VioSetCsrPos endp
endif

	end
