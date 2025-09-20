
;--- alloc a descriptor and set base & limit, attr=data

AllocDescriptor proc stdcall address:dword, limit:word

	mov cx,1
	mov ax,0
	int 31h
	jc error2
	mov bx, ax
	mov dx, word ptr address+0
	mov cx, word ptr address+2
	mov ax, 7
	int 31h
	jc error
	mov cx, ds
	lar cx, cx
	shr cx, 8
	mov ax, 9
	int 31h
	jc error
	mov dx,limit
	xor cx,cx
	mov ax, 8
	int 31h
	jc error
	mov ax,bx
	jmp exit
error:
	mov ax,1
	int 31h
error2:
	xor ax,ax
exit:
	ret
AllocDescriptor endp

;--- get base of a selector

GetSelectorBase proc stdcall uses bx wSel:word
	mov bx,wSel
	mov ax,0006
	int 31h
	push cx
	push dx
	pop ax
	pop dx
	ret
GetSelectorBase endp    

;--- read/write GDT
;--- mode: 0=read, 1=write

GetSetGdt proc stdcall uses ds si di sel:word, baseptr:dword, mode:word

local wSel:word
local gdt[6]:byte

	sgdt	fword ptr gdt
	mov 	ax,sel
	or		ax,7
	cmp 	ax,word ptr gdt+0
	mov 	ax,0
	ja		getsetgdtex

	invoke	AllocDescriptor, dword ptr gdt+2, word ptr gdt+0
	and		ax,ax
	jz		getsetgdtex

	mov		wSel, ax
	mov		es,ax
	mov 	di,sel
	and 	di,0FFF8h
	lds 	si,baseptr
	test	mode,1
	jnz 	getsetgdt1
	xchg	si,di
	mov 	ax,es
	push	ds
	pop 	es
	mov 	ds,ax
getsetgdt1:
	mov cx,4
	rep movsw
	xor ax,ax
	mov ds,ax
	mov bx, wSel
	mov ax, 1
	int 31h
	mov ax,1
getsetgdtex:
	ret

GetSetGdt endp

;*** get descriptor

GetDescriptor proc stdcall uses di sel:word, pDesc:dword

	mov bx,sel
	les di,pDesc
	test bx,4
	jnz getlocalsel
	invoke GetSetGdt, bx, pDesc, 0
	jmp exit
getlocalsel:
	mov ax,000Bh   ;get descriptor
	int 31h
	sbb ax,ax
	inc ax
exit:
	ret
GetDescriptor endp

	.data
msdosstr db "MS-DOS",0
	.code

;--- get LDT selector

GetLDTSelector proc uses esi edi

local entry16:dword
local entry32:fword

	mov esi,offset msdosstr   ; use ESI in case we run as 32-bit client
	mov ax,168Ah
	int 2Fh
	cmp al,00
	jnz error
	mov cx,ss
	lar ecx,ecx
	shr ecx,22+1
	jc is_32
	mov word ptr entry16+0, di
	mov word ptr entry16+2, es
	mov ax,100h
	call entry16
	jnc @F
error:
	xor ax, ax
@@:
	ret
is_32:
	mov dword ptr entry32+0, edi
	mov word ptr entry32+4, es
	mov ax,100h
	call entry32
	jc error
	jmp @B
        
GetLDTSelector endp

createldtalias proc

local desc[8]:byte

	sldt   dx
	invoke GetDescriptor, dx, addr desc
	and    ax,ax
	jz createldt_err2
	mov    cx,1
	mov    ax,0
	int    31h
	jc createldt_err2
	mov    bx,ax
	mov    ch,desc+7
	mov    cl,desc+4
	mov    dx,word ptr desc+2
	mov    ax,7
	int    31h
	jc createldt_err1
	mov    dx,word ptr desc+0
	mov    cx,0
	mov    ax,8
	int    31h
	jc createldt_err1
	mov    ax,bx
	jmp createldt_ex
createldt_err1:
	mov    ax,1
	int    31h
createldt_err2:
	xor    ax,ax
createldt_ex:
	ret
createldtalias endp

CreateLDTSelector proc
	call GetLDTSelector
	and ax, ax
	jnz exit
	call createldtalias
