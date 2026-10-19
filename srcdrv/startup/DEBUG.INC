
ifdef _DEBUG

_dprintf proto c :ptr sbyte, :vararg
@dprintf textequ <invoke _dprintf,>

else

@dprintf textequ <;>

endif

OM_DOS      equ 1
OM_DIRECT   equ 2
OM_DEBUGGER equ 4
OM_BUFFERED equ 8

ifndef CStr

CStr macro text:vararg
local sym
	.const
sym db text,0
	.code
	exitm <offset sym>
endm

endif
