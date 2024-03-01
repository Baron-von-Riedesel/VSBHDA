
;--- HDPMI API interface, C compatible

	.386
ifdef ?FLAT
	.MODEL flat
else
	.MODEL small
endif
	option casemap:none
	option proc:private

	assume ds:_DATA

_hdpmi_traphandler struct
dwOfsIn dd ?
wSegIn dw ?
dwOfsOut dd ?
wSegOut dw ?
_hdpmi_traphandler ends

	.data

externdef c HDPMIAPI_Entry: fword

	.code

;--- get hdpmi API entry point

_get_hdpmi_vendor_api proc c public uses es esi edi pAPI:ptr
	xor edi, edi
	mov es, edi
	mov esi, offset szHDPMI
	mov ax, 168ah
	int 2Fh
	cmp al,0
	jnz failed
	mov edx, pAPI
	mov dword ptr [edx+0], edi
	mov word ptr [edx+4], es
failed:
	ret
_get_hdpmi_vendor_api endp

szHDPMI db "HDPMI",0

;--- set hdpmi context mode ( cmdline option -a )

_hdpmi_set_context_mode proc c public uses ebx bMode:byte
	mov bl, bMode
	mov ax, 5
	call HDPMIAPI_Entry
	ret
_hdpmi_set_context_mode endp

;--- install port trap range
;--- the segment parts of the trap handler are set inside the proc!
;--- out: eax=handle or NULL

_hdpmi_install_trap proc c public uses ebx esi start:dword, cnt:dword, pTrapHandler:ptr
	mov edx, start
	mov ecx, cnt
	mov esi, pTrapHandler
	mov [esi]._hdpmi_traphandler.wSegIn, cs
	mov [esi]._hdpmi_traphandler.wSegOut, cs
	mov ax, 6
	call HDPMIAPI_Entry
	jc @F
	ret
@@:
	xor eax, eax
	ret
_hdpmi_install_trap endp

;--- uninstall port trap range
;--- out: eax=0 if failed, else 1

_hdpmi_uninstall_trap proc c public handle:dword
	mov edx, handle
	mov ax, 7
	call HDPMIAPI_Entry
	sbb eax, eax
	inc eax
	ret
_hdpmi_uninstall_trap endp

_hdpmi_simulate_byte_in proc c public uses ebx port:word
	mov bl, 0
	mov dx, port
	mov ax, 8
	call HDPMIAPI_Entry
	ret
_hdpmi_simulate_byte_in endp

_hdpmi_simulate_byte_out proc c public uses ebx port:word, value:byte
	mov bl, 1
	mov cl, value
	mov dx, port
	mov ax, 8
	call HDPMIAPI_Entry
	ret
_hdpmi_simulate_byte_out endp

;--- install CLI handler
;--- just the offset is provided
;--- if it's 0, it's assumed that the handler is to be deinstalled!

_hdpmi_set_cli_handler proc c public uses ebx pHandler:ptr
	xor ecx, ecx
	mov edx, pHandler
	cmp edx, ecx
	jz @F
	mov ecx, cs
@@:
	mov bl, 0
	mov ax, 9
	call HDPMIAPI_Entry
	ret
_hdpmi_set_cli_handler endp

	END
