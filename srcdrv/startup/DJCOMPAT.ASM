
;--- a few adjustments in case the drivers are compiled with djgpp.

	.386
	.model tiny
	option casemap:none
	option dotname

;--- force jwlink to add .text to dgroup

.text segment dword public 'CODE'
.text ends

DGROUP group .text

_Go32_Info_Block struct
	dd ?,?,?
linear_address_of_transfer_buffer dd ?
	dd ?,?
	db ?,?
	dw ?
	dd ?
linear_address_of_original_psp dd ?
	dw ?,?
_Go32_Info_Block ends

	public c _go32_info_block
	public errno

	.data

_go32_info_block _Go32_Info_Block <>
errno dd 0

	.code

outp proc c port:word, value:byte
	mov dx,port
	mov al,value
	out dx,al
	ret
outp endp

outpw proc c port:word, value:word
	mov dx,port
	mov ax,value
	out dx,ax
	ret
outpw endp

inp proc c port:word
	mov dx,port
	in  al,dx
	ret
inp endp

inpw proc c port:word
	mov dx,port
	in  ax,dx
	ret
inpw endp

;--- memset: it's syscall because OW's stack-based convention has no leading underscore!

memset proc syscall uses edi dst:ptr, value:byte, size_:dword
	cld
	mov edi, dst
	mov al,value
	mov ecx, size_
	rep stosd
	ret
memset endp

	end

