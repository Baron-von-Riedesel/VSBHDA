
	.386
	.MODEL small
	option casemap:none
	option proc:private

	.data

	.code

SNDISR_Mixer proc c public uses ebx esi edi pSrc1:ptr, pSrc2:ptr, dwSamples:dword, dwVol1:dword, dwVol2:dword

	mov edi, pSrc1
	mov esi, pSrc2
	mov ecx, dwSamples
	mov ebx, dwVol1
	mov ebp, dwVol2
nextsample:
	lodsw
	movsx eax, ax
	movsx edx, word ptr [edi]
	imul eax, ebp
	imul edx, ebx
	sar eax, 8
	sar edx, 8
	imul eax, edx		; do a "geometric" addition
	sar eax, 15
	stosw
	dec ecx
	loop nextsample
	lea ebp, [esp+3*4]
	ret
SNDISR_Mixer endp


	END
