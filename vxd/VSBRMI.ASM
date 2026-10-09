;--- VSBRMI.ASM: real mode initialization of VSBVXD (the LE entry point,
;--- called by WIN386 before it switches to protected mode).  Nothing to do
;--- (WIN386 has allocated all XMS memory already at this point):
;--- AX = 0 (Device_Load_Ok), BX = 0 (no excluded pages),
;--- SI = 0 (no instance data), EDX = reference data (0).

	.386
_RCODE segment word use16 public 'RCODE'
	assume cs:_RCODE

rmi_start:
	xor ax, ax
	xor bx, bx
	xor si, si
	xor edx, edx
	ret

_RCODE ends

	end rmi_start
