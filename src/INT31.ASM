
;--- Int 31h/21h hook; purposes:
;--- # hide the sound hw IRQ from client

	.386
	.MODEL ?MODEL, c
	option casemap:none
	option proc:private

	include config.inc
	include debug.inc

	public oldint21
	public bIntSnd
	public bIntSB

externdef _SB_Notify31:near
externdef _Snd_Notify31:near
if TRAPINT21
externdef _SB_Notify21:near
externdef _Snd_Notify21:near
 ifndef NOTFLAT
  if @Model ne 7
externdef c DSBase:dword
  endif
 endif
endif

ifdef NOTFLAT

 if FIXEXC00
InstallExc00 proto 
UninstallExc00 proto 
 endif

	.code

 if FIXEXC00
externdef dwOldexc00:dword	; in _TEXT for 16-bit
externdef bExc00:byte       ; 1=trap set/get exc 00 vector
 endif

oldint31 PFAR ?
 if TRAPINT21
oldint21 PFAR ?
 endif

bIntSnd db -1
bIntSB  db -1
	align 4

else

	assume ds:_DATA

	.data

oldint31 PFAR ?
	align 4
 if TRAPINT21
oldint21 PFAR ?
	align 4
 endif
 
bIntSnd db -1
bIntSB  db -1

	align 4

	.code

endif

myint31 proc

ifdef NOTFLAT
 if FIXEXC00
	cmp ax, 202h	; get exc vector
	jz is202
	cmp ax, 203h	; set exc vector
	jz is203
@@:
 endif
endif
	cmp ax, 204h	; get int vector
	jz is204
	cmp ax, 205h	; set int vector
	jz is205
prevint:
	jmp cs:[oldint31]
is204:
is205:
	cmp bl, -1
	jz prevint
	cmp bl, cs:[bIntSnd]
	jz is_snd
	cmp bl, cs:[bIntSB]
	jz is_SB
	jmp prevint
is_snd:
	call _Snd_Notify31
	jc prevint
	jmp iret_with_NC
is_SB:
	call _SB_Notify31
	jc prevint
iret_with_NC:
 ifdef NOTFLAT
	push bp
	mov bp,sp
	and byte ptr [bp+2].IRETS._fl,not 1
	pop bp
 else
	and byte ptr [esp].IRETS._fl,not 1
 endif
	insIRET

ifdef NOTFLAT
 if FIXEXC00
is202:
	cmp bl,0
	jnz prevint
	cmp cs:[dwOldexc00],0
	jz prevint
	mov dx, word ptr cs:[dwOldexc00+0]
	mov cx, word ptr cs:[dwOldexc00+2]
	jmp iret_with_NC
is203:
	cmp bl,0
	jnz prevint
	cmp cs:[dwOldexc00],0
	jz prevint
	push ds
	mov ds, cs:[_csalias]
	assume ds:_TEXT
	mov word ptr [dwOldexc00+0], dx
	mov word ptr [dwOldexc00+2], cx
	pop ds
	assume ds:DGROUP
	jmp iret_with_NC
 endif
endif
	align 4

myint31 endp

if TRAPINT21

;--- int 21h proc

;--- a DOS/4GW bug has the strange effect that
;--- protected-mode vector for Int 21h must have hiword(edx) cleared!
;--- (because DOS/4GW, after reading this vector, sets exception vectors without
;--- clearing hiword(edx).

ifndef NOTFLAT
	.data
	align 4
int21tmp label byte
	db 0EAh		; jmp far32 ssss:oooooooo
	dd offset myint21
int21tmpCS dw 0
	.code
endif

myint21 proc

	cmp ah, 25h
	jz is25
	cmp ah, 35h
	jz is35
prevint:
	jmp cs:[oldint21]
is25:
is35:
	cmp al,-1
	jz prevint
	cmp al, cs:[bIntSnd]
	jz isSnd
	cmp al, cs:[bIntSB]
	jz isSB
	jmp prevint
isSnd:
	call _Snd_Notify21
	jc prevint
	insIRET
isSB:
	call _SB_Notify21
	jc prevint
	insIRET
	align 4

myint21 endp

endif

;--- install int 31h handler; also int 21h, if TRAPINT21==1

ifdef NOTFLAT
_InstallInt31 proc public uses ebx bDivErr:dword
else
_InstallInt31 proc public uses ebx
endif

	@dprintf CStr("_InstallInt31 enter",10)

ifdef NOTFLAT
 if FIXEXC00
	cmp bDivErr, 0
	jz @F
	call InstallExc00
@@:
 endif
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

if TRAPINT21
	mov bl, 21h
	mov ax, 204h
	int 31h
	jc error
	mov dword ptr oldint21+0, edx
	mov word ptr oldint21+SEGOFS, cx
 ifdef NOTFLAT
	mov ecx, cs
	mov edx, offset myint21
	mov ax, 205h
	int 31h
	jc error
 else
;--- stuff to fix the DOS/4GW error described above.
;--- setup a thunk for int 21h.
	mov cx, 1
	xor eax, eax
	int 31h
	jc error
	mov ebx, eax
;--- setup non-zerobased CS
	mov [int21tmpCS], cs
  if @Model eq 7
	push offset int21tmp
  else
	mov eax, [DSBase]           ; for DJGPP's non-zerobased model
	add eax, offset int21tmp
	push eax
  endif
	pop dx
	pop cx
	mov ax, 7
	int 31h
	mov dx,15
	xor ecx, ecx
	mov ax, 8
	int 31h
	mov ecx, cs
	lar ecx, ecx
	shr ecx, 8
	mov ax, 9
	int 31h
;--- set interrupt vector 21h
	mov ecx, ebx
	xor edx, edx
	mov bl, 21h
	mov ax, 205h
	int 31h
	jc error
 endif
endif

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
_InstallInt31 endp

_UninstallInt31 proc public uses ebx

	@dprintf CStr("_UninstallInt31 enter",10)
if TRAPINT21
	mov edx, dword ptr [oldint21+0]
	mov cx, word ptr [oldint21+SEGOFS]
	mov bl, 21h
	mov ax, 205h
	int 31h
	jc error
endif

	mov edx, dword ptr [oldint31+0]
	mov cx, word ptr [oldint31+SEGOFS]
	mov bl, 31h
	mov ax, 205h
	int 31h
	jc error

ifdef NOTFLAT
 if FIXEXC00
	call UninstallExc00
 endif
endif
	mov eax, 1
	ret
error:
	xor eax, eax
	ret
_UninstallInt31 endp

	END
