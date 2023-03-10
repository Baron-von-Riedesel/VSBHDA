
;--- debug printf

	.386
	.model small
	option casemap:none
	option proc:private

VioPutChar proto

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

dbgprintf PROC c public uses ebx esi edi fmt:ptr sbyte, args:VARARG

local flag:byte
local longarg:byte
local size_:dword
local fillchr:dword
local szTmp[24]:byte

	lea edi,args
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
	xor eax,eax
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
	mov eax,[edi]
	add edi, 4
@@:
	call handle_char
	retn

handle_s:
	mov esi,[edi]
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
	mov eax,[edi]
	add edi,4
	cmp longarg,1
	jnz @F
	mov edx,[edi]
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
	call VioPutChar
	retn
	align 4

dbgprintf ENDP

if 0

_GO32_INFO_BLOCK struct
dwSize            dd ?
dwPrimaryScreen   dd ?
dwSecondaryScreen dd ?
dwTransferBuffer  dd ?	; linear_address_of_transfer_buffer;
dwSizeTransferBuf dd ?	; size_of_transfer_buffer; /* >= 4k */
dwPid             dd ?
bMasterPICBase    db ?	; master_interrupt_controller_base;
bSlavePICBase     db ?	; slave_interrupt_controller_base;
wFlat             dw ?	; selector_for_linear_memory;
dwStubInfo        dd ?	; linear_address_of_stub_info_structure;
dwPSP             dd ?	; linear_address_of_original_psp;
wRunMode          dw ?	; run_mode;
wRunModeInfo      dw ?	; run_mode_info;
_GO32_INFO_BLOCK ends

comm c _go32_info_block:_GO32_INFO_BLOCK

;--- display 32-bit number on screen
;--- low-level.

dbgprintcnt proc c public number:dword, pos:dword

	pushad
	push ds
	mov ds, cs:[_go32_info_block.wFlat]
	mov ebx, pos
	shl ebx, 1
	add ebx, 0B8000h
	mov eax, number
	mov edi, 10
@@nextdigit:
	xor edx, edx
	div edi
	add dl,'0'
	cmp dl,'9'
	jbe @F
	add dl,7+20h
@@:
	mov [ebx],dl
	sub ebx, 2
	and eax, eax
	jne @@nextdigit
	pop ds
exit:
	popad
	ret

dbgprintcnt endp

endif

	end
