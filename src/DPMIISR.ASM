
;--- these variants of DPMI functions are called during interrupt time -
;--- hence int 31h must not be called directly!

	.386
	.MODEL ?MODEL
	option casemap:none
	option proc:private

	include CONFIG.INC

	.code

;--- ax=507h; set page attributes
if 0
__dpmi_set_page_attr_isr proc c public uses ebx edi esi handle:ptr, ofs:dword, pages:dword, attr:word
	mov esi, handle
	mov ebx, ofs
	mov ecx, pages
	mov edi, esp      ;save old esp
	mov ax, attr
@@:
	push ax
	loop @B
	mov ecx, pages
	mov edx, esp
	and esp, -4       ;ensure esp is dword aligned
	mov ax, 507h
	int 31h
	mov esp, edi
	sbb eax, eax
	ret
	align 4
__dpmi_set_page_attr_isr endp
endif
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
