
;--- software mixer - not used

	.386
	.MODEL ?MODEL
	option casemap:none
	option proc:private

	.code

SNDISR_Mixer proc c public uses ebx esi edi pSrc1:ptr, pSrc2:ptr, dwSamples:dword, dwVoiceVol:dword, dwMidiVol:dword

	mov edi, pSrc1
	mov esi, pSrc2
	mov ebx, dwVoiceVol
	mov ecx, dwSamples
	mov ebp, dwMidiVol
nextsample:
	lodsw
	movsx eax, ax
	movsx edx, word ptr [edi]
	add eax, 8000h
	add edx, 8000h

;--- 0 <= eax/edx <= ffff

	imul eax, ebp
	imul edx, ebx
	shr eax, 8
	shr edx, 8

;--- a*b/32768 : kind of "geometric addition" - true geometric would be sqrt(a*b)
	mul edx
	shr eax, 15

if 1
	bt eax, 16
	setnc dl
	mov dh, 0   ; dx=0/1
	sub dx, 1   ; dx=ffff/0
	or eax, edx
endif
	sub ax, 8000h
	stosw
	dec ecx
	jnz nextsample
	lea ebp, [esp+3*4]
	ret

SNDISR_Mixer endp

	END
