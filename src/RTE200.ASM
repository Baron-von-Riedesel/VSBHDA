
;--- for 16-bit: hack to skip Runtime Error 200 of Borland Powerpack

	.386
	.MODEL small
	option casemap:none
	option proc:private

	include config.inc

if FIXEXC00

	.code

	public c dwOldexc00

dwOldexc00 PFAR 0

myexc00 proc
	push es
	push ebx
	mov es, [esp+2*4].EXCFRAME._cs
	movzx ebx, [esp+2*4].EXCFRAME._ip
	cmp word ptr es:[ebx], 0F1F7h	; F7 F1: "div cx"
	jnz oldexc
if 0 ; no need to check the "last instruction" - just test current CX
	cmp bx, 3
	jb oldexc
	cmp word ptr es:[ebx-3], 037b9h	; B9 37 00: "mov cx, 0037h"
	jnz oldexc
	cmp byte ptr es:[ebx-1], 0
	jnz oldexc
else
	cmp cx,37h
	jnz oldexc
endif
	mov dx, 36h	; anthing below 37h should do
	pop ebx
	pop es
	retw
oldexc:
	pop ebx
	pop es
	jmp cs:[dwOldexc00]

myexc00 endp

InstallExc00 proc c public uses ebx

	mov bl, 0
	mov ax, 202h
	int 31h
	push ds
	mov ds, cs:[_csalias]
	assume ds:_TEXT
	mov word ptr dwOldexc00+0, dx
	mov word ptr dwOldexc00+2, cx
	pop ds
	assume ds:DGROUP
	mov edx, offset myexc00
	mov ecx, cs
	mov ax, 203h
	int 31h
	ret
InstallExc00 endp

UninstallExc00 proc c public uses ebx

	mov dx, word ptr dwOldexc00+0
	mov cx, word ptr dwOldexc00+2
	mov bl, 0
	mov ax, 203h
	int 31h
	ret

UninstallExc00 endp

endif

	END
