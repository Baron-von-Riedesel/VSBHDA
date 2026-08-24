
;--- display sound pci devices
;--- optionally set Irq in PCI config space
;--- create binary with: jwasm -mz pcisnd.asm

	.286
	.model tiny
	.stack 4096
	.dosseg
	.386

lf equ 10

CStr macro text:vararg
local sym
	.const
sym db text,0
	.code
	exitm <offset sym>
endm

	.data

bCnt db 0
bIrq db -1
bHelp db 0

	.code

	include printf.inc

listdev proc stdcall uses si di class:word
	mov si, 0
nextdev:
	movzx ecx, class
	shl ecx, 8
	mov ax, 0B103h
	int 1Ah
	jc done
	cmp ah, 0
	jnz done

	.if !bCnt
		invoke printf, CStr("cls/subcls bus/dev/func Irq vendor/dev",lf)
		invoke printf, CStr("---------------------------------------",lf)
	.endif

;--- get interrupt line
	mov di, 3Ch
	mov ax, 0B108h
	int 1Ah

	push cx
	push bx
	mov dx, class
	mov ch, bl
	and ch, 7
	shr bl, 3
	invoke printf, CStr("%3u/%2u %7u/%3u/%2u %5d"), dh, dl, bh, bl, ch, cl
	pop bx
	mov di, 0
	mov ax, 0B10Ah
	int 1Ah
	shld eax, ecx, 16
	invoke printf, CStr("   %04X/%04X",lf), cx, ax
	pop cx

;--- if Irq isn't set yet and option -i has been entered,
;--- change Irq in PCI config space.

	.if bIrq != -1 && (cl == 0 || cl == 0ffh)
		mov cl, bIrq
		mov di, 3Ch
		mov ax, 0B10Bh
		int 1Ah
		invoke printf, CStr("IRQ changed to %u!",lf), bIrq
	.endif

	inc bCnt
	inc si
	jmp nextdev
done:
	ret
listdev endp

scancmdline proc uses ds
;--- scan cmdline for an option
	mov si, 81h
	push es
	pop ds
	assume ds:nothing
	cld
nextchr:
	lodsb
	cmp al, 13
	jz done
	cmp al, ' '
	jz nextchr
	cmp al, 9
	jz nextchr
	cmp al, '-'
	jz is_opt
	cmp al,'/'
	jnz help
is_opt:
	lodsb
	cmp al, 13
	jz help
	or al, 20h
	cmp al,'i'
	jz is_opti
	jmp help
done:
	ret
help:
	mov bHelp, 1
	ret
is_opti:
	lodsb
	call chkdigit
	jc help
	mov bIrq, al
	lodsb
	call chkdigit
	jnc @F
	dec si
	jmp nextchr
@@:
	mov cl,al
	mov al,bIrq
	mov ah,10
	mul ah
	add al,cl
	cmp al,15
	ja help
	mov bIrq,al
	jmp nextchr
chkdigit:
	cmp al,'0'
	jb @F
	cmp al,'9'+1
	cmc
	jb @F
	sub al,'0'
@@:
	retn
	assume ds:DGROUP
    
scancmdline endp

	.const

szHelp label byte
	db "Usage:",lf
	db "PCISND [options]",lf
	db "options:",lf
	db " -i<n>: set IRQ if not set yet [n=0-15]"
	db 0
szErr1 db "no PCI BIOS",0

	.code

main proc c

	invoke printf, CStr("PCISND v1.0, displays all PCI sound devices.",lf,lf)
	call scancmdline
	cmp bHelp, 1
	jz help
	mov ax, 0B101h
	int 1Ah
	cmp ah, 00
	jnz err1
	cmp edx, " ICP"
	jnz err1
	invoke listdev, 403h
	invoke listdev, 401h
	.if bCnt == 0
		invoke printf, CStr("no PCI sound devices found",lf)
	.endif
	mov al, 0
	ret
help:
	mov dx, offset szHelp
	jmp errout
err1:
	mov dx, offset szErr1
errout:
	invoke printf, CStr("%s",lf), dx
	mov al, 1
	ret

main endp

start:
	push cs		; setup a tiny memory model (CS=SS=DS=DGROUP)
	pop ds
	mov ax, ss
	mov dx, cs
	sub ax, dx
	shl ax, 4
	push cs
	pop ss
	add sp, ax
	mov bx, sp
	shr bx, 4
	add bx, 10h
	mov ah, 4Ah	; free unused memory
	int 21h
	invoke main
	mov ah, 4ch
	int 21h

	END start
