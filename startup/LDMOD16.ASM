
;*** for 16-bit variant: load and setup a mz binary
;--- the binary is assumed to run in protected-mode,
;--- code 32-bit. Both cs and ss segments are <= 64kB.

	.386
	.model small

	include debug.inc

HEAPSIZE equ 1000h	;additional memory behind stack

IMAGE_DOS_HEADER STRUCT
  e_magic           WORD      ?	;+0
  e_cblp            WORD      ?	;+2  bytes last page
  e_cp              WORD      ?	;+4  page cnt
  e_crlc            WORD      ?	;+6  number of relocation records
  e_cparhdr         WORD      ?	;+8  size header
  e_minalloc        WORD      ?	;+A 
  e_maxalloc        WORD      ?	;+C 
  e_ss              WORD      ?	;+E 
  e_sp              WORD      ?	;+10
  e_csum            WORD      ?	;+12
  e_ip              WORD      ?	;+14
  e_cs              WORD      ?	;+16
  e_lfarlc          WORD      ?	;+18	begin relocation records
IMAGE_DOS_HEADER ENDS

LDPARMS struct
wIP		dw ?
wCS		dw ?
wSP		dw ?
wSS		dw ?
if HEAPSIZE
dwHeap  dd ?
endif
LDPARMS ends

	.code

_LoadMod16 proc c uses ebx esi edi pName:ptr, pParms:ptr

local wDgroup:word
local wFile:word
local wCodeSel:word
local dwSizeAllData:dword	; data, bss, stack
local dwSizeCode:dword
local dwSizeData:dword	; initialized data
local dwBaseCode:dword
local dwBaseData:dword
local reloc:dword
local mzhdr:IMAGE_DOS_HEADER

	@dprintf CStr("LoadMod16(%s) enter",10), pName
	mov edx, pName
	mov ah, 3Dh
	int 21h
	jc open_err
	mov wFile, ax
	mov ebx, eax
	mov cx, sizeof mzhdr
	lea edx, mzhdr
	mov ah, 3Fh
	int 21h
	jc read_err
	cmp ax, cx
	jnz read_err
	cmp mzhdr.e_magic, "ZM"
	jnz fmt_err
	cmp mzhdr.e_crlc, 1		; just one DGROUP ref allowed!x
	jnz fmt_err

;--- read the relocation
	mov dx, mzhdr.e_lfarlc
	mov cx, 0
	mov ax, 4200h
	int 21h
	jc seek_err
	mov cx, 4
	lea dx, reloc
	mov ah, 3Fh
	int 21h
	jc read_err

;--- and now read the word at the relocation address
;--- this is the start of DGROUP
	movzx eax, word ptr reloc+2
	shl eax, 4
	movzx edx, word ptr reloc+0
	add eax, edx
	mov dx, mzhdr.e_cparhdr	; reloc addresses are "relative", so size of header must be added
	shl edx, 4
	add eax, edx
	push eax
	pop dx
	pop cx
	mov ax, 4200h
	int 21h
	jc seek_err
	lea dx, wDgroup
	mov cx, 2
	mov ah, 3Fh
	int 21h
	jc read_err
	movzx eax, wDgroup
	shl eax, 4
	mov dwSizeCode, eax

	movzx eax, mzhdr.e_cp
	dec eax
	shl eax, 9				; a page is 200h bytes
	movzx edx, mzhdr.e_cblp
	add eax, edx
	movzx edx, mzhdr.e_cparhdr
	shl edx, 4
	sub eax, edx			; eax=size initialized data
	sub eax, dwSizeCode
	mov dwSizeData, eax
	movzx edx, mzhdr.e_minalloc
	shl edx, 4
	add eax, edx
	add eax, 4-1	; ensure the size is dword aligned
	and al, 0fch
	mov dwSizeAllData, eax

;--- alloc code
	mov cx, word ptr [dwSizeCode+0]
	mov bx, word ptr [dwSizeCode+2]
	mov ax,501h
	int 31h
	jc nomem
	mov word ptr dwBaseCode+0, cx
	mov word ptr dwBaseCode+2, bx
