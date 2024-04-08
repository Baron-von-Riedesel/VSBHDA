
ifdef _DEBUG

__dprintf proto c :ptr, :vararg
@dprintf textequ <invoke __dprintf,>

else

@dprintf textequ <;>

endif

ifndef CStr

CStr macro text:vararg
local sym
	.const
sym db text,0
	.code
	exitm <offset sym>
endm

endif
