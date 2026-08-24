
;--- DOS 32-bit dll startup code for 16-bit client
;--- to be assembled with "jwasm -zcw dstrt16x.asm"

	name dllstart

	.386
	.model small
	.dosseg

LFN equ 0
EXIT equ 0
EXITWITHMSG equ 0

FINI_PRIORITY_EXIT equ 16

X_RATIONAL    equ 1
X_PHARLAP_V1  equ 2

XS_RATIONAL_NONZEROBASE equ 1
XS_PHARLAP_NORMAL       equ 0

DOS_PSP_ENV_SEG equ 2Ch
DEFINECRTVARS   equ 1

NO87 equ 0
LFN  equ 0

if NO87
FLG_NO87        equ     1
endif
if LFN
FLG_LFN         equ     1
endif

	externdef c _DSBase          : dword
	externdef c __linear_rmstack : dword
	externdef c _LibMain         : near
	externdef _edata             : near  ; end of DATA (start of BSS)
	externdef _end               : near  ; end of BSS (start of STACK)
	externdef __InitRtns         : near
	externdef __FiniRtns         : near
;	extern    __DOSseg__         : near
ifndef OW19
	externdef __child            : dword
endif
if NO87
	externdef __no87             : byte
endif
	externdef c  _dynend            : dword
	externdef c  _curbrk            : dword
;	externdef c  _psp               : word
;	externdef c  _osmajor           : byte
;	externdef c  _osminor           : byte
;	externdef c  _STACKLOW          : dword
;	externdef c  _STACKTOP          : dword
if LFN
	externdef c  __uselfn           : byte
endif
;	externdef c  _Extender          : byte
;	externdef c  _ExtenderSubtype   : byte
;	externdef c  _Envptr            : fword
;	extern c  __FPE_handler      : dword

; this guarantees that no function pointer will equal NULL
; (WLINK will keep segment 'BEGTEXT' in front)
; This segment must be at least 4 bytes in size to avoid confusing the
; signal function.

BEGTEXT segment para public 'CODE'

forever label   near
        int     3h
        jmp     forever
___begtext label byte
        nop     ;3
        nop     ;4
        nop     ;5
        nop     ;6
        nop     ;7
        nop     ;8
        nop     ;9
        nop     ;A
        nop     ;B
        nop     ;C
        nop     ;D
        nop     ;E
        nop     ;F
;        public ___begtext

BEGTEXT  ends

_NULL   segment para public 'BEGDATA'
__nullarea label word
        db      01h,01h,01h,00h
        public  __nullarea
_NULL   ends

_AFTERNULL segment word public 'BEGDATA'
_AFTERNULL ends

_DATA    segment dword public 'DATA'
if 0
	public __x386_zero_base_selector
;--- this is referenced in inirmsel.c!
__x386_zero_base_selector dw 0  ; base 0 selector for X-32VM
endif
if DEFINECRTVARS
_dynend    dd 0
_curbrk    dd 0
_STACKLOW  dd 0
_STACKTOP  dd 0
_LpCmdLine dd 0
_LpPgmName dd 0
_psp       dw 0
_osmajor   db 0
_osminor   db 0
_Extender  db 0
_ExtenderSubtype db 0
endif
_DATA    ends

DATA    segment dword public 'DATA'
DATA    ends

CONST   segment word public 'DATA'
CONST   ends
CONST2  segment word public 'DATA'
CONST2  ends

if 0
TIB segment byte public 'DATA'
TIB ends
TI  segment byte public 'DATA'
TI  ends
TIE segment byte public 'DATA'
TIE ends

XIB segment word public 'DATA'
XIB ends
XI  segment word public 'DATA'
XI  ends
XIE segment word public 'DATA'
XIE ends

YIB segment word public 'DATA'
YIB ends
YI  segment word public 'DATA'
YI  ends
YIE segment word public 'DATA'
YIE ends
endif

	.data?

;--- don't use .stack in 32-bit (aligns to dword)
STACK   segment para stack 'STACK'
        db 4000h dup(?)
STACK   ends

