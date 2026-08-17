
;--- set PVI bit in CR4

	.286
	.model tiny
	.stack 256
	.dosseg
	.586p

	.code

start:
	mov eax, cr4
	or al, 2
	mov cr4, eax
	mov ah,4Ch
	int 21h

	END start
