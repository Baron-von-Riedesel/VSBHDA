;--- RTVXD.ASM: Open Watcom runtime helpers for VSBVXD.  The library
;--- versions pull in the C startup code (matherr, exit, stdio), which a
;--- VxD can't have.

	.386
	.387
	.model flat
	option casemap:none

	include VXD.INC

	.code

;--- IF@DPOW: x87 inline form of pow(): ST(0) = base (> 0), ST(1) = exponent;
;--- out: ST(0) = base ^ exponent (one register popped)
	public IF@DPOW
IF@DPOW proc
	fyl2x			;exponent * log2(base)
	fld st(0)
	frndint			;integer part
	fsub st(1), st(0)	;st1 = fraction
	fxch
	f2xm1			;2^fraction - 1
	fld1
	faddp st(1), st(0)
	fscale			;* 2^integer
	fstp st(1)
	ret
IF@DPOW endp

;--- __CHP: truncate ST(0) to an integer (round toward zero)
	public __CHP
__CHP proc
	sub esp, 4
	fnstcw [esp]
	push eax
	mov ax, [esp+4]
	or ah, 0Ch		;rounding control: chop
	mov [esp+6], ax
	fldcw [esp+6]
	frndint
	fldcw [esp+4]
	pop eax
	add esp, 4
	ret
__CHP endp

;--- C++ runtime fatal error (e.g. pure virtual call): stop Windows
	public __wcpp_4_fatal_runtime_error_
__wcpp_4_fatal_runtime_error_ proc
	xor eax, eax
	xor esi, esi
	VMMcall Fatal_Error_Handler
	jmp $
__wcpp_4_fatal_runtime_error_ endp

	end
