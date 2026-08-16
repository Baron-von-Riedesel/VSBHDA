;*****************************************************************************
;*
;*                            Open Watcom Project
;*
;*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
;*
;*  ========================================================================
;*
;*    This file contains Original Code and/or Modifications of Original
;*    Code as defined in and that are subject to the Sybase Open Watcom
;*    Public License version 1.0 (the 'License'). You may not use this file
;*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
;*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
;*    provided with the Original Code and Modifications, and is also
;*    available at www.sybase.com/developer/opensource.
;*
;*    The Original Code and all software distributed under the License are
;*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
;*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
;*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
;*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
;*    NON-INFRINGEMENT. Please see the License for the specific language
;*    governing rights and limitations under the License.
;*
;*  ========================================================================
;*
;* Description:  DOS 32-bit startup code for HX.
;*
;*****************************************************************************

	name cstart

	.386
	.model flat
	.dosseg

PEHDR struct
sig                 dd ?
fhdr                dd 5 dup (?)
                    dd ?    ; magic, linker version
codesize            dd ?
initdatasize        dd ?
bsssize             dd ?
entry               dd ?
codebaserva         dd ?
databaserva         dd ?
imagebaserva        dd ?
SectionAlignment    dd ?
alignment           dd ?
                    dd ?,?,?,?
imagesize           dd ?
headersize          dd ?
                    dd ?
                    dd ?
stacksize_rsvd      dd ?
stacksize_commit    dd ?
PEHDR ends

NO87 equ 0
LFN  equ 0

DOS_PSP_ENV_SEG equ 2Ch
if NO87
FLG_NO87        equ 1
endif
if LFN
FLG_LFN         equ 1
endif

main proto c :dword, :ptr

	externdef c _argc:dword
	externdef c _argv:ptr

;	externdef __CMain : near
	externdef __InitRtns : near
	externdef __FiniRtns : near
;	externdef __DOSseg__ : near

	externdef _edata : byte		; end of DATA (start of BSS)
	externdef _end : byte		; end of BSS (but NOT start of STACK with LOADPE)

	externdef c _dynend : dword
	externdef c _curbrk : dword
	externdef c _psp : word
	externdef c _osmajor : byte
	externdef c _osminor : byte
	externdef c _STACKLOW : dword
	externdef c _STACKTOP : dword
if NO87
	externdef __no87 : byte
endif
if LFN
	externdef c __uselfn : byte
endif
	externdef c _Extender : byte
	externdef c _ExtenderSubtype : byte
	externdef c _Envptr : fword
;	externdef c _Envseg : word
;	externdef c __FPE_handler : dword
	externdef c _LpCmdLine : dword
	externdef c _LpPgmName : dword

	public __do_exit_with_msg_
	public __do_exit_with_msg__	; needed for OW v1.9

;	externdef c __GETDSStart_ : near
;	externdef c __GETDSEnd_ : near


FINI_PRIORITY_EXIT equ 16

;BEGTEXT  segment flat para public 'CODE'
;BEGTEXT  ends

_NULL   segment para public 'BEGDATA'
__nullarea label word
	db 01h,01h,01h,00h
	public __nullarea
_NULL   ends

_AFTERNULL segment dword public 'BEGDATA'
_AFTERNULL ends

_DATA segment
__saved_DS	dd 0	; saved DS for interrupt routines
_DATA ends

DATA    segment dword public 'DATA'
DATA    ends
if 0
TIB     segment byte public 'DATA'
TIB     ends
TI      segment byte public 'DATA'
TI      ends
TIE     segment byte public 'DATA'
TIE     ends
XIB     segment word public 'DATA'
XIB     ends
XI      segment word public 'DATA'
XI      ends
XIE     segment word public 'DATA'
XIE     ends
YIB     segment word public 'DATA'
YIB     ends
YI      segment word public 'DATA'
YI      ends
YIE     segment word public 'DATA'
YIE     ends
endif

;DGROUP group _NULL,_AFTERNULL,_DATA,DATA,TIB,TI,TIE,XIB,XI,XIE,YIB,YI,YIE
DGROUP group _NULL,_AFTERNULL,_DATA,DATA

	.const

ConsoleName     db      "con",0
NewLine         db      0Dh,0Ah
szMSDOS         db      "MS-DOS",0
szNoDosExt      db      "No DOS-Extender",13,10,0

	.code

_cstart_ proc

;--- hx: esi=linear address module
;---     ebx=linear address psp (loadpe only!)

	push esi
	add esi, [esi+3ch]		; skip MZ header
	mov ebp, [esi].PEHDR.stacksize_rsvd
	pop esi

	mov eax, esp
	add eax, 4096-1
	and ax, 0f000h
	mov _STACKTOP, eax
	mov _curbrk, eax
	sub eax, ebp
	mov _STACKLOW, eax
	xor ebp, ebp

	mov ax,168ah
	mov esi,offset szMSDOS
	int 2Fh
	cmp al,0
	jz dosext
	mov esi,offset szNoDOSExt	; __do_exit_with_msg() cannot be used without DOS extender!
