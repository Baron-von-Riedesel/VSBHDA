
;--- load and setup a mz binary - used for dynamic sound card driver loading;
;--- the driver binary is assumed to run in protected-mode,
;--- 32-bit tiny model (non-zero-based flat).

	.386
	.model ?MODEL
	option casemap:none
	option proc:private

CS4281 equ 1
ES1370 equ 1
ES1371 equ 1
ICH    equ 1
HDA    equ 1
SBGEN  equ 1
SBXFI  equ 1
VIA82  equ 1

NULL equ 0

	include CONFIG.INC
	include DEBUG.INC

HEAPSIZE equ 2000h	;additional memory behind stack

ifdef DJGPP
;--- problem with DJGPP is that this format (a variant of COFF)
;--- doesn't know groups. If model isn't flat, one has to use assume for data access.
	assume ds:_DATA
_linear_rmstack textequ <_go32_info_block.linear_address_of_transfer_buffer>
externdef c _go32_info_block:_Go32_Info_Block
else
externdef c _linear_rmstack:dword
endif
externdef c DSBase:dword

;--- structs

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
union
 struct
dwEip   dd ?
wCs     dw ?,?
 ends
dfCsEip df ?
ends
memhdl  dd ?	;DPMI memory handle for currently loaded driver
ImgBase dd ?	;base address of currently loaded driver
if @Model ne 7
ImgDiff dd ?	;difference of image bases
endif
card_info dd ?	;address of cardinfo returned by LibMain(), translated
LDPARMS ends

;--- not really necessary, since INVOKE isn't used to call the member function
FUNC1 typedef proto c :dword
PFUNC1 typedef ptr FUNC1
FUNC2 typedef proto c :dword, :dword
PFUNC2 typedef ptr FUNC2
FUNC3 typedef proto c :dword, :dword, :dword
PFUNC3 typedef ptr FUNC3

;--- must match struct sndcard_info_s, defined in AUISTRUC.H

SNDCARD_INFO struct
shortname dd ?
infobits  dd ?
adetect   PFUNC1 ?
start     PFUNC1 ?
stop      PFUNC1 ?
close     PFUNC1 ?
setrate   PFUNC1 ?
writedata PFUNC3 ?
getpos    PFUNC1 ?
irq       PFUNC1 ?
wrMixer   PFUNC3 ?
rdMixer   PFUNC2 ?
Mixerdata dd ?
datasize  dd ?
SNDCARD_INFO ends

AUI struct
private_data dd ?
card_info    dd ?	;*sndcard_info 
card_pDmaBuffer dd ?
AUI ends

;--- macros

Rebase2Intern macro x
if @Model eq 7
	add x, params.ImgBase
else
	add x, params.ImgDiff
endif
endm
Rebase2Extern macro x
if @Model eq 7
	sub x, params.ImgBase
else
	sub x, params.ImgDiff
endif
endm

ifdef DJGPP
	.data
else
	.data?
endif

params LDPARMS <>

	.data

DStr macro text:vararg
local sym
	.const
sym db text,0
	.data
	exitm <offset sym>
endm

@DefInfo macro modname
	exitm <SNDCARD_INFO {DStr(modname),0,dyn_adetect, my_start, my_stop, my_close, my_setrate, NULL, my_getpos, my_irq, my_wrMixer, my_rdMixer, my_Mixerdata, 0}>
endm

;--- currently dynamically loaded drivers may be mixed with static ones -
;--- so the loading should be as transparent as possible.

if ES1370
	public c ES1370_sndcard_info
ES1370_sndcard_info @DefInfo("ES1370.DRV")
endif
if ES1371
	public c ES1371_sndcard_info
ES1371_sndcard_info @DefInfo("ES1371.DRV")
endif
if CS4281
	public c CS4281_sndcard_info
CS4281_sndcard_info @DefInfo("CS4281.DRV")
endif
if ICH
	public c ICH_sndcard_info
ICH_sndcard_info @DefInfo("ICH.DRV")
endif
if HDA
	public c HDA_sndcard_info
HDA_sndcard_info @DefInfo("HDA.DRV")
endif
if SBGEN
	public c SBGEN_sndcard_info
SBGEN_sndcard_info @DefInfo("SBGEN.DRV")
endif
if SBXFI
	public c SBXFI_sndcard_info
SBXFI_sndcard_info @DefInfo("SBXFI.DRV")
endif
if VIA82
	public c VIA82XX_sndcard_info
VIA82XX_sndcard_info @DefInfo("VIA82.DRV")
endif

my_Mixerdata dd 9 dup (0)

	.code

_LoadDriver proc c uses ebx esi edi pName:ptr

