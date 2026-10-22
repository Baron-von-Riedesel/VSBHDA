
;--- DPMI 0x300/0x301 functions.

	.386
	.MODEL tiny
	option casemap:none
	option proc:private

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

	END
