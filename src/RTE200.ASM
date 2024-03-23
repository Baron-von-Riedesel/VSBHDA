
;--- for 16-bit: hack to skip Runtime Error 200 of Borland Powerpack

	.386
	.MODEL small

	option casemap:none
	option proc:private

EXCFRAME struct	;DPMI v0.9 exception frame
_dpmiip    dw ?
_dpmics    dw ?
errorcode  dw ?
_ip        dw ?
_cs        dw ?
_flags     dw ?
_sp        dw ?
_ss        dw ?
EXCFRAME ends

externdef c _csalias:dword

	.code

	public c oldexc00

PF16 typedef ptr far16
oldexc00 PF16 0

myexc00 proc
	push es
	push ebx
	mov es, [esp+2*4].EXCFRAME._cs
	movzx ebx, [esp+2*4].EXCFRAME._ip
	cmp word ptr es:[ebx], 0F1F7h	; F7 F1: "div cx"
	jnz oldexc
	cmp bx, 3
	jb oldexc
	cmp word ptr es:[ebx-3], 037b9h	; B9 37 00: "mov cx, 0037"
	jnz oldexc
	cmp byte ptr es:[ebx-1], 0
	jnz oldexc
	mov dx, 35h	; anthing below 37h should do
	pop ebx
	pop es
	retw
oldexc:
	pop ebx
	pop es
	jmp cs:[oldexc00]

myexc00 endp

InstallExc00 proc c public uses ebx

	mov bl, 0
	mov ax, 202h
	int 31h
	push ds
	mov ds, cs:[_csalias]
	assume ds:_TEXT
	mov word ptr oldexc00+0, dx
	mov word ptr oldexc00+2, cx
	pop ds
	assume ds:DGROUP
	mov edx, offset myexc00
	mov ecx, cs
	mov ax, 203h
	int 31h
	ret
InstallExc00 endp

UninstallExc00 proc c public uses ebx

	mov dx, word ptr oldexc00+0
	mov cx, word ptr oldexc00+2
	mov bl, 0
	mov ax, 203h
	int 31h
	ret

UninstallExc00 endp

	END
