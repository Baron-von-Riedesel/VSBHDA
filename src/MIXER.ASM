
;--- not used yet

	.386
	.MODEL small
	option casemap:none
	option proc:private

	.data

	.code

;--- doint just a "geometric" addition is surely not correct, since
;--- if one factor is 0, the result will be 0.

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
	jz @F
	shr eax, 15
done:
	stosw
	dec ecx
	loop nextsample
	lea ebp, [esp+3*4]
	ret
@@:
	mov ax, [esi-2]
	imul eax, ebp
	sar eax, 8
	add eax, edx
	jmp done

SNDISR_Mixer endp


	END
