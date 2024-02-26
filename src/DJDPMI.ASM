
;--- reimplement the djgpp __dpmi_xxx() functions that are used.

	.386
	.MODEL small, c
	option casemap:none
	option proc:private

PREMAPDMA equ 0

if 1	; optionally, activate if another compiler (watcom?) is to be used

MEMINFO struct
dwHdl     dd ?
dwSize    dd ?
dwAddress dd ?
MEMINFO ends

	.code

__dpmi_simulate_real_mode_interrupt proc c public uses ebx edi intno:byte, pRegs:ptr
	mov edi, pRegs
	movzx ebx, intno
	xor ecx, ecx
	mov ax, 300h
	int 31h
	sbb eax, eax
	ret
__dpmi_simulate_real_mode_interrupt endp

__dpmi_simulate_real_mode_procedure_retf proc c public uses ebx edi pRegs:ptr
	mov edi, pRegs
	xor ebx, ebx
	xor ecx, ecx
	mov ax, 301h
	int 31h
	sbb eax, eax
	ret
__dpmi_simulate_real_mode_procedure_retf endp

__dpmi_simulate_real_mode_procedure_iret proc c public uses ebx edi pRegs:ptr
	mov edi, pRegs
	xor ebx, ebx
	xor ecx, ecx
	mov ax, 302h
	int 31h
	sbb eax, eax
	ret
__dpmi_simulate_real_mode_procedure_iret endp

__dpmi_get_segment_base_address proc c public uses ebx dwSeg:dword, pBase:ptr dword
	mov ebx, dwSeg
	mov ax, 6
	int 31h
	sbb eax, eax
	jnz @F
	mov ebx, pBase
	mov word ptr [ebx+0], dx
	mov word ptr [ebx+2], cx
@@:
	ret
__dpmi_get_segment_base_address endp

__dpmi_set_segment_limit proc c public uses ebx dwSeg:dword, dwLimit:dword
	mov dx, word ptr dwLimit+0
	mov cx, word ptr dwLimit+2
	mov ebx, dwSeg
	mov ax, 8
	int 31h
	sbb eax, eax
	ret
__dpmi_set_segment_limit endp

if PREMAPDMA

__dpmi_allocate_linear_memory proc c public uses ebx edi esi pInfo:ptr, commit:dword
	mov edi, pInfo
	mov ebx, [edi].MEMINFO.dwAddress
	mov ecx, [edi].MEMINFO.dwSize
	mov edx, commit
	mov ax, 504h
	int 31h
	sbb eax, eax
	jnz @F
	mov [edi].MEMINFO.dwHandle, esi
	mov [edi].MEMINFO.dwAddress, ebx
@@:
	ret
__dpmi_allocate_linear_memory endp

__dpmi_free_memory proc c public uses esi edi dwHandle:dword
	mov di, word ptr dwHandle+0
	mov si, word ptr dwHandle+2
	mov ax, 502h
	int 31h
	sbb eax, eax
	ret
__dpmi_free_memory endp

endif

;--- return -1 on error, 0 on success

__dpmi_physical_address_mapping proc c public uses ebx esi edi pInfo:ptr
	mov eax, pInfo
	mov di, word ptr [eax].MEMINFO.dwSize+0
	mov si, word ptr [eax].MEMINFO.dwSize+2
	mov cx, word ptr [eax].MEMINFO.dwAddress+0
	mov bx, word ptr [eax].MEMINFO.dwAddress+2
	mov ax, 800h
	int 31h
	sbb eax, eax	; C=-1, NC=0
	jnz @F
	mov edx, pInfo
	mov word ptr [edx].MEMINFO.dwAddress+0, cx
	mov word ptr [edx].MEMINFO.dwAddress+2, bx
@@:
	ret
__dpmi_physical_address_mapping endp

__dpmi_free_physical_address_mapping proc c public uses ebx pInfo:ptr
	mov eax, pInfo
	mov cx, word ptr [eax].MEMINFO.dwAddress+0
	mov bx, word ptr [eax].MEMINFO.dwAddress+2
	mov ax, 801h
	int 31h
	sbb eax, eax	; C=-1, NC=0
	ret
__dpmi_free_physical_address_mapping endp

__dpmi_free_dos_memory proc c public dwSelector:dword
	mov edx, dwSelector
	mov ax,101h
	int 31h
	sbb eax, eax
	ret
__dpmi_free_dos_memory endp

__dpmi_set_coprocessor_emulation proc c public uses ebx bMode:byte
	mov bl, bMode
	mov ax, 0e01h
	int 31h
	sbb eax, eax
	ret
__dpmi_set_coprocessor_emulation endp

endif

	END
