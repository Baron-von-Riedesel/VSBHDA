
;--- ISR handling
;--- note that jwasm's coff output isn't fully compatible with DJGPP's LD
;--- the problem are "offset xxx" expressions.


	.386
	.MODEL small, c
	option casemap:none
	option proc:private

DPMI_ISR_HANDLE struct 4
dwOfsPM	dd ?	; old sound ISR
wSegPM	dw ?	; old sound ISR
wOfsRM	dw ?
wSegRM	dw ?
intno	db ?
dwISR	dd ?
dwStack	dd ?
dwDS    dd ?
DPMI_ISR_HANDLE ends

	.data

handle dd ?

	.code

	assume ds:_DATA

SwitchStackISR proc

	pushad
	push ds
	push es
	mov eax, cs:[handle]
	mov ds, cs:[eax].DPMI_ISR_HANDLE.dwDS
	mov es, cs:[eax].DPMI_ISR_HANDLE.dwDS
	mov ecx, esp
	mov edx, ss
	lss esp, fword ptr [eax].DPMI_ISR_HANDLE.dwStack
	push edx
	push ecx
	call [eax].DPMI_ISR_HANDLE.dwISR
if 0
	pushfd
	pop eax
	test ah, 2
	jz @F
	int 3
@@:
endif
	lss esp, [esp]
	pop es
	pop ds
	popad
;	test byte ptr [esp+2*4+1], 2	; interrupts disabled? ( may be if called by another ISR )
;	jz @F
	sti
@@:
	iretd

SwitchStackISR endp

_hdpmi_CallOldISR proc public hdl:ptr
	mov eax, hdl
	pushfd
	call fword ptr [eax].DPMI_ISR_HANDLE.dwOfsPM
	ret
_hdpmi_CallOldISR endp

;--- install the sound ISR
;--- the approach is to hide this ISR from the ring3 DOS exender;

_hdpmi_InstallISR proc public uses ebx intno:dword, isr:dword, pHdl:ptr, pStack:ptr

	mov ebx, intno
	mov ax, 204h
	int 31h
	jc error
	mov eax, pHdl
	mov handle, eax
	mov [eax].DPMI_ISR_HANDLE.dwOfsPM, edx
	mov [eax].DPMI_ISR_HANDLE.wSegPM, cx
	mov [eax].DPMI_ISR_HANDLE.intno, bl
	mov edx, pStack
	mov [eax].DPMI_ISR_HANDLE.dwStack, edx
	mov edx, isr
	mov [eax].DPMI_ISR_HANDLE.dwISR, edx
	mov [eax].DPMI_ISR_HANDLE.dwDS, ds
	mov ecx, cs
	mov edx, offset SwitchStackISR
	mov ax, 205h
	int 31h
	jc error
	xor eax, eax
	ret
error:
	or eax, -1
	ret
_hdpmi_InstallISR endp

_hdpmi_UninstallISR proc public uses ebx pHdl:ptr
	mov ebx, pHdl
	mov edx, [ebx].DPMI_ISR_HANDLE.dwOfsPM
	mov cx, [ebx].DPMI_ISR_HANDLE.wSegPM
	mov bl, [ebx].DPMI_ISR_HANDLE.intno
	mov ax, 205h
	int 31h
	jc error
	xor eax, eax
	ret
error:
	or eax, -1
	ret
_hdpmi_UninstallISR endp

if 0

;--- display 32-bit number on screen
;--- low-level.

	.data

flatdesc dd 0

	.code

	assume ds:_DATA

DispNumber proc public number:dword, pos:dword

	pushad
	lar eax, [flatdesc]
	jz @F
	mov ax,0
	mov cx,1
	int 31h
	jc exit
	mov ebx,eax
	mov [flatdesc], eax
	or cx,-1
	or dx,-1
	mov ax,8
	int 31h
@@:
	push ds
	mov ds, [flatdesc]
	mov ebx, pos
	shl ebx, 1
	add ebx, 0B8000h
	mov eax, number
	mov edi, 10
@@nextdigit:
	xor edx, edx
	div edi
	add dl,'0'
	cmp dl,'9'
	jbe @F
	add dl,7+20h
@@:
	mov [ebx],dl
	sub ebx, 2
	and eax, eax
	jne @@nextdigit
	pop ds
exit:
	popad
	ret

DispNumber endp

endif

	END