;DGROUP group _NULL,_AFTERNULL,CONST,CONST2,_DATA,DATA,TIB,TI,TIE,XIB,XI,XIE,YIB,YI,YIE,_BSS,STACK
DGROUP group _NULL,_AFTERNULL,_DATA,DATA,CONST,CONST2,_BSS,STACK

	assume ds:DGROUP

CONST segment

externdef ExpTable:ptr

if EXITWITHMSG
ConsoleName     db      "con",0
NewLine         db      0Dh,0Ah
endif

CONST ends

ifdef _DEBUG
CStr macro text:vararg
local sym
CONST segment
sym db text,0
CONST ends
	exitm <offset sym>
endm
__dprintf proto c :ptr, :vararg
endif

	.code

__DLLstart_ proc far c uses ebx esi edi psp:dword, heapsize:dword, rmstack;dword

	jmp @F

	dw DGROUP		; the one and only segment relocation in the module
	align 4
	dd ___begtext	; make sure dead code elimination
@@:
	mov eax, psp
	mov _psp, ax
	lea eax, [ebp+3*4+3*4+4]	; calc top of stack (frame, arguments, saved ss:sp)
	mov _STACKTOP, eax
	mov _STACKLOW, offset _end
	add eax, heapsize
	mov _curbrk, eax
	mov ecx, rmstack
	mov __linear_rmstack, ecx

	mov ebx, ds
	mov ax, 0006h
	int 31h
	mov word ptr _DSBase+0, dx
	mov word ptr _DSBase+2, cx

;--- get DOS & Extender version number
	mov ah,30h
	int 21h 				; modifies eax,ebx,ecx,edx
	mov _osmajor,al
	mov _osminor,ah

	mov al, X_RATIONAL
	mov ah, XS_RATIONAL_NONZEROBASE
	mov _Extender,al			; record extender type
	mov _ExtenderSubtype,ah 	; record extender subtype

;	mov bx,FLG_LFN*256			; assume 'lfn=n' env. var. not present / assume 'no87=' env. var. not present
if NO87
	mov __no87,0				; set state of "no87" environment var
endif
if LFN
	mov __uselfn,0				; set "LFN" support status
endif
	mov ebx, _STACKTOP
	mov _dynend,ebx 			; set top of dynamic memory area
	mov eax,0FFH				; run all initializers
	call __InitRtns				; call initializer routines
	call _LibMain
	mov eax, offset ExpTable
	ret
__DLLstart_ endp

if EXIT
;--- don't touch AL in __exit, it has the return code

 ifdef FC
EXITCC equ <fastcall>
 else
EXITCC equ <>
 endif

;	public EXITCC __exit

__exit  proc EXITCC

 ifndef __STACK__
	push eax			; get return code into eax
 endif
 if EXITWITHMSG
	jmp L7

	public  __do_exit_with_msg_

; input: ( char *msg, int rc )  always in registers

__do_exit_with_msg_::
	push edx 				; save return code
	push eax 				; save address of msg
	mov edx, offset ConsoleName
	mov ax, 3d01h			; write-only access to screen
	int 21h
	mov ebx,eax 			; get file handle
	pop edx 				; restore address of msg
	mov esi,edx 			; get address of msg
	cld 					; make sure direction forward
@@:
	lodsb
	cmp al,0
	jne @B
	mov ecx,esi 			; calc length of string
	sub ecx,edx
	dec ecx
	mov ah, 40h
	int 21h
	mov edx,offset NewLine
	mov ecx,sizeof NewLine
	mov ah, 40h
	int 21h
L7:
 endif
	xor eax, eax
	mov edx,FINI_PRIORITY_EXIT - 1
	call __FiniRtns			; call finalizer routines

	pop eax 				; restore return code
	mov ah, 4cH 			; DOS call to exit with return code
	int 21h
__exit endp
endif

	align 4

;	public __GETDS
;	public c __GETDSStart_
;	public c __GETDSEnd_

__GETDS proc
;__GETDSStart_ label near
	push eax
	mov eax,cs
	add eax,8
	mov ds,eax
	pop eax
	ret
;	align dword
__GETDS endp
;__GETDSEnd_ label near

	end __DLLstart_
