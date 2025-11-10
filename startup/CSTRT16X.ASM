
;--- DOS 32-bit startup code for 16-bit client
;--- to be assembled with "jwasm -zcw cstrt16x.asm"

	name cstart

	.386
	.model small
	.dosseg

FINI_PRIORITY_EXIT equ 16

X_RATIONAL    equ 1
X_PHARLAP_V1  equ 2

XS_RATIONAL_NONZEROBASE equ 1
XS_PHARLAP_NORMAL       equ 0

NO87 equ 0
LFN equ 0	;1=support LFN (here not needed for C)

DOS_PSP_ENV_SEG equ 2Ch
if NO87
FLG_NO87        equ 1
endif
if LFN
FLG_LFN         equ 1
endif

CStr macro text:vararg
local sym
CONST segment
sym db text,0
CONST ends
	exitm <offset sym>
endm

        include debug.inc

        externdef c _DSBase          : dword
        extrn   __CMain              : near
        extrn   __InitRtns           : near
        extrn   __FiniRtns           : near
;        extrn   __DOSseg__           : near
ifndef OW19
        extrn    __child             : dword
endif
if NO87
        extrn    __no87              : byte
endif
        extrn   _end                 : near  ; end of BSS (start of STACK)

        extern c  _dynend            : dword
        extern c  _curbrk            : dword
        extern c  _psp               : word
        extern c  _osmajor           : byte
        extern c  _osminor           : byte
        extern c  _STACKLOW          : dword
        extern c  _STACKTOP          : dword
if LFN
        extern c  __uselfn           : byte
endif
        extern c  _Extender          : byte
        extern c  _ExtenderSubtype   : byte
        extern c  _Envptr            : fword
;       extern c  __FPE_handler      : dword
        extern c  _LpCmdLine         : dword
        extern c  _LpPgmName         : dword

; this guarantees that no function pointer will equal NULL
; (WLINK will keep segment 'BEGTEXT' in front)
; This segment must be at least 4 bytes in size to avoid confusing the
; signal function.

BEGTEXT  segment use32 para public 'CODE'

;	public ___begtext

forever label near
	int 3h
	jmp forever
___begtext label byte
	while ($ - forever) lt 10h
	nop
	endm

BEGTEXT  ends

_NULL segment para public 'BEGDATA'
__nullarea label word
	db 01h,01h,01h,00h
	public __nullarea
_NULL ends

_AFTERNULL segment dword public 'BEGDATA'
_AFTERNULL ends

DATA    segment dword public 'DATA'
DATA    ends
CONST   segment dword public 'DATA'
CONST   ends
CONST2  segment dword public 'DATA'
CONST2  ends

;--- define those globals to avoid dos4g
;--- interrupt 02 handler to be included.
	public __8087
	public __real87

	public __x386_zero_base_selector
	public __csalias

	.data
__x386_zero_base_selector dw 0  ; base 0 selector for X-32VM
__8087    db 0
__real87  db 0

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

	.data?

;--- .stack won't align to para in 32-bit
;	.stack 4000h
STACK segment para stack 'STACK'
	org 4000h
STACK ends

DGROUP group _NULL,_AFTERNULL,CONST,CONST2,_DATA,DATA,TIB,TI,TIE,XIB,XI,XIE,YIB,YI,YIE,_BSS,STACK

	.const
ConsoleName     db "con",0
NewLine         db 0Dh,0Ah
szMSDOS         db "MS-DOS",0
szNoDosExt      db "No DOS-Extender",13,10,0

	.code

;--- entry: ds,ss=dgroup, es=psp

_cstart_ proc near public
	jmp around

	align 4
	dd ___begtext              ; make sure dead code elimination

;--- since model is small, a csalias is needed
;--- to store some variables in _TEXT.
__csalias dw 0

around:
	;v1.8: dprintf needs a working __GETDS()
	;@dprintf CStr("vsbhda16: entry, esp=%X",10), esp
	mov _psp, es
	mov _STACKTOP, esp
	mov _STACKLOW, offset _end
	mov _curbrk, esp
	xor ebp, ebp

	mov ax, 4Fh
	int 41h
	cmp ax, 0F386h
	jnz @F
	int 3
@@:

	push es
	mov ax,168ah
	mov esi,offset szMSDOS
	int 2Fh
	pop es
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

	mov ebx, cs
	mov ax, 000Ah
	int 31h
	push ds
	mov ds, eax
	assume ds:_TEXT
	mov __csalias, ax
	pop ds
	assume ds:DGROUP

	mov ebx, ds
	mov ax, 0006h
	int 31h
	mov word ptr _DSBase+0, dx
	mov word ptr _DSBase+2, cx

;--- resize dos mem to psp
	mov edx, es
	mov bx, 10h
	mov ax, 102h
	int 31h

;--- get DOS

	mov ah,30h
	int 21h
	mov _osmajor,al
	mov _osminor,ah
	mov ecx,eax                 ; remember DOS version number
	sub esi,esi                 ; offset 0 for environment strings
	mov edi,81H                 ; DOS command buffer es:edi

;--- get CS alias into BX

	movzx ebx, __csalias

;--- get environment segment into cx

	mov cx, es:[DOS_PSP_ENV_SEG]

;--- HX is mostly compatible with rational

	mov al, X_RATIONAL
	mov ah, XS_RATIONAL_NONZEROBASE

