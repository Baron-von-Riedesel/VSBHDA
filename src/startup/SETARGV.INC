
;--- setup argc and argv.
;--- create an argc/argv structure on the stack.
;--- in: ES=FLAT, DS=FLAT, SS=FLAT
;---     cmdline (asciiz terminated) in _LpCmdLine
;--- all std registers modified (including ESP)

?QUOTES  equ 1 ;1=handle double quotes

_setargv proc private

    mov ebp, esp
    sub esp, 256            ; just make enough room for argc/argv

    xor edi, edi            ; init argc
    xor edx, edx            ; init size of mem block
    mov esi, [_LpCmdLine]
    jmp scanarg

;--- EDI = argc
;--- EDX = block size (not including null terminators)
nextarg:
    push Ebx                ; save argument size
scanarg:
@@:
    lodsb
    cmp al, ' '
    je @B
    cmp al, 9
    je @B
    cmp al, 0
    jz doneargs             ; exit if eol
    inc edi                 ; another argument
    xor ebx, ebx            ; init argument size
if ?QUOTES
    cmp al, '"'
    jz handle_quote
endif
    dec esi                 ; back up to reload character
    push esi                ; save argument ofs
@@:
    lodsb
    cmp al, ' '             ; end argument?
    je nextarg
    cmp al, 9
    je nextarg              ; white space terminates argument
    cmp al, 0
    jz doneargs2            ; exit if eol
    inc ebx
    inc edx
    jmp @B
if ?QUOTES
handle_quote:
    push esi
@@:
    lodsb
    cmp al, 13
    jz quoteerr
    cmp al, '"'
    jz @F
    inc edx
    inc ebx
    jmp @B
quoteerr:
    dec esi                 ; "unread" the CR
@@:
    jmp nextarg
endif
doneargs2:
    push ebx                ; last argument's size
doneargs:

;--- address & size of arguments are pushed

    mov ecx, edi
    add edx, edi            ; EDX=size arguments + terminator bytes
    inc edi                 ; add one for NULL pointer
    inc edi                 ; add one for filename
    shl edi, 2              ; each ofs needs 4 bytes
    add edx, edi            ; EDX=size args + size argv
    add edx, 3
    and edx, -4             ; ensure stack remains dword aligned
    mov eax, [ebp]
    sub ebp, edx            ; alloc the really needed space for argc/argv
    mov [ebp-4], eax        ; store return address

    mov [_argc], ecx
    mov [_argv], ebp

    add edi, ebp            ; edi -> behind vector table (strings)
    xor eax, eax
    lea ebx, [edi-4]
    mov [ebx], eax          ; terminating 0000 _argv[x]
    sub ebx, 4
    jecxz noargs

;--- copy the arguments from PSP onto the stack

    mov edx, ecx
@@:
    pop ecx                 ; size
    pop esi                 ; address
    mov [ebx], edi          ; store _argv[x]
    sub ebx, 4
    rep movsb
    stosb                   ; AL still 0
    dec edx
    jnz @B

noargs:
    mov eax, _LpPgmName
    mov [ebx], eax          ; store 0 as dummy filename
    inc dword ptr [_argc]
    lea esp, [ebp-4]
    ret
_setargv endp

