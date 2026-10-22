
;--- used by sbxfi - EMU20K2

	.386
	.MODEL tiny
	option casemap:none
	option proc:private

	.code

;--- set page attributes
;--- calls DPMI int 31h, ax=507h, es:edx=attribute array

__dpmi_set_page_attr proc c public uses ebx edi esi handle:ptr, ofs:dword, pages:dword, attr:word
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
__dpmi_set_page_attr endp

	END
