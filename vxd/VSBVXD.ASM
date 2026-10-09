;--- VSBVXD.ASM: Windows 3.1 enhanced mode virtual device for the VSBHDA
;--- Sound Blaster emulation.  Device descriptor block, system control
;--- dispatcher and C callable wrappers for the VMM/VPICD/VDMAD services.
;--- The C side (VXDMAIN.C) uses the cdecl convention (OW -ecc).

	.386p
	.model flat, c
	option casemap:none

	include VXD.INC

;--- C functions
vsb_sys_critical_init proto c :dword
vsb_device_init       proto c
vsb_init_complete     proto c
vsb_system_exit       proto c
vsb_destroy_vm        proto c :dword
vsb_io                proto c :dword, :dword, :dword, :dword
vsb_hwint             proto c :dword
vsb_virt_eoi          proto c :dword, :dword
vsb_dma_changed       proto c :dword, :dword
vsb_timer             proto c
vsb_pm_api            proto c :dword, :dword
vsb_control           proto c :dword, :dword

	.code


;--- All entries from the VMM clear the direction flag: the C code (and its
;--- memcpy/memset with rep movs/stos) assumes DF = 0, but e.g. a hardware
;--- interrupt can arrive while DOS or Windows code runs with DF = 1 - the
;--- copies then go backwards (heap data in the sound buffer).

;--- system control: EAX = message, EBX = VM handle; CF set = failure

control_proc proc
	cld
	push eax
	push edx
	add al,'0'
	mov dx,0E9h
	out dx,al
	pop edx
	pop eax
	cmp eax, SYS_CRITICAL_INIT
	jz m_sci
	cmp eax, DEVICE_INIT
	jz m_di
	cmp eax, INIT_COMPLETE
	jz m_ic
	cmp eax, SYSTEM_EXIT
	jz m_sx
	cmp eax, DESTROY_VM
	jz m_dvm
	pushad
	invoke vsb_control, eax, ebx
	popad
	clc
	ret
m_sci:
	pushad
	invoke vsb_sys_critical_init, edx	;EDX = reference data of VSBRMI.ASM
	jmp done
m_di:
	pushad
	call vsb_device_init
	jmp done
m_ic:
	pushad
	call vsb_init_complete
	jmp done
m_sx:
	pushad
	call vsb_system_exit
	jmp done
m_dvm:
	pushad
	invoke vsb_destroy_vm, ebx
done:
	cmp eax, 1	;C returns 0 = ok -> CF=1 only if eax != 0
	cmc
	popad
	ret
control_proc endp

;--- I/O trap handler (one for all trapped ports):
;--- EAX = data (out), EBX = VM, ECX = type, EDX = port, EBP = client regs;
;--- out: EAX = data (in).  Word, dword and string accesses are split into
;--- byte accesses by Simulate_IO, which calls this handler again.

io_handler proc
	cld
	test ecx, not OUTPUT
	jnz split
	push ecx
	push edx
	invoke vsb_io, ebx, ecx, edx, eax
	pop edx
	pop ecx
	ret
split:
	VMMcall Simulate_IO
	ret
io_handler endp

;--- protected mode API (VSBHDA.DRV): EBX = VM, EBP = client registers
pm_api_proc proc
	pushad
	cld
	invoke vsb_pm_api, ebx, ebp
	popad
	ret
pm_api_proc endp

;--- VPICD callbacks: EAX = IRQ handle, EBX = VM

hwint_proc proc
	pushad
	cld
	invoke vsb_hwint, eax	;returns 0 = interrupt was ours
	cmp eax, 1
	cmc
	popad
	ret
hwint_proc endp

;--- hardware handler of the emulated SB IRQ (there should be no device)
sbhw_proc proc
	VxDcall VPICD_Phys_EOI
	clc
	ret
sbhw_proc endp

veoi_proc proc
	pushad
	cld
	invoke vsb_virt_eoi, eax, ebx
	popad
	ret
veoi_proc endp

;--- VDMAD callback: EAX = DMA handle, EBX = VM

dma_proc proc
	pushad
	cld
	invoke vsb_dma_changed, eax, ebx
	popad
	ret
dma_proc endp

