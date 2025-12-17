
;--- logfile - for debug version only

ifdef _DEBUG

	.386
	.model ?MODEL
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

	.code

myint41 proc
	cmp ax,0
	jnz exit
	push ebx
	mov ebx, logfile_start
	add ebx, logfile_ofs
	mov [ebx], dl
	inc logfile_ofs
	and logfile_ofs, LOGFILESIZE - 1
	pop ebx
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

LogfileDump proc c public uses ebx pName:ptr

local hLog:dword

	invoke fopen, pName, offset szWB
	.if eax
		mov ebx, eax
		invoke fwrite, logfile_start, logfile_ofs, 1, ebx
		invoke fclose, ebx
	.endif
	ret
LogfileDump endp

endif

	end
