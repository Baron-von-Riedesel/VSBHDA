
;--- debug printf
;--- for OW, assemble with -zcw

	.386
	.model ?MODEL
	option casemap:none
	option proc:private

ifdef DJGPP
	assume ds:_DATA
endif

VioPutChar proto

	.data

;--- bOMode is 1 while the program runs as current program;
;--- once it has made itself resident, bOMode will be 2, meaning
;--- stdout shouldn't be used anymore.

bInit db 0
externdef c bOMode:byte

	.code

;--- i64toa(long long n, char * s, int base);
;--- convert 64-bit long long to string

i64toa PROC stdcall uses esi edi number:qword, outb:ptr, base:dword

	mov ch,0
	mov edi, base
	mov eax, dword ptr number+0
	mov esi, dword ptr number+4
	cmp edi,-10
	jne @F
	neg edi
	and esi,esi
	jns @F
	neg esi
	neg eax
	sbb esi,0
	mov ch,'-'
@@:
	mov ebx,outb
	add ebx,22
	mov byte ptr [ebx],0
@@nextdigit:
	dec ebx
	xor edx,edx
	xchg eax,esi
	div edi
	xchg eax,esi
	div edi
	add dl,'0'
	cmp dl,'9'
	jbe @F
	add dl,7+20h
@@:
	mov [ebx],dl
	mov edx, eax
	or edx, esi
	jne @@nextdigit
	cmp ch,0
	je @F
	dec ebx
	mov [ebx],ch
@@:
	mov eax,ebx
	ret

i64toa ENDP

	.data?
szTmp db 24 dup (?)
	.code

vprintf proc c uses ebx esi edi fmt:ptr sbyte, pArgs:ptr
local flag:byte
local longarg:byte
local size_:dword
local fillchr:dword

	mov edi,pArgs
@@L335:
	mov esi,fmt
nextchar:
	lodsb
	or al,al
	je done
	cmp al,'%'
	je formatitem
	call handle_char
	jmp nextchar
done:
	ret 

formatitem:
	push offset @@L335
	xor edx,edx
	mov [longarg],dl
	mov bl,1
	mov cl,' '
	cmp BYTE PTR [esi],'-'
	jne @F
	dec bl
	inc esi
@@:
	mov [flag],bl
	cmp BYTE PTR [esi],'0'
	jne @F
	mov cl,'0'
	inc esi
@@:
	mov [fillchr],ecx
	mov ebx,edx

	.while ( byte ptr [esi] >= '0' && byte ptr [esi] <= '9' )
		lodsb
		sub al,'0'
		movzx eax,al
		imul ecx,ebx,10		;ecx = ebx * 10
		add eax,ecx
		mov ebx,eax
	.endw

	mov [size_],ebx
	cmp BYTE PTR [esi],'l'
	jne @F
	mov [longarg],1
	inc esi
@@:
	lodsb
	mov [fmt],esi
	cmp al,'x'
	je handle_x
	cmp al,'X'
	je handle_x
	cmp al,'d'
	je handle_d
	cmp al,'u'
	je handle_u
	cmp al,'s'
	je handle_s
	cmp al,'c'
	je handle_c
	and al,al
	jnz @F
	pop eax
	jmp done
handle_c:
	mov eax,ss:[edi]
	add edi, 4
@@:
	call handle_char
	retn

handle_s:
	mov esi,ss:[edi]
	add edi,4
	jmp print_string
handle_d:
handle_i:
	mov ebx,-10
	jmp @F
handle_u:
	mov ebx, 10
	jmp @F
handle_x:
	mov ebx, 16
@@:
	xor edx,edx
	mov eax,ss:[edi]
	add edi,4
	cmp longarg,1
	jnz @F
	mov edx,ss:[edi]
	add edi,4
	jmp printnum
@@:
	and ebx,ebx
	jns @F
	cdq
@@:
printnum:
	lea esi, szTmp
	invoke i64toa, edx::eax, esi, ebx
	mov esi, eax

print_string:		;print string ESI, size EAX
	mov eax, esi
	.while byte ptr [esi]
		inc esi
	.endw
	sub esi, eax
	xchg eax, esi
	mov ebx,size_
	sub ebx,eax
	.if flag == 1
		.while sdword ptr ebx > 0
			mov eax, [fillchr]
			call handle_char	;print leading filler chars
			dec ebx
		.endw
	.endif

	.while byte ptr [esi]
		lodsb
		call handle_char	;print char of string
	.endw

	.while sdword ptr ebx > 0
		mov eax, [fillchr]
		call handle_char	;print trailing spaces
		dec ebx
	.endw
	retn

handle_char:
	cmp al,10
	jnz @F
	mov al,13
	call @F
	mov al,10
@@:
	.if bOMode & 1
		mov dl, al
		mov ah, 2
		int 21h
	.elseif bOMode & 2
		call VioPutChar
	.else
		mov dl, al
		xor ax, ax
		int 41h
	.endif
	retn
	align 4
vprintf endp

ifdef _DEBUG

;--- DS must be dgroup.
;--- VioOut expects ES=dgroup.
;--- SS is unknown.

	alias <___dprintf> = <_dprintf>

_dprintf PROC c public fmt:ptr sbyte, args:VARARG

	cmp bInit, 1
	jz @F
	mov bInit, 1
	mov ax, 04Fh
	int 41h
	cmp ax, 0F386h
	jnz @F
	mov bOMode, 4	; output to debugger
@@:
	push eax
	push ecx
	push edx
	invoke vprintf, fmt, addr args
	pop edx
	pop ecx
	pop eax
	ret

_dprintf ENDP
endif

;--- in case the CRT printf is NOT to be used, activate the switch below
ifndef DJGPP	; for Open Watcom only

printf proc syscall public fstr:ptr, args:vararg
	invoke vprintf, fstr, addr args
	ret
printf endp

endif

	end
