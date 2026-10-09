;--- VXDCALL.ASM: call the protected mode API of VSBVXD.386 from the
;--- 16-bit Windows driver VSBHDA.DRV (large model, Open Watcom).
;--- int vxd_init(void): get the API entry (Int 2Fh, AX=1684h); 0 = ok
;--- int vxd_call(struct vxdregs far *): registers in/out
;---     (eax, ebx, ecx, edx, esi, edi as dwords); 0 = ok, -1 = carry

VSBVXD_DEVICE_ID equ 7A56h

	.model large, c
	.386

	.data
vxd_entry dd 0
tmp_ebx   dd 0

	.code VXDCALL_TEXT

vxd_init proc uses di
	push es
	xor di, di
	mov es, di
	mov ax, 1684h
	mov bx, VSBVXD_DEVICE_ID
	int 2Fh
	mov word ptr vxd_entry+0, di
	mov word ptr vxd_entry+2, es
	pop es
	mov ax, word ptr vxd_entry+2
	or ax, word ptr vxd_entry+0
	neg ax			;0 -> 0, else nonzero
	sbb ax, ax		;entry found: -1
	not ax			;found: 0, not found: -1
	ret
vxd_init endp

vxd_call proc uses si di, regs:far ptr
	push es
	les bx, regs
	push es
	push bx
	mov eax, es:[bx+4]
	mov tmp_ebx, eax
	mov eax, es:[bx+0]
	mov ecx, es:[bx+8]
	mov edx, es:[bx+12]
	mov esi, es:[bx+16]
	mov edi, es:[bx+20]
	mov ebx, tmp_ebx
	call vxd_entry
	mov tmp_ebx, ebx
	pop bx
	pop es
	pushf
	mov es:[bx+0], eax
	mov eax, tmp_ebx
	mov es:[bx+4], eax
	mov es:[bx+8], ecx
	mov es:[bx+12], edx
	mov es:[bx+16], esi
	mov es:[bx+20], edi
	popf
	sbb ax, ax
	pop es
	ret
vxd_call endp

	end