@@:
	lodsb
	and al,al
	jz @F
	mov dl,al
	mov ah,2
	int 21h
	jmp @B
@@:
	mov ax,4C01h
	int 21h
dosext:
;
;       get DOS & Extender version number
;
	mov ah,30h
	int 21h 				; modifies eax,ebx,ecx,edx
	mov _osmajor,al
	mov _osminor,ah
	mov ecx,eax 			; remember DOS version number
	mov ah, 51h 			; get PSP selector - requires DOS extender
	int 21h
	mov _psp, bx
	push ds
	mov ds, ebx
	mov bx,ds:[DOS_PSP_ENV_SEG] 	; get environment selector
	pop ds
;	mov al,X_HX
;	mov al,X_RATIONAL
	mov al,0
	mov ah,0				; meaning "zero-based"
	mov _Extender,al		; record extender type
	mov _ExtenderSubtype,ah ; record extender subtype
	mov __saved_DS,ds		; save DS value
	mov ax,6
	int 31h
	mov word ptr [_Envptr+0], dx
	mov word ptr [_Envptr+2], cx
;
;--- convert cmdline to asciiz format
;
	mov bx,_psp
	mov ax, 6
	int 31h
	push cx
	push dx
	pop eax
	mov edi,81H 			; DOS command buffer es:edi
	add edi, eax
	push ds
	pop es
	movzx ecx,byte ptr [edi-1]
	sub al,al
	mov [edi+ecx],al
;
;--- get pgmname in environment
;
	cld
	mov esi,dword ptr [_Envptr]
if LFN
	mov bx,FLG_LFN*256		; assume 'lfn=n' env. var. not present / assume 'no87=' env. var. not present
elseif NO87
	xor bx,bx
endif
L1:
if NO87 or LFN
	mov eax,[esi]			; get first 4 characters
	or eax,20202020h		; map to lower case
endif
if NO87
	cmp eax,37386f6eh		; check for "no87"
	jne L2					; skip if not "no87"
	cmp byte ptr 4[esi],'=' ; make sure next char is "="
	jne L4					; no
	or bl,FLG_NO87 			; - indicate 'no87' was present
	jmp L4
L2:
endif
if LFN
	cmp eax,3d6e666ch		; check for 'lfn='
	jne L4					; skip if not 'lfn='
	mov al,byte ptr 4[esi]	; get next character
	or al,20h				; map to lower case
	cmp al,'n'				; make sure next char is 'n'
	jne L4					; no
	and bh,not FLG_LFN		; indicate no 'lfn=n' present
endif
L4:
	cmp byte ptr [esi],0	; end of string ?
	lodsb
	jne L4					; until end of string
	cmp byte ptr [esi],0	; end of all strings ?
	jne L1					; if not, then skip next string
	lodsb
	inc esi 				; point to program name
	inc esi 				; . . .

if NO87
	mov __no87,bl			; set state of "no87" enironment var
endif
if LFN
	and __uselfn,bh 		; set "LFN" support status
endif
	mov _dynend,esp 		; set top of dynamic memory area
	mov _LpCmdLine,edi		; save command line address
	mov _LpPgmName,esi		; save program name address
;
;--- clear _BSS section
;
	mov ecx,offset _end 	; end of _BSS segment (start of STACK)
	mov edi,offset _edata	; start of _BSS segment
	sub ecx,edi 			; calc # of bytes in _BSS segment
	mov dl,cl				; save bottom 2 bits of count in edx
	shr ecx,2				; calc # of dwords
	sub eax,eax
	rep stosd
	mov cl,dl				; get bottom 2 bits of count
	and cl,3
	rep stosb

	mov eax,0FFH			; run all initializers
	call __InitRtns			; call initializer routines
	push _argv
	push _argc
	call main
_cstart_ endp ; fall thru

__exit proc

ifndef __STACK__
	push eax 				; return code in eax
endif
	jmp short L7

; input: ( char *msg, int rc )  always in registers

__do_exit_with_msg__::
__do_exit_with_msg_::
	push edx 				; save return code
	push eax 				; save address of msg
	mov edx,offset ConsoleName
	mov ax, 3d01h			; write-only access to screen
	int 21h
	mov ebx,eax 			; get file handle
	pop edx 				; restore address of msg
	mov esi,edx 			; get address of msg
	cld 					; make sure direction forward
L6:
	lodsb					; get char
	cmp al,0
	jne L6
	mov ecx,esi 			; calc length of string
	sub ecx,edx
	dec ecx
	mov ah, 40h
	int 21h
	mov edx,offset NewLine	; write out the string
	mov cl,sizeof NewLine
	mov ah, 40h
	int 21h
L7:
	xor eax, eax
	mov edx,FINI_PRIORITY_EXIT-1; less than exit
	call __FiniRtns			; call finalizer routines
	pop eax 				; restore return code
	mov ah, 4cH
	int 21h
__exit endp

	align 4

__GETDS proc
;__GETDSStart_ label near
	mov ds,cs:__saved_DS	; load saved DS value
	ret
__GETDS endp
;__GETDSEnd_ label near

	end _cstart_
