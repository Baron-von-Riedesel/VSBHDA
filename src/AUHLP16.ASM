
;--- for 16-bit variant: helper to call functions in sndcard.drv binary.
;--- the calls trigger a full stack switch, so that the called
;--- functions can assume small model setup SS=DS.

	.386
	.MODEL small

	option casemap:none
	option proc:private

externdef c psp:dword
externdef c DSBase:dword
externdef c _linear_rmstack:dword

LoadMod16 proto c :ptr, :ptr

;--- LoadMod16() fills LDPARMS

LDPARMS struct
wIP		dw ?
wCS		dw ?
wSP		dw ?
wSS		dw ?
dwHeap  dd ?
LDPARMS ends

SCDEXP struct
AU_init     dd ?
AU_prestart dd ?
AU_start    dd ?
AU_stop     dd ?
AU_close    dd ?
AU_getirq   dd ?
AU_isirq    dd ?
AU_getfreq  dd ?
AU_getshortname  dd ?
AU_setoutbytes   dd ?
AU_setrate       dd ?
AU_setmixer_init dd ?
AU_setmixer_one  dd ?
AU_setmixer_outs dd ?
AU_setmixer_all  dd ?
AU_writedata     dd ?
AU_cardbuf_space dd ?
DllStart dd ?
SCDEXP ends

	.data?

dwDrvBase dd ?
scd LDPARMS <>
vec SCDEXP <>

	.const

szDrv db "sndcard.drv",0

	.code

;--- macro to call an exported function in sndcard.drv

@callexp macro func, argspace
	mov eax, vec.func
	mov ecx, argspace
	call callexp
endm

;--- call function contained in SCDEXP.
;--- switch stack, copy arguments, call function, switch stack back
;--- eax=function to call
;--- ecx=# of arguments to copy

STKADJUST equ 2048h

callexp proc
	push es
	push ds
	push edi
	push esi
	mov es, scd.wSS
	movzx edi, scd.wSP
	sub scd.wSP, STKADJUST	; adjusting wSP, just in case
	shl ecx, 2
	sub edi, ecx
	shr ecx, 2
	lea edi, [edi-5*4]
	mov edx, ss
	shl edx, 16
	mov dx, sp
	mov ss, scd.wSS
	mov esp, edi
	stosd
	mov ax, scd.wCS
	stosd
	mov eax, retaddr
	stosd
	mov eax, cs
	stosd
	lea esi, [ebp+2*4]
	rep movsd
	mov es:[edi], edx	; finally, our ss:sp
	mov ds, scd.wSS
	retd
retaddr:
	lss sp,ss:[edi]
	pop esi
	pop edi
	pop ds
	add scd.wSP, STKADJUST
	pop es
	ret
callexp endp

;--- AU_init is the first function that is called.

AU_init proc c public uses ebx esi edi a1:dword, a2:dword

;--- load the sndcard.drv binary.
	invoke LoadMod16, addr szDrv, addr scd
	cmp eax, 0
	jz exit
	mov bx, scd.wSS
	mov ax, 6
	int 31h
	mov word ptr [dwDrvBase+0], dx
	mov word ptr [dwDrvBase+2], cx

;--- call the module's entry point
;--- call structure is:
;---  void *DllStart( word psp_sel, uint32_t heapsize, uint32_t rmstack );

	movzx eax, [scd.wIP]
	mov vec.DllStart, eax
	push _linear_rmstack
	push scd.dwHeap
	push [psp]
	push 0
	push ebp
	movzx ebp, sp
	@callexp DllStart, 3
	pop ebp
	add esp, 4*4
	.if eax
;--- copy the exported functions array to vec
		mov ecx, (sizeof SCDEXP - 4 ) / 4
		mov edi, offset vec
		add eax, dwDrvBase	;now a linear address
		sub eax, DSBase		;now a near pointer
		mov esi, eax
		rep movsd
;--- and finally call AU_init().
		@callexp AU_init, 2
	.endif
exit:
	ret

AU_init endp

AU_prestart proc c public a1:ptr

	@callexp AU_prestart, 1
	ret

AU_prestart endp

AU_start proc c public a1:ptr

	@callexp AU_start, 1
	ret

AU_start endp

AU_stop proc c public a1:ptr

	@callexp AU_stop, 1
	ret

AU_stop endp

AU_close proc c public a1:ptr

	@callexp AU_close, 1
	ret

AU_close endp

AU_getirq proc c public a1:ptr

	@callexp AU_getirq, 1
	ret

AU_getirq endp

AU_isirq proc c public a1:ptr

	@callexp AU_isirq, 1
	ret

AU_isirq endp

AU_getfreq proc c public a1:ptr

	@callexp AU_getfreq, 1
	ret

AU_getfreq endp

;--- sndcard.drv returns a near ptr
;--- must be converted to a near ptr for vsbhda16

AU_getshortname proc c public uses ebx a1:ptr

	@callexp AU_getshortname, 1
	.if eax					;now a drv near pointer
		add eax, dwDrvBase	;now a linear address
		sub eax, DSBase		;now a near pointer
	.endif
	ret

AU_getshortname endp

AU_setoutbytes proc c public a1:ptr

	@callexp AU_setoutbytes, 1
	ret

AU_setoutbytes endp

AU_setrate proc c public a1:ptr, a2:dword, a3:dword, a4:dword

	@callexp AU_setrate, 4
	ret

AU_setrate endp

AU_setmixer_init proc c public a1:ptr

	@callexp AU_setmixer_init, 1
	ret

AU_setmixer_init endp

AU_setmixer_one proc c public a1:ptr, a2:dword, a3:dword, a4:dword

	@callexp AU_setmixer_one, 4
	ret

AU_setmixer_one endp

AU_setmixer_outs proc c public a1:ptr, a2:dword, a3:dword

	@callexp AU_setmixer_outs, 3
	ret

AU_setmixer_outs endp

AU_setmixer_all proc c public a1:ptr

	@callexp AU_setmixer_all, 1
	ret

AU_setmixer_all endp

AU_writedata proc c public a1:ptr, a2:dword, pPCM:ptr

	mov eax, pPCM		; vsbhda near ptr
	add eax, DSBase		; linear address
	sub eax, dwDrvBase	; sndcard near ptr
	mov pPCM, eax
	@callexp AU_writedata, 3
	ret

AU_writedata endp

AU_cardbuf_space proc c public a1:ptr

	@callexp AU_cardbuf_space, 1
	ret

AU_cardbuf_space endp

	END