;--- alloc data
	mov eax, dwSizeAllData
if HEAPSIZE
	add eax, HEAPSIZE
endif
	push eax
	pop cx
	pop bx
	mov ax,501h
	int 31h
	jc nomem
	mov word ptr dwBaseData+0, cx
	mov word ptr dwBaseData+2, bx

	mov cx,2
	xor ax, ax
	int 31h
	jc nodesc
	mov wCodeSel, ax
	mov ebx, eax
	mov dx, word ptr dwBaseCode+0
	mov cx, word ptr dwBaseCode+2
	mov ax, 7
	int 31h
	mov eax, dwSizeCode
	dec eax
	push eax
	pop dx
	pop cx
	mov ax, 8
	int 31h
	jc nodesc

	add bx,8
	mov dx, word ptr dwBaseData+0
	mov cx, word ptr dwBaseData+2
	mov ax, 7
	int 31h
if 0
	mov eax, dwSizeAllData
	dec eax
	push eax
	pop dx
	pop cx
else
	or dx, -1
	or cx, -1
endif
	mov ax, 8
	int 31h

;--- read code

	mov bx, wFile
	mov dx, mzhdr.e_cparhdr
	shl dx, 4
	xor cx, cx
	mov ax, 4200h
	int 21h
	jc seek_err
	push ds
	mov ds, wCodeSel
	xor dx, dx
	mov cx, word ptr dwSizeCode
	mov ax, 3F00h
	int 21h
	jc read_err

;--- read data, init bss+stack
	mov eax, ds
	add eax, 8
	mov ds, eax
	mov ecx, dwSizeData
	mov eax, dwSizeAllData
	sub eax, ecx
	push eax
	mov ax, 3F00h
	int 21h
	jc read_err
	mov edi, ecx
	pop ecx
	push es
	push ds
	pop es
	mov al, 0
	rep stosb
	pop es
	pop ds

	mov ah, 3Eh
	int 21h

;--- set code descriptor to code, 32-bit
	mov bx, wCodeSel
	mov ecx, cs
	lar ecx, ecx
	shr ecx, 8
	or ch, 40h
	mov ax, 9
	int 31h

	mov ax, mzhdr.e_ip
	mov ecx, dwSizeAllData
	mov edx, pParms
	mov [edx].LDPARMS.wIP, ax
	mov [edx].LDPARMS.wCS, bx
	mov [edx].LDPARMS.wSP, cx
	add ebx, 8
	mov [edx].LDPARMS.wSS, bx
if HEAPSIZE
	mov [edx].LDPARMS.dwHeap, HEAPSIZE
endif
	mov eax, 1
	ret

if 1
nomem:
nodesc:
	mov ax, 109
	jmp err_with_close
fmt_err:
	mov ax, 108
seek_err:
read_err:
err_with_close:
	push eax
	mov bx, wFile
	mov ah, 3Eh
	int 21h
	pop eax
open_err:
	@dprintf CStr("LoadMod16 failed, ax=%X",10), ax
	mov edx, eax
	xor eax, eax
	ret
else
	.const

errstr1 db "open error",13,10,'$'
errstr2 db "read error",13,10,'$'
errstr3 db "no descriptors",13,10,'$'
errstr4 db "no extended memory",13,10,'$'
errstr5 db "format error",13,10,'$'
errstr6 db "seek error",13,10,'$'

	.code

open_err:
	push offset errstr1
	jmp errexit
seek_err:
	push offset errstr6
	jmp err_with_close
read_err:
	push offset errstr2
	jmp err_with_close
nodesc:
	push offset errstr3
	jmp err_with_close
nomem:
	push offset errstr4
	jmp err_with_close
fmt_err:
	push offset errstr5
errexit2:
	push eax
	mov bx, wFile
	mov ah, 3Eh
	int 21h
	pop eax
errexit:
	pop edx
	push eax
	mov ah,09
	int 21h
	pop edx
	xor eax, eax
	ret

endif

_LoadMod16 endp

    end
