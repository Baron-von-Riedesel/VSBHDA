
;--- DPMI 0x800/0x801 functions.

	.386
	.MODEL tiny
	option casemap:none
	option proc:private

MEMINFO struct
dwHdl     dd ?
dwSize    dd ?
dwAddress dd ?
MEMINFO ends

	.code

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
