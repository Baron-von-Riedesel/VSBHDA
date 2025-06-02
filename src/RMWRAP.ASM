
;--- include the 16-bit code as binary. This way one avoids possible
;--- problems with a linker's 16-bit fixup handling for 32-bit binaries.

	.386
	.model ?MODEL

	.code

RMCode1:
%	incbin <OUTD\rmcode1.bin>
RMCode1End equ $
RMCode2:
%	incbin <OUTD\rmcode2.bin>
RMCode2End equ $

copyrmcode proc c public uses esi edi pDst:ptr, nSrc:dword
	mov edi, pDst
	mov esi, offset RMCode1
	mov ecx, RMCode1End - RMCode1
	cmp nSrc, 1
	jz @F
	mov esi, offset RMCode2
	mov ecx, RMCode2End - RMCode2
@@:
	rep movsb es:[edi], cs:[esi]
	mov eax, edi
	ret
copyrmcode endp

	end
