
;--- low-level output for dbgprintf()

	.386
	.model small
	option casemap:none

;--- low-level screen output, may be used in ISR.

_GO32_INFO_BLOCK struct
dwSize            dd ?
dwPrimaryScreen   dd ?
dwSecondaryScreen dd ?
dwTransferBuffer  dd ?	; linear_address_of_transfer_buffer;
dwSizeTransferBuf dd ?	; size_of_transfer_buffer; /* >= 4k */
dwPid             dd ?
bMasterPICBase    db ?	; master_interrupt_controller_base;
bSlavePICBase     db ?	; slave_interrupt_controller_base;
wFlat             dw ?	; selector_for_linear_memory ( 0-10FFFF only );
dwStubInfo        dd ?	; linear_address_of_stub_info_structure;
dwPSP             dd ?	; linear_address_of_original_psp;
wRunMode          dw ?	; run_mode;
wRunModeInfo      dw ?	; run_mode_info;
_GO32_INFO_BLOCK ends

;--- _go32_info_block is a comm var, but defining it as such
;--- would actually include this module even if it's not used at all.

externdef c _go32_info_block:_GO32_INFO_BLOCK

	.code

@getcursorpos macro reg
	movzx e&reg, byte ptr ds:[462h];page
	mov reg, ds:[e&reg*2+450h]
endm

@setcursorpos macro reg
	movzx e&reg, byte ptr ds:[462h];page
	mov ds:[e&reg*2+450h], ax
endm

;--- print a char
;--- no registers modified

VioPutChar proc

local	wCols:word
local	bChar:byte
local	bRows:byte

	push ds
	pushad

	mov ds, cs:[_go32_info_block.wFlat]
	mov bChar, al
	mov ch, ds:[0484h]		; rows-1
	mov cl, ds:[044Ah]		; cols
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
	mov ds:[ebx+esi], al
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
	push edi
	movzx esi, wCols
	lea esi, [esi*2+edi]
	mov cl, byte ptr wCols
	mov al, bRows
	mul cl
	movzx ecx, ax
	shr cx,1
	rep movsd es:[edi], ds:[esi]
	push edi
	mov cx, wCols
	mov eax,07200720h
	shr cx, 1
	rep stosd es:[edi]
	pop eax
	pop edi
	sub eax, edi
	pop es
	retn

VioPutChar endp

VioGetCsrPos proc
	@getcursorpos ax
	ret
VioGetCsrPos endp

;--- set cursor pos
;--- usage: invoke VioSetCsrPos, wPos
;--- col: lobyte wPos
;--- row: hibyte wPos

VioSetCsrPos proc
	mov eax, [esp+4]
	@setcursorpos dx
	ret 4
VioSetCsrPos endp

	end