;--- global time-out callback: calls vsb_timer(), which returns the ms
;--- until the next call (0 = no more)
timer_proc proc
	pushad
	cld
	call vsb_timer
	or eax, eax
	jz @F
	mov esi, offset timer_proc
	xor edx, edx
	VMMcall Set_Global_Time_Out
@@:
	popad
	ret
timer_proc endp

;--- wrappers ---------------------------------------------------------

;--- void vxd_start_timer(unsigned ms)
vxd_start_timer proc uses esi ms:dword
	mov eax, ms
	xor edx, edx
	mov esi, offset timer_proc
	VMMcall Set_Global_Time_Out
	ret
vxd_start_timer endp


vxd_get_sys_vm proc uses ebx
	VMMcall Get_Sys_VM_Handle
	mov eax, ebx
	ret
vxd_get_sys_vm endp

vxd_get_cur_vm proc uses ebx
	VMMcall Get_Cur_VM_Handle
	mov eax, ebx
	ret
vxd_get_cur_vm endp

vxd_get_time proc
	VMMcall Get_System_Time	;EAX = ms since Windows started
	ret
vxd_get_time endp

;--- void *vxd_heap_alloc(unsigned size): zero initialized
vxd_heap_alloc proc size_:dword
	push 1	;HEAPZEROINIT
	push size_
	VMMcall _HeapAllocate
	add esp, 2*4
	ret
vxd_heap_alloc endp

vxd_heap_free proc p:dword
	push 0
	push p
	VMMcall _HeapFree
	add esp, 2*4
	ret
vxd_heap_free endp

;--- void *vxd_page_alloc(unsigned pages): fixed, zero-initialized system memory
vxd_page_alloc proc uses ebx pages:dword
	push 1 or 8		;PageZeroInit, PageFixed
	push 0			;PhysAddrPtr
	push 0			;max phys page
	push 0			;min phys page
	push 0			;align mask
	push 0			;VM (not used for PG_SYS)
	push 1			;PG_SYS
	push pages
	VMMcall _PageAllocate
	add esp, 8*4
	or eax, eax
	jz @F
	mov eax, edx		;linear address
@@:
	ret
vxd_page_alloc endp

;--- uint32_t vxd_lin_to_phys(void *lin): physical address (PTE)
vxd_lin_to_phys proc lin:dword
	local pte:dword
	push 0
	lea eax, pte
	push eax
	push 1
	mov eax, lin
	shr eax, 12
	push eax
	VMMcall _CopyPageTable
	add esp, 4*4
	mov eax, pte
	and eax, 0FFFFF000h
	mov ecx, lin
	and ecx, 0FFFh
	or eax, ecx
	ret
vxd_lin_to_phys endp

;--- void *vxd_map_phys(unsigned phys, unsigned size): -1 on error
vxd_map_phys proc physaddr:dword, size_:dword
	push 0
	push size_
	push physaddr
	VMMcall _MapPhysToLinear
	add esp, 3*4
	ret
vxd_map_phys endp

;--- int vxd_profile_int(const char *key, int default): [VSBVXD] in SYSTEM.INI
vxd_profile_int proc uses esi edi key:dword, def:dword
	mov eax, def
	mov esi, offset szSection
	mov edi, key
	VMMcall Get_Profile_Decimal_Int
	ret
vxd_profile_int endp

;--- const char *vxd_profile_str(const char *key): NULL if not found
vxd_profile_str proc uses esi edi key:dword
	mov esi, offset szSection
	mov edi, key
	VMMcall Get_Profile_String
	mov eax, edx
	jnc @F
	xor eax, eax
@@:
	ret
vxd_profile_str endp

;--- int vxd_install_io(unsigned port): 0 = ok
vxd_install_io proc uses esi port:dword
	mov esi, offset io_handler
	mov edx, port
	VMMcall Install_IO_Handler
	sbb eax, eax
	ret
vxd_install_io endp

