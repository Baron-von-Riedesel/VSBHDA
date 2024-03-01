
;--- include the 16-bit code as binary. This way one avoids possible
;--- problems with a linker's 16-bit fixup handling for 32-bit binaries.

	.386
ifdef ?FLAT
	.model flat
else
	.model small
endif

	.code

PTRAP_RM_Wrapper proc c public

%	incbin <OUTD\rmcode.bin>

PTRAP_RM_Wrapper endp

PTRAP_RM_WrapperEnd proc c public
PTRAP_RM_WrapperEnd endp

	end
