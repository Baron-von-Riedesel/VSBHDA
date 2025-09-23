
;--- include the 16-bit code as binary. This way one avoids possible
;--- problems with a linker's 16-bit fixup handling for 32-bit binaries.

	.386
	.model ?MODEL

	.const

RMCodeTab label dword
	dd offset RMCode1, RMCode1Size
	dd offset RMCode2, RMCode2Size

RMCode1 label byte
%	incbin <OUTD\rmcode1.bin>
RMCode1Size equ $ - offset RMCode1
RMCode2 label byte
%	incbin <OUTD\rmcode2.bin>
RMCode2Size equ $ - offset RMCode2

	.code

copyrmcode proc c public uses esi edi pDst:ptr, nSrc:dword
	mov edi, pDst
	mov esi, nSrc
	lea esi, [esi*8+RMCodeTab]
	lodsd
	mov ecx, eax
	lodsd
	mov esi, eax
	xchg esi, ecx
	rep movsb
	mov eax, edi
	ret
copyrmcode endp

	end
