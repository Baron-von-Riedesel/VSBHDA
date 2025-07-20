
;--- raw file access - to avoid the C buffered file system

	.386
	.MODEL ?MODEL, c
	option casemap:none

	.code

;--- just mode "rb" is supported;
;--- expects DOS LFN API supported by DOS extender (true for HDPMI)

fopen proc uses ebx esi pszName:ptr, pszMode:ptr

	mov edx,pszMode
	cmp byte ptr [edx+2],0
	jnz error
	cmp word ptr [edx],"br"
	jnz error
	mov dx,1
	mov bx,3000h	;BH bit 5:1=return error, no int 24h, bit 4:1=allow 4GB-1 file size
	mov cx,0
	mov esi,pszName
	mov ax,716ch
	stc
	int 21h
	jnc done
	cmp ax,7100h
	jnz error
	mov ax,6c00h
	int 21h
	jc error
done:
	movzx eax,ax
	ret
error:
	xor eax,eax
	ret

fopen endp

fclose proc uses ebx pFile:ptr

	mov ebx,pFile
	mov ah,3Eh
	int 21h
	jc error
	mov eax,1
	ret
error:
	xor eax,eax
	ret

fclose endp

fseek proc uses ebx pFile:ptr, ofs:dword, mode:dword
	mov ebx,pFile
	mov dx,word ptr ofs+0
	mov cx,word ptr ofs+2
	mov al,byte ptr mode
	mov ah,42h
	int 21h
	jc error
	push dx
	push ax
	pop eax
	ret
error:
	or eax,-1
	ret
fseek endp

ifdef NOTFLAT
fread proc uses es ebx edi pBuffer:ptr, elems:dword, size_:dword, pFile:ptr

;--- descriptor adjustments needed if client is 16-bit
local tsel:dword
local desc[8]:byte
	push ss
	pop es
	lea edi,desc
	mov ebx,ds
	mov ax,000Bh
	int 31h
	mov cx,1
	mov ax,0
	int 31h
	jc error2
	mov tsel,eax
	mov edx,pBuffer
nextchk:
	mov eax,edx
	call setdesc
	mov ecx,size_
	cmp ecx,10000h
	jb @F
	mov ecx,10000h-10h
@@:
	xor edx,edx
	mov ebx,pFile
	push ds
	mov ds,tsel
	mov ah,3Fh
	int 21h
	pop ds
	jc error
	add edx,ecx
	sub size_, ecx
	jnz nextchk
	push eax
	call freedesc
	pop eax
	ret
else
fread proc uses ebx pBuffer:ptr, elems:dword, size_:dword, pFile:ptr
	mov ebx,pFile
	mov edx,pBuffer
	mov ecx,elems
	imul ecx,size_
	mov ah,3Fh
	int 21h
	jc error
	ret
endif
error:
ifdef NOTFLAT
	call freedesc
error2:
endif
	or eax,-1
	ret
ifdef NOTFLAT
freedesc:
	mov ebx,tsel
	mov ax,1
	int 31h
	retn
setdesc:
	mov ch,desc+7
	mov cl,desc+4
	shl ecx,16
	mov cx,word ptr desc+2
	add eax,ecx
	mov word ptr desc+2,ax
	shr eax,16
	mov desc+4,al
	mov desc+7,ah
	mov ebx,tsel
	mov ax,000Ch
	int 31h
	retn
endif
fread endp

	END
