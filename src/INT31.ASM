
;--- Int 31h/21h hook; purposes:
;--- 1. to hide the sound hw IRQ from client
;--- 2. clear ISR_DMA_MappedAddr at DOS exit, since it will be released by hdpmi

	.386
ifdef ?FLAT
	.MODEL flat, c
else
	.MODEL small, c
endif
	option casemap:none
	option proc:private

externdef c ISR_DMA_MappedAddr:dword

ifdef NOTFLAT

InstallExc00 proto 
UninstallExc00 proto 

externdef c _csalias:dword

	.code

SEGOFS equ 2
IRETFLOFS equ 2*2
insIRET textequ <IRET>

externdef dfOldISR:dword	; in _TEXT for 16-bit
externdef oldexc00:dword	; in _TEXT for 16-bit

PF16 typedef ptr far16
oldint31 PF16 ?
oldint21 PF16 ?
currsndint dd ?	; current vector for sound ISR

dsseg dw ?
intno db ?

else

	assume ds:_DATA

	.data

SEGOFS equ 4
IRETFLOFS equ 2*4
insIRET textequ <IRETD>

externdef dfOldISR:fword

oldint31 df ?
	dw ?
oldint21 df ?
	dw ?
currsndint df ?	; current vector for sound ISR
	dw ?

dsseg dw ?
intno db ?

	.code

endif

myint31 proc

ifdef NOTFLAT
	cmp ax, 202h
	jz is202
	cmp ax, 203h
	jz is203
endif
	cmp ax, 204h
	jz is204
	cmp ax, 205h
	jz is205
prevint:
	jmp cs:[oldint31]
is204:
is205:
	cmp bl, cs:[intno]
	jnz prevint
	cmp al, 5
	jz is05
ifdef NOTFLAT
	mov dx, word ptr cs:[currsndint+0]
else
	mov edx, dword ptr cs:[currsndint+0]
endif
	mov cx, word ptr cs:[currsndint+SEGOFS]
	and byte ptr [esp+IRETFLOFS], 0feh
	insIRET
is05:
	push eax
	push ds
ifdef NOTFLAT
	mov ds, cs:[_csalias]
	assume ds:_TEXT
else
	mov ds, cs:[dsseg]
endif
	mov dword ptr [currsndint+0], edx
	mov word ptr [currsndint+SEGOFS], cx
	pop ds
ifdef NOTFLAT
	assume ds:DGROUP
endif
	pop eax
	and byte ptr [esp+IRETFLOFS], 0feh
	insIRET
ifdef NOTFLAT
is202:
	cmp bl,0
	jnz prevint
	mov dx, word ptr cs:[oldexc00+0]
	mov cx, word ptr cs:[oldexc00+2]
	insIRET
is203:
	cmp bl,0
	jnz prevint
	push ds
	mov ds, cs:[_csalias]
	assume ds:_TEXT
	mov word ptr [oldexc00+0], dx
	mov word ptr [oldexc00+2], cx
	pop ds
	assume ds:DGROUP
	insIRET
endif
	align 4
myint31 endp

myint21 proc

	cmp ah, 25h
	jz is25
	cmp ah, 35h
	jz is35
	cmp ah, 4Ch
	jz is4C
prevint:
	jmp cs:[oldint21]
is4C:
	push ds
	mov ds, cs:[dsseg]
	mov [ISR_DMA_MappedAddr], 0
	pop ds
	jmp prevint
is25:
is35:
	cmp al, cs:[intno]
	jnz prevint
	cmp ah, 25h
	jz isset
ifdef NOTFLAT
	mov bx, word ptr cs:[currsndint+0]
else
	mov ebx, dword ptr cs:[currsndint+0]
endif
	mov es, word ptr cs:[currsndint+SEGOFS]
	and byte ptr [esp+IRETFLOFS], 0feh
	insIRET
isset:
	push eax
	push ds
ifdef NOTFLAT
	mov ds, cs:[_csalias]
	assume ds:_TEXT
else
	mov ds, cs:[dsseg]
endif
	mov dword ptr [currsndint+0], edx
	mov word ptr [currsndint+SEGOFS], cx
	pop ds
ifdef NOTFLAT
	assume ds:DGROUP
endif
	pop eax
	and byte ptr [esp+IRETFLOFS], 0feh
	insIRET
	align 4
myint21 endp

;--- install int 31h handler

_hdpmi_InstallInt31 proc public uses ebx interrupt:byte

ifdef NOTFLAT
	call InstallExc00
	push ds
	mov ds, cs:[_csalias]
	assume ds:_TEXT
endif

;--- hook into DPMI int 31h
	mov bl, 31h
	mov ax, 204h
	int 31h
	jc error
	mov dword ptr oldint31+0, edx
	mov word ptr oldint31+SEGOFS, cx
	mov ecx, cs
	mov edx, offset myint31
	mov ax, 205h
	int 31h
	jc error

	mov bl, 21h
	mov ax, 204h
	int 31h
	jc error
	mov dword ptr oldint21+0, edx
	mov word ptr oldint21+SEGOFS, cx
	mov ecx, cs
	mov edx, offset myint21
	mov ax, 205h
	int 31h
	jc error

	mov bl, interrupt
	mov edx, dword ptr dfOldISR+0
	mov cx, word ptr dfOldISR+SEGOFS
	mov dword ptr [currsndint+0], edx
	mov word ptr [currsndint+SEGOFS], cx
	mov [intno], bl
	mov [dsseg], ds
ifdef NOTFLAT
	pop ds
endif
	mov eax, 1
	ret
error:
ifdef NOTFLAT
	pop ds
	assume ds:DGROUP
endif
	xor eax, eax
	ret
_hdpmi_InstallInt31 endp

_hdpmi_UninstallInt31 proc public uses ebx

	mov edx, dword ptr [oldint21+0]
	mov cx, word ptr [oldint21+SEGOFS]
	mov bl, 21h
	mov ax, 205h
	int 31h
	jc error

	mov edx, dword ptr [oldint31+0]
	mov cx, word ptr [oldint31+SEGOFS]
	mov bl, 31h
	mov ax, 205h
	int 31h
	jc error

ifdef NOTFLAT
	call UninstallExc00
endif
	mov eax, 1
	ret
error:
	xor eax, eax
	ret
_hdpmi_UninstallInt31 endp

	END
