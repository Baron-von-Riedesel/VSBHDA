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
;* Description:  DOS 32-bit startup code.
;*
;*****************************************************************************


	name    cstart

	.386
	.model flat
	.dosseg

;include langenv.inc
;include tinit.inc
TIB     segment byte public 'DATA'
TIB     ends
TI      segment byte public 'DATA'
TI      ends
TIE     segment byte public 'DATA'
TIE     ends

;include xinit.inc
FINI_PRIORITY_EXIT equ 16
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

;include extender.inc

NO87 equ 0
LFN  equ 0

DOS_PSP_ENV_SEG equ 2Ch
if NO87
FLG_NO87        equ     1
endif
if LFN
FLG_LFN         equ     1
endif

@cextrn macro x,y
extern c x:y
endm

@cpublic macro x
public c x
endm

        assume  nothing

        extrn   __CMain              : near
        extrn   __InitRtns           : near
        extrn   __FiniRtns           : near
;        extrn   __DOSseg__           : near

        extrn   _edata               : byte  ; end of DATA (start of BSS)
        extrn   _end                 : byte  ; end of BSS (start of STACK)

        @cextrn   _dynend            , dword
        @cextrn   _curbrk            , dword
        @cextrn   _psp               , word
        @cextrn   _osmajor           , byte
        @cextrn   _osminor           , byte
        @cextrn   _STACKLOW          , dword
        @cextrn   _STACKTOP          , dword
ifndef OW19
        extrn    __child             : dword
endif
if NO87
        extrn    __no87              : byte
endif
if LFN
        @cextrn   __uselfn           , byte
endif
        @cextrn   _Extender          , byte
        @cextrn   _ExtenderSubtype   , byte
        @cextrn   _Envptr            , dword
;        @cextrn   _Envseg            , word
        @cextrn   __FPE_handler      , dword
        @cextrn   _LpCmdLine         , dword
        @cextrn   _LpPgmName         , dword

DGROUP group _NULL,_AFTERNULL,CONST,CONST2,_DATA,DATA,TIB,TI,TIE,XIB,XI,XIE,YIB,YI,YIE,_BSS,STACK

; this guarantees that no function pointer will equal NULL
; (WLINK will keep segment 'BEGTEXT' in front)
; This segment must be at least 4 bytes in size to avoid confusing the
; signal function.

BEGTEXT  segment para public 'CODE'

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
        assume  cs:nothing
BEGTEXT  ends

_NULL   segment para public 'BEGDATA'
__nullarea label word
        db      01h,01h,01h,00h
        public  __nullarea
_NULL   ends

_AFTERNULL segment dword public 'BEGDATA'
_AFTERNULL ends

CONST   segment dword public 'DATA'
CONST   ends
CONST2  segment dword public 'DATA'
CONST2  ends

_DATA    segment dword public 'DATA'
;--- this is referenced in inirmsel.c!
__x386_zero_base_selector dw 0  ; base 0 selector for X-32VM
        public  __x386_zero_base_selector
_DATA    ends

DATA    segment dword public 'DATA'
DATA    ends

_BSS    segment dword public 'BSS'
_BSS    ends

STACK_SIZE      equ     10000h

STACK   segment para stack 'STACK'
        db      (STACK_SIZE) dup(?)
STACK   ends

		.code

        public  _cstart_

_cstart_ proc near
        jmp   short around

;
; miscellaneous code-segment messages
;
ConsoleName     db      "con",0
NewLine         db      0Dh,0Ah
szMSDOS         db      "MS-DOS",0
szNoDosExt      db      "No DOS-Extender",13,10,0

        align   4
        dd      ___begtext              ; make sure dead code elimination

around:

        assume  ds:DGROUP

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
        mov     ah,30h
        int     21h                     ; modifies eax,ebx,ecx,edx
        mov     _osmajor,al
        mov     _osminor,ah
        mov     ecx,eax                 ; remember DOS version number
        sub     esi,esi                 ; offset 0 for environment strings
        mov     edi,81H                 ; DOS command buffer es:edi
        mov ah, 51h     ; works with HDPMI only
        int 21h
        mov _psp, bx
        mov es, ebx
        mov cx,es:[DOS_PSP_ENV_SEG]     ; - get environment segment into cx
        mov ebx, ds
;        mov al,X_HX
;        mov al,X_RATIONAL
        mov al,0
        mov ah,0          ; meaning "zero-based"

;--- here: esi=0 (start env), ebx=ds, ax=extender version, cx=environment selector
;---       edi=offset start cmdline (rel to [_psp])

        mov     _Extender,al            ; record extender type
        mov     _ExtenderSubtype,ah     ; record extender subtype
        mov     es,ebx                  ; get access to code segment
        mov     es:__saved_DS,ds        ; save DS value
        mov     _Envptr,esi             ; save address of environment strings
        mov     word ptr _Envptr+4,cx   ; save segment of environment area
        push    esi                     ; save address of environment strings
