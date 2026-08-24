
;--- OW: make linker avoid to include all those float error messages
;--- saves ~3.5 kB

	.386
	.model ?MODEL

	.code

ifdef OW19
_matherr proc c arg1:ptr
	xor eax,eax
	ret
_matherr endp
endif

__rtmathfuncname proc c funccode:dword
	mov eax, offset szNull
	ret
__rtmathfuncname endp

szNull db 0

__reporterror proc c type_:dword, name_:ptr, arg1:real8, arg2:real8, retval:real8
	xor eax,eax
	ret
__reporterror endp

	end
