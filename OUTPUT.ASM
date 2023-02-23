
	.386
	.model small

;--- display 32-bit number on screen
;--- low-level.

	.data

;externdef c _DPMI_Addressing:dword
_DPMI_Addressing dd 0d7h	; flat selector

	.code

	assume ds:_DATA

@getcursorpos macro
	movzx ebx, byte ptr ds:[462h];page
	mov bx, ds:[EBX*2+450h]
endm

@setcursorpos macro
	movzx ebx, byte ptr ds:[462h];page
	mov ds:[EBX*2+450h], ax
endm

;--- print a char
;--- no registers modified

VioPutChar proc

local	wCols:word
local	bChar:byte
local	bRows:byte

	push ds
	pushad

	mov bChar, al
	mov ds, [_DPMI_Addressing]
	mov ch, ds:[0484h]			; rows-1
	mov cl, ds:[044Ah]			; cols
	mov bRows, ch
	@getcursorpos				; bh=row, bl=col
	mov al, bh		;row pos
	mov ch, 0
	mov wCols, cx
	mul cl
	add ax, ax
	mov bh, 00  ; bx=col pos
	add bx, bx
	add bx, ax
	mov si, ds:[044Eh]			; page offset
	cmp word ptr ds:[0463H],3B4h
	jz @F
	add si, 8000h
@@:
	movzx esi, si
	add esi, 0B0000h

	mov al, bChar

	cmp al, 13
	jnz @F
	mov ax, bx
	shr ax, 1
	div cl
	mov al, ah
	xor ah, ah
	add ax, ax
	sub bx, ax
	jmp char_done
@@:
	cmp al, 10
	jnz @F
	add bx, cx
	add bx, cx
	jmp char_done
@@:
	cmp al, 8
	jnz @F
	sub bx, 2
	jmp char_done
@@:
	movzx ebx, bx
	mov ds:[ebx+esi], al
	inc bx
	inc bx
char_done:
	mov al, bRows
	inc al
	mul cl
	add ax, ax
	cmp bx, ax
	jc @F
	call scrollup
	mov bx, ax
@@:
	mov ax, bx
	push ax
	mov cx, wCols
	shr ax, 1
	div cl
	xchg al, ah
	@setcursorpos
	pop ax
	call cursorset
	popad
	pop ds
	ret

cursorset:
	add ax, ds:[044EH]	;offset page
	mov dx, ds:[0463H]
	shr ax, 1			;the CRT offset is one plane only, no attribute bytes 
	mov cl, al			;first high byte
	mov al, 0eh
	out dx, ax
	mov ah, cl			;then low byte
	mov al, 0fh
	out dx, ax
	retn

scrollup:			;scroll up one line
	push es
	push ds
	pop es
	mov edi, esi
	push di
	movzx esi, wCols
	lea esi, [esi*2+edi]
	mov cl, byte ptr wCols
	mov al, bRows
	mul cl
	movzx ecx, ax
	shr cx,1
	rep movsd es:[edi], ds:[esi]
	push di
	mov cx, wCols
	mov eax,07200720h
	shr cx, 1
	rep stosd es:[edi]
	pop ax
	pop di
	sub ax, di
	pop es
	retn

VioPutChar endp

if 0

DispNumber proc public number:dword, pos:dword

	pushad
	lar eax, [wFlat]
	jz @F
	mov ax,0
	mov cx,1
	int 31h
	jc exit
	mov ebx,eax
	mov [wFlat], eax
	or cx,-1
	or dx,-1
	mov ax,8
	int 31h
@@:
	push ds
	mov ds, [wFlat]
	mov ebx, pos
	shl ebx, 1
	add ebx, 0B8000h
	mov eax, number
	mov edi, 10
@@nextdigit:
	xor edx, edx
	div edi
	add dl,'0'
	cmp dl,'9'
	jbe @F
	add dl,7+20h
@@:
	mov [ebx],dl
	sub ebx, 2
	and eax, eax
	jne @@nextdigit
	pop ds
exit:
	popad
	ret

DispNumber endp

endif

	end

