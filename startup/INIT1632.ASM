
;*** simple startup code for a 16-bit DPMI client in MZ format
;*** will load HDPMI16.EXE if no DPMI API is found
;--- client is setup as small model
;--- cs = _TEXT, 32-bit, must be < 64 kB
;--- ss=ds=DGROUP, 16-bit, must be < 64 kB
;--- es = psp

    .286

CSGT64K  equ 1	; 1=_TEXT may exceed 64k
CHKCS64K equ 0	; 1= check if _TEXT is < 64k (if CSGT64K==0)

externdef _cstart_:far16
externdef _edata:near32

_TEXT16 segment word public 'CODE'

    .386

_DATA segment use32 dword public 'DATA'
_DATA ends

DGROUP group _DATA

_start proc

local psp:word
local codesize:word	; code size in paras
local dwCode:dword	; linear address code extended memory block
local dwData:dword	; linear address data extended memory block
local lpproc:dword

    lea bx, [bp+2]
    add bx, 16-1
    shr bx, 4       ; bx=size of stack in para
    mov ax, ss
    add bx, ax
    mov cx, es
    sub bx, cx      ; bx=size of memory block in para
    mov ah, 4Ah
    int 21h

    mov cx, DGROUP
    mov ax, ss
    sub ax, cx
    lea bx, [bp+2]
    shr bx, 4
    add bx, ax
    cmp bx, 1000h
    ja datatoobig
    shl ax, 4
    mov ss, cx              ; ds=ss=dgroup
    add sp, ax
    add bp, ax

    mov psp, es

if 1
;--- we don't need this 16-bit startup code in extended memory;
;--- this code assumes that _TEXT16 is "between" _TEXT and DGROUP!
    mov cx, cs
endif
    mov ax, es
    add ax, 10h				; skip PSP -> start para of segment _TEXT
    sub cx, ax
if CHKCS64K
    cmp cx, 1000h			; _TEXT < 1000h paragraphs?
    ja codetoobig
endif
    mov codesize, cx
    mov ds, ax              ; set ds to start of _TEXT so we get a csalias

    mov ax, 1687h           ; DPMI host installed?
    int 2fh
    and ax,ax
    jz @F
    call loadserver
    mov ax,1687h            ; try again
    int 2fh
    and ax,ax
    jnz nohost              ; still no host, exit
@@:        
    mov word ptr lpproc+0, di
    mov word ptr lpproc+2, es
    and si,si
    jz @F
    call allocdpmimem
    jc nodosmem
@@:
    xor ax, ax              ; 16 bit application
    call lpproc
    jc initfailed

;--- alloc memory for _TEXT segment
	movzx eax, codesize
	shl eax, 4
	push eax
	pop cx
	pop bx
	mov ax, 501h
	int 31h
	jc nomem
	mov word ptr [dwCode+0],cx
	mov word ptr [dwCode+2],bx

;--- alloc memory for DGROUP segment
	lea eax, [bp+2]
	push eax
	pop cx
	pop bx
	mov ax, 501h
	int 31h
	jc nomem
	mov word ptr [dwData+0],cx
	mov word ptr [dwData+2],bx

;--- get 2 descriptors for _TEXT/DGROUP in extended memory
	mov cx,2
	xor ax, ax
	int 31h
	jc nodesc

;--- setup _TEXT segment; base & limit
	mov bx, ax
	mov dx, word ptr [dwCode+0]
	mov cx, word ptr [dwCode+2]
	mov ax, 7
	int 31h
	movzx eax, codesize
	shl eax, 4
	push eax
	dec eax
	push eax
	pop dx
	pop cx
	mov ax, 8
	int 31h
if CSGT64K
	push bx
	mov bx,ds	;extend csalias as well
	int 31h
	pop bx
endif
	pop ecx

	push es

;--- copy code
;--- codesize is actually a bit too large, since it contains _TEXT16 as well
	shr ecx, 1
	mov es, bx
if CSGT64K
	xor edi,edi
	xor esi,esi
	rep movsw [edi],[esi]
else
	xor di, di
	xor si, si
	rep movsw
endif

;--- set attributes of _TEXT to code, 32-bit
	mov ax, cs
	lar cx, ax
	shr cx, 8
	or ch, 40h
	mov ax, 9
	int 31h

	add bx, 8

;--- setup DGROUP segment: base & limit
;--- for linear access, we need full 4GB descriptor
	mov dx, word ptr dwData+0
	mov cx, word ptr dwData+2
	mov ax, 7
	int 31h
	mov dx, -1
	mov cx, dx
	mov ax, 8
	int 31h

;--- copy DGROUP, init _BSS

	mov ecx, _edata		; start _BSS
	lea eax, [bp+2]		; top of stack
	sub eax, ecx
	xor di, di
	xor si, si
	mov es, bx
	rep movsb [di],ss:[si]
	mov cx, ax
	mov al, 0
	rep stosb

	pop es		; restore PSP

	mov ax, ds	; get old csalias
	mov dx, ss	; get old DGROUP
;--- switch ss,ds to new dgroup
	mov ds, bx
	mov ss, bx
	lea sp, [bp+2]
	sub bx, 8
	push bx
	mov bx, ax
	mov ax, 1
	int 31h
	mov bx, dx
	int 31h
	push _cstart_
	retf

