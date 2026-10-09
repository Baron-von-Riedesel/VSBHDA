;--- VXD.INC: minimal definitions for a Windows 3.1 virtual device driver
;--- (VSBVXD).  Written from the documented interface; service numbers are
;--- the positions in the VMM/VPICD/VDMAD service tables of Windows 3.1.

DDK_VERSION         equ 030Ah
UNDEFINED_DEVICE_ID equ 0
UNDEFINED_INIT_ORDER equ 80000000h

;--- device id of VSBVXD (not assigned by Microsoft): needed for the
;--- protected mode API (Int 2Fh, AX=1684h, BX=id) of the wave driver
VSBVXD_DEVICE_ID equ 7A56h

VMM_DEVICE_ID   equ 0001h
VPICD_DEVICE_ID equ 0003h
VDMAD_DEVICE_ID equ 0004h

;--- device descriptor block (Windows 3.1 layout)
VxD_Desc_Block struct
DDB_Next               dd ?
DDB_SDK_Version        dw ?
DDB_Req_Device_Number  dw ?
DDB_Dev_Major_Version  db ?
DDB_Dev_Minor_Version  db ?
DDB_Flags              dw ?
DDB_Name               db 8 dup (?)
DDB_Init_Order         dd ?
DDB_Control_Proc       dd ?
DDB_V86_API_Proc       dd ?
DDB_PM_API_Proc        dd ?
DDB_V86_API_CSIP       dd ?
DDB_PM_API_CSIP        dd ?
DDB_Reference_Data     dd ?
DDB_Service_Table_Ptr  dd ?
DDB_Service_Table_Size dd ?
VxD_Desc_Block ends

;--- system control messages
SYS_CRITICAL_INIT  equ 00h
DEVICE_INIT        equ 01h
INIT_COMPLETE      equ 02h
SYS_VM_INIT        equ 03h
SYS_VM_TERMINATE   equ 04h
SYSTEM_EXIT        equ 05h
SYS_CRITICAL_EXIT  equ 06h
CREATE_VM          equ 07h
VM_CRITICAL_INIT   equ 08h
VM_INIT            equ 09h
VM_TERMINATE       equ 0Ah
VM_NOT_EXECUTEABLE equ 0Bh
DESTROY_VM         equ 0Ch

;--- VM control block
CB_VM_Status   equ 0
CB_High_Linear equ 4
CB_Client_Pointer equ 8
CB_VMID        equ 0Ch

;--- I/O trap types (ECX of an I/O handler)
BYTE_INPUT  equ 00h
BYTE_OUTPUT equ 04h
OUTPUT      equ 04h
WORD_IO     equ 08h
DWORD_IO    equ 10h
STRING_IO   equ 20h

;--- VPICD virtual IRQ descriptor
VPICD_IRQ_Descriptor struct
VID_IRQ_Number       dw ?
VID_Options          dw ?
VID_Hw_Int_Proc      dd ?
VID_Virt_Int_Proc    dd ?
VID_EOI_Proc         dd ?
VID_Mask_Change_Proc dd ?
VID_IRET_Proc        dd ?
VID_IRET_Time_Out    dd ?
VPICD_IRQ_Descriptor ends
VPICD_OPT_CAN_SHARE  equ 0002h

;--- service ids: device shl 16 + position in the service table
@VMM macro name, num
name equ (VMM_DEVICE_ID shl 16) + num
endm
@VMM Get_VMM_Version,          000h
@VMM Get_Cur_VM_Handle,        001h
@VMM Get_Sys_VM_Handle,        003h
@VMM Get_Next_VM_Handle,       03Bh
@VMM Get_System_Time,          03Fh
@VMM Set_Global_Time_Out,      03Ch
@VMM _HeapAllocate,            04Fh
@VMM _HeapFree,                051h
@VMM _PageAllocate,            053h
@VMM _PageFree,                055h
@VMM _MapPhysToLinear,         06Ch
@VMM _CopyPageTable,           061h
@VMM Simulate_IO,              094h
@VMM Install_IO_Handler,       096h
@VMM Enable_Global_Trapping,   097h
@VMM Disable_Global_Trapping,  099h
@VMM Get_Profile_Decimal_Int,  0ABh
@VMM Get_Profile_String,       0B3h
@VMM Fatal_Error_Handler,      0BEh

@VPICD macro name, num
name equ (VPICD_DEVICE_ID shl 16) + num
endm
@VPICD VPICD_Get_Version,       00h
@VPICD VPICD_Virtualize_IRQ,    01h
@VPICD VPICD_Set_Int_Request,   02h
@VPICD VPICD_Clear_Int_Request, 03h
@VPICD VPICD_Phys_EOI,          04h
@VPICD VPICD_Get_Status,        06h
@VPICD VPICD_Physically_Mask,   08h
@VPICD VPICD_Physically_Unmask, 09h

@VDMAD macro name, num
name equ (VDMAD_DEVICE_ID shl 16) + num
endm
@VDMAD VDMAD_Get_Version,        00h
@VDMAD VDMAD_Virtualize_Channel, 01h
@VDMAD VDMAD_Get_Virt_State,     04h
@VDMAD VDMAD_Set_Virt_State,     05h
@VDMAD VDMAD_Mask_Channel,       07h
@VDMAD VDMAD_UnMask_Channel,     08h

;--- dynamic link: INT 20h followed by the service id (patched by VMM)
VMMcall macro service
	int 20h
	dd service
endm
VxDcall equ <VMMcall>