local hFile:word
local wCodeSel:word
local dwSizeAll:dword	; code, data, bss (no stack!)
local dwSizeImg:dword
local dwImgBase:dword
local mzhdr:IMAGE_DOS_HEADER

	@dprintf CStr("LoadDriver(%s) enter",10), pName
	mov edx, pName
	mov ax, 3D00h
	int 21h
	jc open_err
	mov hFile, ax
	mov ebx, eax
	mov ecx, sizeof mzhdr
	lea edx, mzhdr
	mov ah, 3Fh
	int 21h
	jc read_err
	cmp ax, cx
	jnz read_err
	cmp mzhdr.e_magic, "ZM"
	jnz fmt_err
	cmp mzhdr.e_crlc, 0		; no segment relocations allowed!
	jnz fmt_err

	movzx eax, mzhdr.e_cp
	dec eax
	shl eax, 9				; a page is 200h bytes
	movzx edx, mzhdr.e_cblp
	add eax, edx
	movzx edx, mzhdr.e_cparhdr
	shl edx, 4
	sub eax, edx			; eax=size initialized code+data
	mov dwSizeImg, eax    
	movzx edx, mzhdr.e_minalloc
	shl edx, 4
	add eax, edx
	add eax, 4-1	; ensure the size is dword aligned
	and al, 0fch
	mov dwSizeAll, eax
if HEAPSIZE
	add eax, HEAPSIZE
endif

;--- alloc mem for code, data, bss, heap
	mov cx, ax
	shr eax,16
	mov bx, ax
	mov ax,501h
	int 31h
	jc nomem
	mov word ptr dwImgBase+0, cx
	mov word ptr dwImgBase+2, bx
	mov word ptr params.memhdl+0,di
	mov word ptr params.memhdl+2,si
	@dprintf CStr("LoadDriver: image base/size=%X/%X, full size=%X",10), dwImgBase, dwSizeImg, dwSizeAll

;--- alloc descriptors for code & data
	mov cx,2
	xor ax, ax
	int 31h
	jc nodesc
	mov wCodeSel, ax
	mov ebx, eax
	mov dx, word ptr dwImgBase+0
	mov cx, word ptr dwImgBase+2
	mov ax, 7
	int 31h
	mov eax, dwSizeAll
if HEAPSIZE
	add eax, HEAPSIZE
endif
	dec eax
	push eax
	pop dx
	pop cx
	mov ax, 8
	int 31h		;set limit of code seg
	jc nodesc

	add bx,8
	mov dx, word ptr dwImgBase+0
	mov cx, word ptr dwImgBase+2
	mov ax, 7
	int 31h
	or dx, -1	;size of data seg is 4G
	or cx, -1
	mov ax, 8
	int 31h		;set limit of data (=-1)

;--- read image

	mov bx, hFile
	mov dx, mzhdr.e_cparhdr
	shl dx, 4
	xor cx, cx
	mov ax, 4200h
	int 21h
	jc seek_err
	push ds
	mov ds, wCodeSel
	xor edx, edx
	mov ecx, dwSizeImg
	mov ax, 3F00h
	int 21h
	pop ds
	jc read_err

;--- clear _BSS segment

	mov eax, dwSizeAll
	sub eax, ecx
	mov ecx, eax
	push es
	mov es, wCodeSel
	mov edi, dwSizeImg
	mov al,0
	cld
	rep stosb
	pop es

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
;--- set data descriptor to data, 32-bit
	push ebx
	add ebx, 8
	mov ecx, ds
	lar ecx, ecx
	shr ecx, 8
	or ch, 40h
	mov ax, 9
	int 31h
	pop ebx

	movzx eax, mzhdr.e_ip
	mov params.dwEip, eax
	mov params.wCs, bx
	mov eax, dwImgBase
	mov params.ImgBase, eax
if @Model ne 7
	sub eax, DSBase
	mov params.ImgDiff, eax
	@dprintf CStr("LoadDriver: ImgDiff=%X, DSBase=%X",10), params.ImgDiff, DSBase
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
	mov bx, hFile
	mov ah, 3Eh
	int 21h
	pop eax
open_err:
	@dprintf CStr("LoadDriver failed, ax=%X",10), ax
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

_LoadDriver endp

_FreeDriver proc c uses ebx esi edi
	mov bx, params.wCs
	mov ax,1
	int 31h
	add bx,8
	int 31h
	mov di,word ptr params.memhdl+0
	mov si,word ptr params.memhdl+2
	mov ax,502h
	int 31h
	ret
_FreeDriver endp

@CallMember macro member:req,arg1:req,arg2,arg3
	push es
	push ss
	push ds
	mov eax, dword ptr params.wCs
	mov edx, params.card_info
	push eax
	push [edx].SNDCARD_INFO.member
	add eax,8
?ARGS = 1
ifnb <arg3>
?ARGS = ?ARGS+1
	push arg3
endif
ifnb <arg2>
?ARGS = ?ARGS+1
	push arg2
endif
	push arg1
	mov ss, eax
	Rebase2Extern esp
	mov ds, eax
	mov es, eax
	call fword ptr [esp+4*?ARGS]
	add esp,(2+?ARGS)*4   ;skip eip,cs,arg1,arg2,arg3
	pop ds
	pop ss
	Rebase2Intern esp
	pop es
endm