allocdpmimem:
	mov ax,5802h		;save status umb
	int 21h
	xor ah,ah
	push ax
	mov ax,5800h		;save memory alloc strategy
	int 21h
	xor ah,ah
	push ax				;status 2
	mov bx,0081h		;first high,then low
	mov cx,0001h		;include umbs
	call setumbstatus
;--- alloc memory for dpmi host
	mov bx, si
	mov ah, 48h
	int 21h
	mov es, ax
	pop bx
	pop cx
setumbstatus:
	pushf
	push cx
	mov ax,5801h		;memory alloc strat restore
	int 21h
	pop bx
	mov ax,5803h		;umb link restore
	int 21h
	popf
	retn

if CHKCS64K
codetoobig:
	mov dx,offset errstr6
	jmp errexit
endif
datatoobig:
	mov dx,offset errstr5
	jmp errexit
nodesc:
	mov dx,offset errstr3
	jmp errexit
nomem:
	mov dx,offset errstr4
	jmp errexit
nohost:
	mov dx,offset errstr1
	jmp errexit
nodosmem:
initfailed:
	mov dx,offset errstr2
errexit:
	push cs
	pop ds
	mov ah, 9
	int 21h
	mov ax, 4CFFh
	int 21h
_start endp

EXECRM struct
environ dw ?
cmdline dd ?
fcb1    dd ?
fcb2    dd ?
res1    dd ?
res2    dd ?
EXECRM ends

;*** load dpmi host hdpmi16

loadserver proc private uses ds es si di

local psp:word
local env:word
local cmdline:word
local parmblock:EXECRM
local pgmname[80]:byte

    mov ah,51h
    int 21h
    mov psp,bx
    mov es,bx
    mov ax,es:[002Ch]
    mov env,ax

    CALL searchpath  ;search PATH= variable -> SI, SI=0000 if error
    CALL searchpgm   ;search HDPMI16
    JB error       ;---> error exit "not found"

    mov AX,env
    mov parmblock.environ,ax
    mov cmdline,0D00h
    lea bx,cmdline
    mov word ptr parmblock.cmdline+0,bx
    mov word ptr parmblock.cmdline+2,ss
    mov AX,psp
    mov word ptr parmblock.fcb1+0,5Ch
    mov word ptr parmblock.fcb1+2,ax
    mov word ptr parmblock.fcb1+0,6Ch
    mov word ptr parmblock.fcb2+2,ax

    push SS
    pop DS                       ;DS:DX=Path, ES:BX=parameter block
    push SS
    pop ES
    lea DX,pgmname               ;path for DPMISV16/32.EXE
    lea BX,parmblock
    MOV AX,4B00h                 ;execute server
    INT 21h
    JB error
    mov ax,1
    ret
error:
    mov ax,0
    ret

;*** search HDPMI16 in current directory and directories of PATH ***
;*** Input: SI=address of PATH Variable or NULL (no PATH defined) ***
;***      : DI=name of 

searchpgm:
    push si
    mov si,di
    lea DI, pgmname
    PUSH SS
    POP ES
    mov dx,di
    mov ds,env
nxtc:
    lodsb
    stosb
    cmp al,'\'
    jnz @F
    mov dx,di
@@:
    cmp al,0
    jnz nxtc
    mov di,dx
    pop si
    mov bl,0
nextdir:                             ;<---- try next directory in path
    PUSH CS
    POP DS
    PUSH SI
    mov si,offset srvname            ;name "HDPMI16.EXE"
    mov cx,lstr
    rep movsb

    push ss
    pop ds
    lea DX,pgmname
    MOV AX,3D00h
    INT 21h
    POP SI
    JNB found
    and bl,bl
    jnz @F
    mov bl,1
    lea di,pgmname                   ;get current directory
    jmp nextdir
@@:
    AND SI,SI
    JZ not_found                     ;PATH isnt defined, so were done
    MOV DI,DX
    mov ds,env
@@:
    lodsb
    stosb
    CMP AL,';'
    JZ @F
    CMP AL,00
    JNZ @B
    XOR SI,SI
@@:
    DEC DI
    mov al,'\'
    CMP al, es:[DI-01]
    JZ nextdir
    stosb
    JMP nextdir

found:
    MOV BX,AX
    MOV AH,3Eh                       ;Close hdpmi16.exe
    INT 21h
    CLC
    RETn
not_found:
    STC
    RETn

;*** search PATH in environment ***
;*** Out: SI-> behind "PATH=" or 0000 ***
;***      DI-> path of executable in environment

searchpath:
    SUB DI,DI
    xor dx,dx
    MOV ES,env
    PUSH CS
    POP DS
nexttry:
    MOV SI,offset szPath	;"PATH="
    MOV CX,0005
    REPZ CMPSB
    JNZ @F
    mov dx,di
@@:
    mov al,00
    or cx,-1
    repnz scasb
    cmp al,es:[di]
    JNZ nexttry
    add di,3            ;so DI points to path of executable now
    mov si,dx
    RETn

loadserver endp

szPath db 'PATH='

srvname db 'HDPMI16.EXE',00
lstr    equ $ - srvname

errstr1 db "no DPMI host found",13,10,'$'
errstr2 db "DPMI initialization failed",13,10,'$'
errstr3 db "no descriptors",13,10,'$'
errstr4 db "no extended memory",13,10,'$'
errstr5 db "DGROUP exceeds 64kB",13,10,'$'
if CHKCS64K
errstr6 db "code exceeds 64kB",13,10,'$'
endif
_TEXT16 ends

    end _start