;--- here: esi=0 (start env), ebx=cs alias, ax=extender version, cx=environment selector
;---       edi=offset start cmdline (rel to [_psp])

	mov _Extender,al            ; record extender type
	mov _ExtenderSubtype,ah     ; record extender subtype
	mov es,ebx                  ; get access to code segment
	mov es:__saved_DS,ds        ; save DS value
	mov dword ptr _Envptr+0,esi ; save address of environment strings
	mov word ptr _Envptr+4,cx   ; save segment of environment area
	push esi                    ; save address of environment strings
;
;       copy command line into bottom of stack
;
	mov es,_psp                 ; point to PSP
	mov edx, _STACKLOW
	sub ecx,ecx
	mov cl,es:[edi-1]           ; get length of command
	cld                         ; set direction forward
	mov al,' '
	repe scasb
	lea esi,[edi-1]
	mov edi,edx
	mov ebx,es
	mov edx,ds
	mov ds,ebx
	mov es,edx                  ; es:edi is destination
	je @F
	inc ecx
	rep movsb
@@:
	sub al,al
	stosb
	stosb
	pop esi                     ; restore address of environment strings
	dec edi                     ; back up pointer 1
	push edi                    ; save pointer to pgm name
	push edx                    ; save ds(stored in dx)
	mov ds,word ptr es:_Envptr+4; get segment addr of environment area
	assume ds:nothing
if LFN
	mov bx,FLG_LFN shl 8        ; assume 'lfn=n' env. var. not present / assume 'no87=' env. var. not present
elseif NO87
	xor bx,bx
endif
nextitem:
if NO87 or LFN
	mov eax,[esi]               ; get first 4 characters
	or eax,20202020h            ; map to lower case
endif
if NO87
	cmp eax,"78on"              ; check for "no87"
	jne @F                      ; skip if not "no87"
	cmp byte ptr [esi+4],'='    ; make sure next char is "="
	jne L4                      ; no
	or bl,FLG_NO87              ; - indicate 'no87' was present
	jmp L4
@@:
endif
if LFN
	cmp eax,"=nfl"              ; check for 'lfn='
	jne L4                      ; skip if not 'lfn='
	mov al,[esi][4]             ; get next character
	or al,20h
	cmp al,'n'                  ; make sure next char is 'n'
	jne L4                      ; no
	and bh,not FLG_LFN          ; indicate no 'lfn=n' present
endif
L4:
	cmp byte ptr [esi],0        ; end of string ?
	lodsb
	jne L4                      ; until end of string
	cmp byte ptr [esi],0        ; end of all strings ?
	jne nextitem                ; if not, then skip next string
	lodsb
	inc esi                     ; point to program name
	inc esi

;--- copy the program name into bottom of stack

@@:
	cmp byte ptr [esi],0        ; end of pgm name ?
	movsb
	jne @B
	pop ds
	assume ds:DGROUP
	pop esi                     ; restore address of pgm name
if NO87
	mov __no87,bl               ; set state of "no87" enironment var
endif
if LFN
	and __uselfn,bh             ; set "LFN" support status
endif
	mov ebx, _STACKTOP
	mov _dynend, ebx            ; set top of dynamic memory area
	mov eax, edi
	xchg eax, _STACKLOW
	mov _LpCmdLine, eax         ; save command line address
	mov _LpPgmName, esi         ; save program name address
	mov eax, 0FFH               ; run all initalizers
	call __InitRtns             ; call initializer routines
	call __CMain
_cstart_ endp

;--- don't touch AL in __exit, it has the return code

ifdef FC
EXITCC equ <fastcall>
else
EXITCC equ <>
endif

	public EXITCC __exit
	public __do_exit_with_msg_
ifdef OW19
	public __do_exit_with_msg__
endif

__exit  proc near EXITCC

ifndef __STACK__
    push eax                     ; get return code into eax
endif
    jmp L7

; input: ( char *msg, int rc )  always in registers

ifdef OW19
__do_exit_with_msg__::
endif
__do_exit_with_msg_::
	push edx                     ; save return code
	push eax                     ; save address of msg
	mov  edx,offset ConsoleName
	mov ax, 3d01h                ; write-only access to screen
	int 21h
	mov ebx,eax                  ; get file handle
	pop edx                      ; restore address of msg
	mov esi,edx                  ; get address of msg
	cld
@@:
	lodsb
	cmp al,0
	jne @B
	mov ecx,esi                  ; calc length of string
	sub ecx,edx
	dec ecx
	mov ah,40h                   ; write out the string
	int 21h
	mov edx,offset NewLine
	mov ecx,sizeof NewLine
	mov ah,40h
	int 21h
L7:
	xor eax, eax
	mov edx,FINI_PRIORITY_EXIT-1; less than exit
	call __FiniRtns              ; call finializer routines
	pop eax                      ; restore return code
	mov ah, 4cH
	int 21h
__exit endp

	align 4

	public  __GETDS
	public c __GETDSStart_
	public c __GETDSEnd_

__GETDS proc near
__GETDSStart_ label near
	mov ds,cs:__saved_DS
	ret
	align dword
__saved_DS  dw  0                ; DS save area for interrupt routines
__GETDS endp
__GETDSEnd_ label near

	end
