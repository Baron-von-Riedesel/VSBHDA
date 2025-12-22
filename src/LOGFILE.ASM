
;--- logfile - for debug version only

ifdef _DEBUG

	.386
	.model ?MODEL, c
	option casemap:none
	option proc:private

	include config.inc

ifdef DJGPP
	assume ds:_DATA
endif

ifndef DJGPP
__GETDS proto near
endif

fopen  proto c :ptr, :ptr
fwrite proto c :ptr, :dword, :dword, :ptr
fclose proto c :ptr

	.data

oldint41 PFAR ?
externdef c logfile_start:ptr
externdef c logfile_ofs:dword

	.const

szWB db "wb",0
szOpenErr db "logfile open error",13,10,'$'
szWriteErr db "logfile write error",13,10,'$'

	.code

;--- logfile write uses Int 41h, ax=0

myint41 proc
	cmp ax,0
	jnz exit
	push ds
	push ebx
	mov ebx, cs
	add ebx, 8
	mov ds, ebx
	mov ebx, logfile_start
	add ebx, logfile_ofs
	mov [ebx], dl
	inc logfile_ofs
	and logfile_ofs, LOGFILESIZE - 1
	pop ebx
	pop ds
exit:
	insIRET
myint41 endp

Int41_Init proc c public uses ebx
	mov bl,41h
	mov ax,204h
	int 31h
	mov dword ptr [oldint41+0],edx
	mov word ptr [oldint41+SEGOFS],cx
	mov edx, offset myint41
	mov ecx, cs
	mov ax,205h
	int 31h
	ret
Int41_Init endp

Int41_Exit proc c public uses ebx
	mov edx,dword ptr [oldint41+0]
	mov cx,word ptr [oldint41+SEGOFS]
	mov bl,41h
	mov ax,205h
	int 31h
	ret
Int41_Exit endp

;--- this has been moved here since fopen/fwrite/fclose are std C calling convention

LogfileDump proc c public uses ebx pName:ptr

if 1
;--- enable interrupts, since this proc may be called by port trapping code;
;--- this should never be necessary
	mov ax,901h
	insPUSHF
	call cs:[oldint31]
endif

	invoke fopen, pName, offset szWB
	.if eax
		mov ebx, eax
		invoke fwrite, logfile_start, logfile_ofs, 1, ebx
		.if eax != logfile_ofs
			mov edx,offset szWriteErr
			mov ah,9
			int 21h
		.endif
		invoke fclose, ebx
	.else
		mov edx,offset szOpenErr
		mov ah,9
		int 21h
	.endif
	ret
LogfileDump endp

endif

	end
