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
;* Description:  DOS 32-bit dll startup code for 16-bit client
;*
;*****************************************************************************

;--- to be assembled with "jwasm -zcw dstrt16x.asm"

        name    dllstart

.387
.386p

FINI_PRIORITY_EXIT equ 16

X_RATIONAL    equ 1
X_PHARLAP_V1  equ 2

XS_RATIONAL_NONZEROBASE equ 1
XS_PHARLAP_NORMAL       equ 0

DOS_PSP_ENV_SEG equ 2Ch
FLG_NO87        equ     1
FLG_LFN         equ     1

@cextrn macro x,y
ifdef __JWASM__
extern c x:y
else
extrn "C",x:y
endif
endm

@cpublic macro x
ifdef __JWASM__
public c x
else
public "C",x
endif
endm

        externdef c _DSBase          : dword
        externdef c __linear_rmstack : dword
        externdef c _LibMain         : near
        extrn   __InitRtns           : near
        extrn   __FiniRtns           : near
        extrn   __DOSseg__           : near

        extrn   _edata               : byte  ; end of DATA (start of BSS)
        extrn   _end                 : byte  ; end of BSS (start of STACK)

        @cextrn   _dynend            , dword
        @cextrn   _curbrk            , dword
        @cextrn   _psp               , word
        @cextrn   _osmajor           , byte
        @cextrn   _osminor           , byte
        @cextrn   _STACKLOW          , dword
        @cextrn   _STACKTOP          , dword
        extrn    __child             : dword
        extrn    __no87              : byte
        @cextrn   __uselfn           , byte
        @cextrn   _Extender          , byte
        @cextrn   _ExtenderSubtype   , byte
        @cextrn   _Envptr            , dword
        @cextrn   __FPE_handler      , dword

; this guarantees that no function pointer will equal NULL
; (WLINK will keep segment 'BEGTEXT' in front)
; This segment must be at least 4 bytes in size to avoid confusing the
; signal function.

BEGTEXT  segment use32 para public 'CODE'

forever label   near
        int     3h
        jmp     short forever
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

CONST   segment word public 'DATA'
CONST   ends
CONST2  segment word public 'DATA'
CONST2  ends

_DATA    segment dword public 'DATA'
;--- this is referenced in inirmsel.c!
__x386_zero_base_selector dw 0  ; base 0 selector for X-32VM
        public  __x386_zero_base_selector
_DATA    ends

DATA    segment dword public 'DATA'
DATA    ends

TIB     segment byte public 'DATA'
TIB     ends
TI      segment byte public 'DATA'
TI      ends
TIE     segment byte public 'DATA'
TIE     ends
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

_BSS    segment dword public 'BSS'
_BSS    ends

STACK   segment para stack 'STACK'
        db 4000h dup(?)
STACK   ends

DGROUP group _NULL,_AFTERNULL,CONST,CONST2,_DATA,DATA,TIB,TI,TIE,XIB,XI,XIE,YIB,YI,YIE,_BSS,STACK

        assume  ds:DGROUP
        assume  cs:_TEXT

_TEXT   segment use32 dword public 'CODE'
externdef c _AU_init       :near32
externdef c _AU_prestart   :near32
externdef c _AU_start      :near32
externdef c _AU_stop       :near32
externdef c _AU_close      :near32
externdef c _AU_getirq     :near32
externdef c _AU_isirq      :near32
externdef c _AU_getfreq    :near32
externdef c _AU_getshortname  :near32
externdef c _AU_setoutbytes   :near32
externdef c _AU_setrate       :near32
externdef c _AU_setmixer_init :near32
externdef c _AU_setmixer_one  :near32
externdef c _AU_setmixer_outs :near32
externdef c _AU_setmixer_all  :near32
externdef c _AU_writedata     :near32
externdef c _AU_cardbuf_space :near32
_TEXT ends

CONST segment

SCDEXP struct
AU_init     dd ?
AU_prestart dd ?
AU_start    dd ?
AU_stop     dd ?
AU_close    dd ?
AU_getirq   dd ?
AU_isirq    dd ?
AU_getfreq  dd ?
AU_getshortname  dd ?
AU_setoutbytes   dd ?
AU_setrate       dd ?
AU_setmixer_init dd ?
AU_setmixer_one  dd ?
AU_setmixer_outs dd ?
AU_setmixer_all  dd ?
AU_writedata     dd ?
AU_cardbuf_space dd ?
SCDEXP ends

scd SCDEXP <offset _AU_init, offset _AU_prestart, offset _AU_start, _AU_stop, _AU_close, _AU_getirq, _AU_isirq, _AU_getfreq,
            _AU_getshortname, _AU_setoutbytes, _AU_setrate, _AU_setmixer_init, _AU_setmixer_one, 
            _AU_setmixer_outs, _AU_setmixer_all, _AU_writedata, _AU_cardbuf_space>

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


_TEXT segment

;--- entry: ds,ss=dgroup, es=psp
;--- edi: heap size
;--- esi: rmstack

__DLLstart_ proc near public
        jmp around
        dw DGROUP
;
; miscellaneous code-segment messages
;
ConsoleName     db      "con",0
NewLine         db      0Dh,0Ah

        align   4
        dd      ___begtext              ; make sure dead code elimination

