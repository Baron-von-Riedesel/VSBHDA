
;--- convert mono to stereo
;--- this optionally replaces the C code in sndisr.c
;--- currently not active ( problem with Tyrian 2000 ).

	.386
	.model ?MODEL
	option casemap:none

	.code

cv_channels_1_to_2 proc c public uses esi edi pSrc:ptr, nSamples:dword

	mov esi, pSrc
	mov ecx, nSamples
	lea edi, [esi+ecx*4-2]
	lea esi, [esi+ecx*2-2]
	std
@@:
	lodsw
	stosw
	stosw
	loop @B
	cld
	ret
cv_channels_1_to_2 endp

	end
