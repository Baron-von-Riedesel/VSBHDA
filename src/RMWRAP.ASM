
;--- include the 16-bit code as binary. This way one avoids possible
;--- problems with a linker's 16-bit fixup handling for 32-bit binaries.

	.386
	.model ?MODEL

	.code

PTRAP_RM_Wrapper proc c public

%	incbin <OUTD\rmcode.bin>

PTRAP_RM_Wrapper endp

PTRAP_RM_WrapperEnd proc c public
PTRAP_RM_WrapperEnd endp

copyrmwrap proc c public uses esi edi pDst:ptr
	mov edi, pDst
	mov esi, offset PTRAP_RM_Wrapper
	mov ecx, PTRAP_RM_WrapperEnd - PTRAP_RM_Wrapper
	rep movsb es:[edi], cs:[esi]
	ret
copyrmwrap endp

	end
