
;--- these variants of DPMI functions are called during interrupt time -
;--- hence int 31h must not be called directly!

	.386
	.MODEL ?MODEL
	option casemap:none
	option proc:private

	include CONFIG.INC

	.code

;--- ax=508h; map physical device/memory

__dpmi_map_physical_device_isr proc c public uses ebx esi handle:ptr, ofs:dword, pages:dword, physaddr:dword
	mov esi, handle
	mov ebx, ofs
	mov ecx, pages
	mov edx, physaddr
	mov ax, 508h
	insPUSHF
	call cs:[oldint31]
	sbb eax, eax
	ret
	align 4
__dpmi_map_physical_device_isr endp

	END