;--- unsigned vxd_virtualize_irq(unsigned irq, int hw): IRQ handle, 0 = error
;--- hw: 1 = sound hardware IRQ (shared, hardware handler),
;---     0 = emulated SB IRQ (virtual EOI handler)
vxd_virtualize_irq proc uses edi irq:dword, hw:dword
	local vid:VPICD_IRQ_Descriptor
	lea edi, vid
	push edi
	mov ecx, sizeof VPICD_IRQ_Descriptor / 4
	xor eax, eax
	rep stosd
	pop edi
	mov eax, irq
	mov [edi].VPICD_IRQ_Descriptor.VID_IRQ_Number, ax
	mov [edi].VPICD_IRQ_Descriptor.VID_IRET_Time_Out, 500
	.if hw
		mov [edi].VPICD_IRQ_Descriptor.VID_Options, VPICD_OPT_CAN_SHARE
		mov [edi].VPICD_IRQ_Descriptor.VID_Hw_Int_Proc, offset hwint_proc
	.else
		mov [edi].VPICD_IRQ_Descriptor.VID_Hw_Int_Proc, offset sbhw_proc
		mov [edi].VPICD_IRQ_Descriptor.VID_EOI_Proc, offset veoi_proc
	.endif
	VxDcall VPICD_Virtualize_IRQ
	jnc @F
	xor eax, eax
@@:
	ret
vxd_virtualize_irq endp

vxd_phys_eoi proc irqh:dword
	mov eax, irqh
	VxDcall VPICD_Phys_EOI
	ret
vxd_phys_eoi endp

vxd_phys_unmask proc irqh:dword
	mov eax, irqh
	VxDcall VPICD_Physically_Unmask
	ret
vxd_phys_unmask endp

vxd_set_int_request proc uses ebx irqh:dword, vm:dword
	mov eax, irqh
	mov ebx, vm
	VxDcall VPICD_Set_Int_Request
	ret
vxd_set_int_request endp

vxd_clear_int_request proc uses ebx irqh:dword, vm:dword
	mov eax, irqh
	mov ebx, vm
	VxDcall VPICD_Clear_Int_Request
	ret
vxd_clear_int_request endp

;--- unsigned vxd_virtualize_dma(unsigned channel): DMA handle, 0 = error
vxd_virtualize_dma proc uses esi channel:dword
	mov eax, channel
	mov esi, offset dma_proc
	VxDcall VDMAD_Virtualize_Channel
	jnc @F
	xor eax, eax
@@:
	ret
vxd_virtualize_dma endp

;--- void vxd_get_dma_state(unsigned dmah, unsigned vm, struct dmastate *)
;--- struct dmastate { unsigned addr, count; unsigned char mode, flags; }
vxd_get_dma_state proc uses ebx esi edi dmah:dword, vm:dword, pstate:dword
	mov eax, dmah
	mov ebx, vm
	VxDcall VDMAD_Get_Virt_State
	mov edi, pstate
	mov [edi+0], esi
	mov [edi+4], ecx
	mov [edi+8], dx
	ret
vxd_get_dma_state endp

vxd_set_dma_state proc uses ebx esi edi dmah:dword, vm:dword, pstate:dword
	mov edi, pstate
	mov eax, dmah
	mov ebx, vm
	mov esi, [edi+0]
	mov ecx, [edi+4]
	mov dx, [edi+8]
	VxDcall VDMAD_Set_Virt_State
	ret
vxd_set_dma_state endp

;--- FPU use in ring 0: clear CR0.TS for the time of the use
;--- unsigned vxd_fpu_begin(void *buf108): returns old CR0
vxd_fpu_begin proc buf:dword
	mov eax, cr0
	push eax
	clts
	mov edx, buf
	fnsave [edx]
	fninit
	pop eax
	ret
vxd_fpu_begin endp

vxd_fpu_end proc buf:dword, oldcr0:dword
	mov edx, buf
	frstor [edx]
	mov eax, oldcr0
	mov cr0, eax
	ret
vxd_fpu_end endp

	public VSBVXD_DDB
VSBVXD_DDB VxD_Desc_Block <0, DDK_VERSION, VSBVXD_DEVICE_ID, 1, 0, 0, \
	"VSBVXD  ", UNDEFINED_INIT_ORDER, offset control_proc, 0, offset pm_api_proc, 0, 0, 0, 0, 0>

szSection db "VSBVXD",0

	end