exit:
	ret
CreateLDTSelector endp

FreeLDTSelector proc stdcall ldtsel:word

	call GetLDTSelector
	and ax, ax
	jnz @F
	mov bx, ldtsel
	mov ax, 1
	int 31h
@@:
	ret
FreeLDTSelector endp

;*** set descriptor via LDT selector

SetDescriptorDirect proc stdcall sel:word, descriptor1:dword, descriptor2:dword

local rc:word

	mov rc,0

	call CreateLDTSelector
	verw ax
	jnz sm1
	mov es,ax
	mov bx,sel
	and bl,0F8h
	mov eax,descriptor1
	mov es:[bx+0],eax
	mov eax,descriptor2
	mov es:[bx+4],eax
	mov rc,1
	invoke FreeLDTSelector, es
sm1:
	mov ax,rc
	ret
SetDescriptorDirect endp

;--- call a 32-bit PL0 proc

PF16 typedef ptr far16

CallRing0Proc proc stdcall uses si di procaddr:fword, retreg:dword

local lpproc:PF16
local ring0cs:word
local descriptor[8]:byte
local reip:dword
local rc:word

	mov 	rc,0

	mov 	cx,2
	mov 	ax,0
	int 	31h
	jc		callring0_ex

	mov 	word ptr lpproc+2,ax
	add 	ax,8
	mov 	ring0cs,ax

	xor 	ax,ax

	mov 	word ptr descriptor+2,ax   ;Base = 00000000
	mov 	byte ptr descriptor+4,al
	mov 	word ptr descriptor+5,0CF9Bh ;ring0 32bit-codesegment,
	mov 	byte ptr descriptor+7,al
	dec 	ax
	mov 	word ptr descriptor+0,ax   ;Limit = 0FFFF

	invoke	SetDescriptorDirect, ring0cs, dword ptr descriptor+0, dword ptr descriptor+4
	and 	ax,ax
	jz		callring0ex1

	mov 	ax,word ptr procaddr+4
	mov 	es,ax
	invoke	GetSelectorBase, ax
	add 	ax,word ptr procaddr+0
	adc 	dx,word ptr procaddr+2

	mov 	word ptr descriptor+0,ax			;offset = linear address of proc
	mov 	word ptr descriptor+6,dx
	mov 	ax,ring0cs
	mov 	word ptr descriptor+2,ax
	mov 	ax,0EC00h				   ;386 call gate
	mov 	word ptr descriptor+4,ax

	invoke	SetDescriptorDirect,word ptr lpproc+2, dword ptr descriptor+0, dword ptr descriptor+4
	and 	ax,ax
	jz		callring0ex1

	cli
	call	lpproc
	sti
	les 	bx,retreg
	mov 	es:[bx],eax
	mov 	rc,1
callring0ex1:
	mov 	bx,word ptr lpproc+2
	mov 	ax,1
	int 	31h
	mov 	bx,ring0cs
	mov 	ax,1
	int 	31h
callring0_ex:
	mov 	ax,rc
	ret

CallRing0Proc endp

;--- get 2 descriptors
;--- 1. code, addr wSeg shl 4
;--- 2. data, addr flat

GetDesc32 proc stdcall wSeg:word
	mov cx,2
	mov ax,0
	int 31h
	jc error
	mov bx,ax
	movzx eax, wSeg
	shl eax, 4
	push eax
	pop dx
	pop cx
	mov ax,7
	int 31h
	or dx,-1
	xor cx,cx
	mov ax,8
	int 31h
	mov cx,cs
	lar cx,cx
	shr cx,8
	mov ax,9
	int 31h

	add bx,8
	xor cx,cx
	xor dx,dx
	mov ax,7
	int 31h
	or cx,-1
	or dx,-1
	mov ax,8
	int 31h
	sub bx,8

	mov ax,bx
	ret
error:
	xor ax,ax
	ret
GetDesc32 endp

FreeDesc32 proc stdcall wSel:word
	mov bx, wSel
	mov ax, 1
	int 31h
	add bx, 8
	int 31h
	ret
FreeDesc32 endp
