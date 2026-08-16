
;*** convert a string to a 32-bit int

	.386
	.MODEL ?MODEL
	option casemap:none

	.CODE

strtol proc c public uses esi ebx nptr:ptr sbyte, endptr: ptr ptr sbyte, base:dword

	mov esi, nptr
	.repeat
		mov al,[esi]
		.break .if (al != ' ') && (al != 9)
		inc esi
	.until 0
	mov bh,0
	.if al == '+'
		inc esi
	.elseif al == '-'
		inc esi
		mov bh,-1
	.endif
	mov ecx, base
	.if !ecx
		mov cl,10
		mov ax, [esi]
		.if al == '0'
			inc esi
			mov cl,8
			.if (ah == "x") || (ah == "X")
				inc esi
				mov cl, 16
			.endif
		.endif
	.endif
	mov bl, cl
	add bl, '0'
	.if bl > 10
		add bl,7
	.endif
	or bl,20h
	xor edx, edx
	xor eax, eax
	.while 1
		lodsb
		call makedigit
		.break .if CARRY?
		push eax
		mov eax, edx
		mul ecx
		mov edx, eax
		pop eax
		add edx, eax
	.endw
	dec esi
	mov ecx, endptr
	jecxz @F
	mov [ecx], esi
@@:
	mov eax, edx
	.if bh
		neg eax
	.endif
	ret
makedigit:
	cmp al,'0'
	jb nodigit
	cmp al,'9'
	jbe digit
	cmp al, 'A'
	jb nodigit
	or al,20h
	cmp al,bl
	jnb nodigit
	sub al,'a'-10
	clc
	retn
digit:
	sub al,'0'
	clc
	retn
nodigit:
	stc
	retn
strtol endp

	end