;--- call the module's adetect() function;
;--- that adetect() function, however, is a member of the card_info returned by LibMain(),
;--- which is a FAR function.
;--- aui isn't modified.

my_adetect proc c aui:ptr

	@CallMember adetect,aui
	and eax, eax
	jnz @F
	call _FreeDriver
	xor eax, eax
@@:
	ret
my_adetect endp

my_start proc c aui:ptr
	@CallMember start,aui
	ret
my_start endp
my_stop proc c aui:ptr
	@CallMember stop,aui
	ret
my_stop endp
my_close proc c aui:ptr
	@CallMember close,aui
	ret
my_close endp
my_setrate proc c aui:ptr
	@CallMember setrate,aui
	ret
my_setrate endp
my_writedata proc c aui:ptr, pdata:ptr, size_:dword
	@CallMember writedata,aui,pdata,size_
	ret
my_writedata endp
my_getpos proc c aui:ptr
	@CallMember getpos,aui
	ret
my_getpos endp
my_irq proc c card:ptr
	@CallMember irq,card
	ret
my_irq endp
my_wrMixer proc c aui:ptr,reg:dword,value:dword
	@CallMember wrMixer,aui,reg,value
	ret
my_wrMixer endp
my_rdMixer proc c aui:ptr,reg:dword
	@CallMember rdMixer,aui,reg
	ret
my_rdMixer endp

;--- this is the first callout by AU_CARDS.AU_init();
;--- the card_info member in aui has been set to one of the drivers;
;--- card_info.shortname contains the driver file name.
;--- Now the first thing to do is to load the driver file with _LoadDriver();
;--- if that succeeds, the driver's entry has to be called - it's defined by
;--- LibMain32.asm. This function returns the card_info struct of the driver;
;--- thus the driver's functions ( adetect, setrate, start, ... ) can be called.

dyn_adetect proc c aui:ptr AUI

	mov edx, aui
	mov edx, [edx].AUI.card_info
	mov edx, [edx].SNDCARD_INFO.shortname
	@dprintf CStr("lddrv.adetect: loading %s driver, rmstack=%X",10), edx, _linear_rmstack
	invoke _LoadDriver, edx
	.if eax
		@dprintf CStr("lddrv.adetect: calling LibMain()",10)
		push ds
		push es
		push _linear_rmstack
		push HEAPSIZE
if @Model ne 7
		push DSBase
else
		push 0
endif
		movzx eax, params.wCs
		add eax, 8
		mov ds, eax
		mov es, eax
		call ss:params.dfCsEip		;call driver's LibMain() - stack NOT rebased here!
		add esp, 3*4
		pop ds
		pop es
		@dprintf CStr("lddrv.adetect: LibMain() returned, eax=%X",10), eax

;--- LibMain() has returned a SDNCARD_INFO struct;
;--- however, that's not the one defined by the driver code - it's defined in
;--- LIBMAIN32.ASM and contains addresses of functions with RETF!
;--- the returned struct address is stored in params, it cannot be used by
;--- AU_CARDS.C.

		.if eax
			Rebase2Intern eax
			mov params.card_info, eax
			invoke my_adetect, aui
			@dprintf CStr("lddrv.adetect: adetect returned, eax=%X",10), eax
			.if eax
				push ebx
				push esi
				mov esi, aui
;--- after driver's adetect() has run, card_info's shortname member may hold
;--- the name of the soundcard that has been found; store it in the cardinfo
;--- struct known by AU_CARDS.C
				mov ebx, [esi].AUI.card_info
				mov edx, params.card_info
				mov ecx, [edx].SNDCARD_INFO.shortname
				Rebase2Intern ecx
				mov [ebx].SNDCARD_INFO.shortname, ecx
;--- soundcard infobits ( currently just SCINFO_CARDBUF_SPACE, that no driver sets )
				mov ecx, [edx].SNDCARD_INFO.infobits
				mov [ebx].SNDCARD_INFO.infobits, ecx
;--- check if writedata member is set
				cmp [edx].SNDCARD_INFO.writedata,0
				jz @F
				mov [ebx].SNDCARD_INFO.writedata, offset my_writedata
@@:
;--- translate the mixerdata vector
				mov ecx, [edx].SNDCARD_INFO.Mixerdata
				jecxz @F
				Rebase2Intern ecx
				mov ebx, offset my_Mixerdata
				call translate_mixerarray
@@:
;--- translate the DMA buffer pointer - if it has been set by the driver
				mov ecx, [esi].AUI.card_pDmaBuffer
				jecxz @F
				Rebase2Intern ecx
				mov [esi].AUI.card_pDmaBuffer, ecx
@@:
				pop esi
				pop ebx
			.endif
		.endif
	.endif
	ret

;--- translate the mixerdata vector; the array is used by AU_CARDS.setmixer_one();

translate_mixerarray:
	mov edx, [ecx]
	cmp edx, 0
	jz @F
	Rebase2Intern edx
	mov [ebx], edx
	add ecx, 4
	add ebx, 4
	jmp translate_mixerarray
@@:
	retn
dyn_adetect endp

    end
