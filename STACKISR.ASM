
;--- ISR handling

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

;--- ISR for sound hw interrupt occuring in protected-mode

SwitchStackISR proc

	pushad
	push ds
	push es
	mov eax, cs:[handle]
	mov ds, cs:[eax].DPMI_ISR_HANDLE.dwDS
	mov es, [eax].DPMI_ISR_HANDLE.dwDS
	mov edx, ss
	cmp dx, word ptr [eax].DPMI_ISR_HANDLE.dwDS
	jz noswitch
	mov ecx, esp
	lss esp, fword ptr [eax].DPMI_ISR_HANDLE.dwStack
	cld
	push edx
	push ecx
	call [eax].DPMI_ISR_HANDLE.dwISR
	lss esp, [esp]
	jmp switchdone
noswitch:
if 0
	mov ax, 3
	int 10h
	mov ax, 0E5Ah
	mov bh,0
	int 10h
	jmp $
endif
	call [eax].DPMI_ISR_HANDLE.dwISR
switchdone:
	pop es
	pop ds
	popad
	sti
	iretd

SwitchStackISR endp

_hdpmi_CallOldISR proc public hdl:ptr
	mov eax, hdl
	pushfd
	call fword ptr [eax].DPMI_ISR_HANDLE.dwOfsPM
	ret
_hdpmi_CallOldISR endp

;--- install the sound ISR
;--- the idea is to hide this ISR from the ring3 DOS extender;

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
	mov eax, 1
	ret
error:
	xor eax, eax
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
	mov eax, 1
	ret
error:
	xor eax, eax
	ret
_hdpmi_UninstallISR endp

	END
