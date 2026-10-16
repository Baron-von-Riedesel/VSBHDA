
;--- LibMain for dynamically loaded drivers;
;--- supplies the module's entry address;
;--- the binary is assumed to run in protected-mode,
;--- 32-bit tiny model (non-zero-based flat).

	.386
	.model tiny
	.dosseg
	option proc:private
	option casemap:none

	include DEBUG.INC

NULL equ 0

SNDCARD_INFO struct 4
shortname dd ?
info      dd ?
adetect   dd ?
start     dd ?
stop      dd ?
close     dd ?
setrate   dd ?
writedata dd ?
getpos    dd ?
irq       dd ?
wrMixer   dd ?
rdMixer   dd ?
Mixerdata dd ?
datasize  dd ?
SNDCARD_INFO ends

AUI struct
private_data dd ?
card_info    dd ?	;*sndcard_info 
AUI ends

calloc proto c :dword, :dword
externdef __linear_rmstack:dword
externdef __sndcard_info:SNDCARD_INFO
externdef _curbrk:dword
externdef _dynend:dword
externdef _end:near		  ; end of BSS
externdef _DSBase:dword

;--- macros for address translation

;Rebase2Extern macro x
;	add x, basediff
;endm
Rebase2Intern macro x
	sub x, basediff
endm

@CallVector macro member, arg, translate:=<1>
if translate
	mov eax, arg
	Rebase2Intern eax
	push eax
else
	push arg
endif
	call __sndcard_info.member
endm

	.data

cardinfo SNDCARD_INFO <0,0,my_adetect, my_start, my_stop, my_close, my_setrate, NULL, my_getpos, my_irq, my_wrMixer, my_rdMixer, 0, 0>

_DSBase dd 0
__linear_rmstack dd ?
_curbrk dd 0
_dynend dd 0
basediff dd 0
ifdef _DEBUG
OMODE equ 4	;OM_DEBUGGER ( output via int 41h )
else
OMODE equ 1	;OM_DOS ( output via DOS int 21h )
endif
externdef _bOMode:byte
_bOMode db OMODE

	.code

;--- allocate the private data here in adetect - it's the first call into the driver code!
;--- the driver's adetect may modify some members of the card_info struct:
;--- shortname, Mixerdata, ...
;--- so after the call it's the right time to copy those members to cardinfo.

my_adetect proc far c aui:ptr

	invoke calloc, 1, __sndcard_info.datasize
	and eax, eax
	jz done
	mov edx, eax
	mov eax, aui
	Rebase2Intern eax
	mov [eax].AUI.private_data, edx
	push eax
	call __sndcard_info.adetect
	add esp,4
	and eax, eax
	jz done
;--- cardinfo's shortname used to return name of detected card.
	mov ecx, __sndcard_info.shortname
	mov edx, __sndcard_info.Mixerdata
	mov cardinfo.shortname, ecx
	mov cardinfo.Mixerdata, edx
done:
	ret
my_adetect endp

my_start proc far c aui:ptr
	@CallVector start, aui
	cmp _bOMode, 1	;DOS output active?
	jnz @F
	mov _bOMode, 2	;switch to direct VIO out
@@:
	ret
my_start endp

my_stop proc far c aui:ptr
	@CallVector stop, aui
	ret
my_stop endp

my_close proc far c aui:ptr
	@CallVector close, aui
	ret
my_close endp

my_setrate proc far c aui:ptr
	@CallVector setrate, aui
	ret
my_setrate endp

my_getpos proc far c aui:ptr
	@CallVector getpos, aui
	ret
my_getpos endp

my_irq proc far c card:ptr
	@CallVector irq, card, 0
	ret
my_irq endp

my_wrMixer proc far c aui:ptr
	@CallVector wrMixer, aui
	ret
my_wrMixer endp

my_rdMixer proc far c aui:ptr
	@CallVector rdMixer, aui
	ret
my_rdMixer endp

;--- __GETDS() is used by dprintf()

ifdef _DEBUG
__GETDS proc public
	push eax
	mov eax, cs
	add eax, 8
	mov ds, eax
	pop eax
	ret
__GETDS endp
endif

;--- LibMain; returns address of local card_info struct (functions with RETF!)
;--- baseaddr: caller's base address.
;--- setup heap variables, DSBase and basediff.

LibMain proc far c public uses ebx baseaddr:dword, heapsize:dword, rmstack:dword

	mov eax, rmstack
	mov __linear_rmstack, eax
	mov eax, _end
	mov _dynend, eax
	add eax, heapsize
	mov _curbrk, eax
	mov ebx, ds
	mov ax,6
	int 31h
	push cx
	push dx
	pop eax
	mov _DSBase,eax
	sub eax, baseaddr
	mov basediff, eax
	mov eax, offset cardinfo
	ret

LibMain endp

    end LibMain