around:
        mov _psp, es
        lea eax, [esp+3*4]		; far return & saved ss:sp
        mov _STACKTOP, eax
        mov _STACKLOW, offset _end
        add edi, esp
        mov _curbrk, edi
        mov __linear_rmstack, esi
        xor ebp, ebp

		mov ebx, ds
		mov ax, 0006h
		int 31h
		mov word ptr _DSBase+0, dx
		mov word ptr _DSBase+2, cx

;       get DOS & Extender version number
;
        mov     ah,30h
        int     21h                     ; modifies eax,ebx,ecx,edx
        mov     _osmajor,al
        mov     _osminor,ah
        mov     ecx,eax                 ; remember DOS version number
        sub     esi,esi                 ; offset 0 for environment strings
        mov     edi,81H                 ; DOS command buffer es:edi

        mov ebx, cs
        mov ax, 000AH
        int 31h
        mov ebx, eax
        mov cx, es:[DOS_PSP_ENV_SEG]    ; - get environment segment into cx

        mov al, X_RATIONAL
        mov ah, XS_RATIONAL_NONZEROBASE

;--- here: esi=0 (start env), ebx=cs alias, ax=extender version, cx=environment selector
;---       edi=offset start cmdline (rel to [_psp])

        mov     _Extender,al            ; record extender type
        mov     _ExtenderSubtype,ah     ; record extender subtype
        mov     es,ebx                  ; get access to code segment
        mov     es:__saved_DS,ds        ; save DS value
        mov     _Envptr,esi             ; save address of environment strings
        mov     word ptr _Envptr+4,cx   ; save segment of environment area
;
        mov     bx,FLG_LFN*256          ; assume 'lfn=n' env. var. not present / assume 'no87=' env. var. not present
        mov     __no87,bl               ; set state of "no87" enironment var
        and     __uselfn,bh             ; set "LFN" support status

        mov     ebx, _STACKTOP
        mov     _dynend,ebx             ; set top of dynamic memory area
        mov     eax,0FFH                ; run all initalizers
        call    __InitRtns              ; call initializer routines

        call    _LibMain

        mov eax, offset scd
        retf
__DLLstart_ endp

;       don't touch AL in __exit, it has the return code

ifdef FC
EXITCC equ <fastcall>
else
EXITCC equ <>
endif
        public  EXITCC __exit

__exit  proc near EXITCC

ifndef __STACK__
        push    eax                     ; get return code into eax
endif
        jmp     L7

        public  __do_exit_with_msg_

; input: ( char *msg, int rc )  always in registers

ifdef __JWASM__
__do_exit_with_msg_::
else
__do_exit_with_msg_:
endif
        push    edx                     ; save return code
        push    eax                     ; save address of msg
        mov     edx,offset ConsoleName
        push ds
        push cs
        pop ds
        mov     ax,03d01h               ; write-only access to screen
        int     021h
        pop ds
        mov     ebx,eax                 ; get file handle
        pop     edx                     ; restore address of msg
        mov     esi,edx                 ; get address of msg
        cld                             ; make sure direction forward
L6:     lodsb                           ; get char
        cmp     al,0                    ; end of string?
        jne     L6                      ; no
        mov     ecx,esi                 ; calc length of string
        sub     ecx,edx                 ; . . .
        dec     ecx                     ; . . .
        mov     ah,040h                 ; write out the string
        int     021h                    ; . . .
        push ds
        push cs
        pop ds
        mov     edx,offset NewLine      ; write out the string
        mov     ecx,sizeof NewLine      ; . . .
        mov     ah,040h                 ; . . .
        int     021h                    ; . . .
        pop ds
L7:
        xor     eax, eax
        mov     edx,FINI_PRIORITY_EXIT-1; less than exit
        call    __FiniRtns              ; call finializer routines

        pop     eax                     ; restore return code
        mov     ah,04cH                 ; DOS call to exit with return code
        int     021h                    ; back to DOS
__exit endp

        align   4

        public  __GETDS
        @cpublic __GETDSStart_
        @cpublic __GETDSEnd_

__GETDS proc near
__GETDSStart_ label near
        mov     ds,cs:__saved_DS        ; load saved DS value
        ret                             ; return
        align dword
__saved_DS  dw  0                       ; DS save area for interrupt routines
__GETDS endp
__GETDSEnd_ label near

memset proto c p:ptr, value:dword, size_:dword
__brk proto c :dword

malloc proc c public amnt:dword

ifdef _DEBUG
	invoke __dprintf, CStr("malloc(%X) dynend=%X curbrk=%X",10), amnt, _dynend, _curbrk
endif
tryagain:
	mov ecx, amnt
	add ecx, 8-1
	and cl, 0f8h
	mov eax, _curbrk
	sub eax, _dynend
	jc outofmem
	cmp eax, ecx
	jc outofmem
	mov eax, _dynend
	add _dynend, ecx
	ret
outofmem:
	invoke __brk, 10000h
	and eax, eax
	jnz tryagain
	ret
malloc endp

free proc c public pMem:ptr
	mov ecx, pMem
	cmp ecx, 0
	jz done
	cmp ecx, _curbrk
	jnc done
done:
	ret
free endp

calloc proc c public items:dword, size_:dword
	mov eax, size_
	mul items
	push eax
	invoke malloc, eax
	pop edx
	.if eax
		push eax
		invoke memset, eax, 0, edx
		pop eax
	.endif
	ret
calloc endp

_TEXT ends

        end __DLLstart_
