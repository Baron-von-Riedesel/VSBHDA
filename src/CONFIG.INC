
SB16      equ 1	; 1=SB16 supported
VMPU      equ 1	; 1=VMPU supported
SLOWDOWN  equ 1	; 1=SL option supported
SOUNDFONT equ 1	; 1=Soundfonts supported

TRAPINT21 equ 1	; 1=hook int 21h
SETIF     equ 1	; 1=enable interrupts inside sound isr (setting should match the one in config.h)
COMPAT4   equ 1	; 1=support /CF4

CF_STIONEOI equ 2
CF_NODIVERR equ 8

ifdef NOTFLAT

SEGOFS equ 2
insIRET textequ <iret>
insPUSHF textequ <pushf>
PFAR typedef ptr far16
FIXEXC00 equ 1	;1=fix borland exc 00 bug

IRETS struct
_ip	dw ?
_cs	dw ?
_fl	dw ?
IRETS ends

EXCFRAME struct	;DPMI v0.9 exception frame
_dpmiip    dw ?
_dpmics    dw ?
_errcode   dw ?
_ip        dw ?
_cs        dw ?
_fl        dw ?
_sp        dw ?
_ss        dw ?
EXCFRAME ends

externdef c _csalias:dword

else

SEGOFS equ 4
insIRET textequ <iretd>
insPUSHF textequ <pushfd>
PFAR typedef ptr far32

IRETS struct
_ip dd ?
_cs dd ?
_fl dd ?
IRETS ends

EXCFRAME struct	;DPMI v0.9 exception frame
_dpmiip    dd ?
_dpmics    dd ?
_errcode   dd ?
_ip        dd ?
_cs        dd ?
_fl        dd ?
_sp        dd ?
_ss        dd ?
EXCFRAME ends

endif

ifdef DJGPP
_Go32_Info_Block struct
	dd 6 dup (?)
	db ?,?
	dw ?
	dd ?
linear_address_of_original_psp dd ?
	dw ?,?
_Go32_Info_Block ends
endif

PUSHADS struct
_edi	dd ?
_esi	dd ?
_ebp	dd ?
		dd ?
_ebx	dd ?
_edx	dd ?
_ecx	dd ?
_eax	dd ?
PUSHADS ends

RMCS struct     ;real mode call structure
rEDI    dd ?    ;+0
rESI    dd ?    ;+4
rEBP    dd ?    ;+8
        dd ?    ;+12
rEBX    dd ?    ;+16
rEDX    dd ?    ;+20
rECX    dd ?    ;+24
rEAX    dd ?    ;+28
rFlags  dw ?    ;+32
rES     dw ?    ;+34
rDS     dw ?    ;+36
rFS     dw ?    ;+38    
rGS     dw ?    ;+40  
union
rCSIP   dd ?
 struct
rIP     dw ?
rCS     dw ?
 ends
ends
union
rSSSP   dd ?
 struct
rSP     dw ?
rSS     dw ?
 ends
ends
RMCS ends

externdef oldint31:PFAR

GLOBALVARS struct
	dd 3 dup (?)
if SB16
	dd ?
endif
	dd ?
if VMPU
	dd ?
endif
	dd 5 dup (?)
if SLOWDOWN
	dd ?
endif
if SOUNDFONT
	dd ?
	dd ?
endif
compatflags	dd ?	; /* compatibility flags */
	dd 3 dup (?)
GLOBALVARS ends

