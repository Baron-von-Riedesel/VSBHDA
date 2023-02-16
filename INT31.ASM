
;--- ISR handling
;--- note that jwasm's coff output isn't fully compatible with DJGPP's LD
;--- that's why all procs must be in their own source file


	.386
	.MODEL small, c
	option casemap:none
	option proc:private

DPMI_ISR_HANDLE struct 4
dwOfsPM	dd ?	; old sound ISR
wSegPM	dw ?	; old sound ISR
wOfsRM	dw ?
wSegRM	dw ?
intno	db ?
dwISR	dd ?
dwStack	dd ?
dwDS    dd ?
DPMI_ISR_HANDLE ends

;--- using a struct for global variables is another quirk
;--- required to make LD address them correctly.

XXX struct
oldint31 df ?
currsndint df ?	; current vector for sound ISR
dsseg dw ?
intno db ?
XXX ends

	.data

vars XXX <?,?,?,?>

	.code

	assume ds:_DATA

myint31 proc

	cmp ax, 204h
	jz is204
	cmp ax, 205h
	jz is205
prevint:
	jmp cs:[vars.oldint31]
is204:
is205:
	cmp bl, cs:[vars.intno]
	jnz prevint
	cmp al, 5
	jz is05
	mov edx, dword ptr cs:[vars.currsndint+0]
	mov cx, word ptr cs:[vars.currsndint+4]
	and byte ptr [esp+2*4], 0feh
	iretd
is05:
	push eax
	push ds
	mov ds, cs:[vars.dsseg]
	mov dword ptr [vars.currsndint+0], edx
	mov word ptr [vars.currsndint+4], cx
	pop ds
	pop eax
	and byte ptr [esp+2*4], 0feh
	iretd

myint31 endp

;--- install int 31h handler

_hdpmi_InstallInt31 proc public uses ebx handle:dword

;--- hook into DPMI int 31h
	mov bl, 31h
	mov ax, 204h
	int 31h
	jc error
	mov dword ptr vars.oldint31+0, edx
	mov word ptr vars.oldint31+4, cx
	mov ecx, cs
	mov edx, offset myint31
	mov ax, 205h
	int 31h
	jc error

	mov eax, handle
	mov edx, [eax].DPMI_ISR_HANDLE.dwOfsPM
	mov cx, [eax].DPMI_ISR_HANDLE.wSegPM
	mov bl, [eax].DPMI_ISR_HANDLE.intno
	mov dword ptr [vars.currsndint+0], edx
	mov word ptr [vars.currsndint+4], cx
	mov [vars.intno], bl
	mov [vars.dsseg], ds
	xor eax, eax
	ret
error:
	or eax, -1
	ret
_hdpmi_InstallInt31 endp

_hdpmi_UninstallInt31 proc public uses ebx

	mov edx, dword ptr [vars.oldint31+0]
	mov cx, word ptr [vars.oldint31+4]
	mov bl, [vars.intno]
	mov ax, 205h
	int 31h
	jc error
	xor eax, eax
	ret
error:
	or eax, -1
	ret
_hdpmi_UninstallInt31 endp

	END
