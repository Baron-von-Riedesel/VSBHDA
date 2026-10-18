
;--- DPMI functions called by drivers.

	.386
	.MODEL ?MODEL
	option casemap:none
	option proc:private

MEMINFO struct
dwHdl     dd ?
dwSize    dd ?
dwAddress dd ?
MEMINFO ends

	.code

;--- simulate real-mode int ( ax=0300h )

__dpmi_simulate_real_mode_interrupt proc c public uses ebx edi intno:byte, pRegs:ptr
	mov edi, pRegs
	movzx ebx, intno
	xor ecx, ecx
	mov ax, 300h
	int 31h
	sbb eax, eax
	ret
	align 4
__dpmi_simulate_real_mode_interrupt endp

;--- simulate real-mode far call with RETF frame ( ax=0301h )

__dpmi_simulate_real_mode_procedure_retf proc c public uses ebx edi pRegs:ptr
	mov edi, pRegs
	xor ebx, ebx
	xor ecx, ecx
	mov ax, 301h
	int 31h
	sbb eax, eax
	ret
	align 4
__dpmi_simulate_real_mode_procedure_retf endp

;--- physical memory mapping ( ax=0800h )

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
	align 4
__dpmi_physical_address_mapping endp

;--- free physical memory mapping ( ax=0801h )

__dpmi_free_physical_address_mapping proc c public uses ebx pInfo:ptr
	mov eax, pInfo
	mov cx, word ptr [eax].MEMINFO.dwAddress+0
	mov bx, word ptr [eax].MEMINFO.dwAddress+2
	mov ax, 801h
	int 31h
	sbb eax, eax	; C=-1, NC=0
	ret
	align 4
__dpmi_free_physical_address_mapping endp

	END