;
;       copy command line into bottom of stack
;
        mov     es,_psp                 ; point to PSP
        mov     edx, _STACKLOW
        sub     ecx,ecx
        mov     cl,es:[edi-1]           ; get length of command
        cld                             ; set direction forward
        mov     al,' '
        repe    scasb
        lea     esi,[edi-1]
        mov     edi,edx
        mov     ebx,es
        mov     edx,ds
        mov     ds,ebx
        mov     es,edx                  ; es:edi is destination
        je      noparm
        inc     ecx
        rep     movsb
noparm: sub     al,al
        stosb                           ; store NULLCHAR
        stosb                           ; assume no pgm name
        pop     esi                     ; restore address of environment strings
        dec     edi                     ; back up pointer 1
        push    edi                     ; save pointer to pgm name
        push    edx                     ; save ds(stored in dx)
        mov     ds,word ptr es:_Envptr+4; get segment addr of environment area
if LFN
        mov     bx,FLG_LFN*256          ; assume 'lfn=n' env. var. not present / assume 'no87=' env. var. not present
elseif NO87
        xor     bx,bx
endif
L1:
if NO87 or LFN
        mov     eax,[esi]               ; get first 4 characters
        or      eax,20202020h           ; map to lower case
endif
if NO87
        cmp     eax,37386f6eh           ; check for "no87"
        jne     short L2                ; skip if not "no87"
        cmp     byte ptr 4[esi],'='     ; make sure next char is "="
        jne     short L4                ; no
        or      bl,FLG_NO87             ; - indicate 'no87' was present
        jmp     L4
L2:
endif
if LFN
        cmp     eax,3d6e666ch           ; check for 'lfn='
        jne     short L4                ; skip if not 'lfn='
        mov     al,byte ptr 4[esi]      ; get next character
        or      al,20h                  ; map to lower case
        cmp     al,'n'                  ; make sure next char is 'n'
        jne     short L4                ; no
        and     bh,not FLG_LFN          ; indicate no 'lfn=n' present
endif
L4:
        cmp     byte ptr [esi],0        ; end of string ?
        lodsb
        jne     L4                      ; until end of string
        cmp     byte ptr [esi],0        ; end of all strings ?
        jne     L1                      ; if not, then skip next string
        lodsb
        inc     esi                     ; point to program name
        inc     esi                     ; . . .
;
;       copy the program name into bottom of stack
;
L5:     cmp     byte ptr [esi],0        ; end of pgm name ?
        movsb                           ; copy a byte
        jne     L5                      ; until end of pgm name
        pop     ds                      ; restore ds
        pop     esi                     ; restore address of pgm name

        assume  ds:DGROUP
if NO87
        mov     __no87,bl               ; set state of "no87" enironment var
endif
if LFN
        and     __uselfn,bh             ; set "LFN" support status
endif
        mov     ebx,esp                 ; end of stack in data segment
        mov     _dynend,ebx             ; set top of dynamic memory area
        push edi

        mov     ecx,offset DGROUP:_end  ; end of _BSS segment (start of STACK)
        mov     edi,offset DGROUP:_edata; start of _BSS segment
        sub     ecx,edi                 ; calc # of bytes in _BSS segment
                                        ; DOS extender will zero rest of pages
zerobss:mov     dl,cl                   ; save bottom 2 bits of count in edx
        shr     ecx,2                   ; calc # of dwords
        sub     eax,eax                 ; zero the _BSS segment
        rep     stosd                   ; ...
        mov     cl,dl                   ; get bottom 2 bits of count
        and     cl,3                    ; ...
        rep     stosb                   ; ...
        pop eax
        xchg eax, _STACKLOW
        mov     _LpCmdLine,eax          ; save command line address
        mov     _LpPgmName,esi          ; save program name address
        mov     eax,0FFH                ; run all initalizers
        call    __InitRtns              ; call initializer routines
        call    __CMain
_cstart_ endp

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
        jmp     short   L7

        public  __do_exit_with_msg_
ifdef OW19
        public  __do_exit_with_msg__
endif

; input: ( char *msg, int rc )  always in registers

ifdef OW19
__do_exit_with_msg__::
endif
__do_exit_with_msg_::
        push    edx                     ; save return code
        push    eax                     ; save address of msg
        mov     edx,offset ConsoleName
        mov     ax,03d01h               ; write-only access to screen
        int     021h
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
        mov     edx,offset NewLine      ; write out the string
        mov     cl,sizeof NewLine
        mov     ah,040h                 ; . . .
        int     021h                    ; . . .
L7:
        xor     eax, eax
        mov     edx,FINI_PRIORITY_EXIT-1; less than exit
        call    __FiniRtns              ; call finializer routines
        pop     eax                     ; restore return code
        mov     ah,04cH                 ; DOS call to exit with return code
        int     021h                    ; back to DOS
__exit endp

;include msgcpyrt.inc

        align   4

        public  __GETDS
        @cpublic __GETDSStart_
        @cpublic __GETDSEnd_

_DATA segment ; HX is generally flat, zero-based; code section may be r/o
__saved_DS  dw  0                       ; DS save area for interrupt routines
_DATA ends

__GETDS proc    near
__GETDSStart_ label near
        mov     ds,cs:__saved_DS        ; load saved DS value
        ret                             ; return
__GETDS endp
__GETDSEnd_ label near

        end     _cstart_
