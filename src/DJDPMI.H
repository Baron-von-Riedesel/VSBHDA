
/* C interface for __dpmi_xxx replacement functions implemented by djdpmi.asm */

#ifdef cplusplus
extern "C" {
#endif

typedef union {
  struct {
    unsigned long edi;
    unsigned long esi;
    unsigned long ebp;
    unsigned long res;
    unsigned long ebx;
    unsigned long edx;
    unsigned long ecx;
    unsigned long eax;
  } d;
  struct {
    unsigned short di, di_hi;
    unsigned short si, si_hi;
    unsigned short bp, bp_hi;
    unsigned short res, res_hi;
    unsigned short bx, bx_hi;
    unsigned short dx, dx_hi;
    unsigned short cx, cx_hi;
    unsigned short ax, ax_hi;
    unsigned short flags;
    unsigned short es;
    unsigned short ds;
    unsigned short fs;
    unsigned short gs;
    unsigned short ip;
    unsigned short cs;
    unsigned short sp;
    unsigned short ss;
  } x;
  struct {
    unsigned char edi[4];
    unsigned char esi[4];
    unsigned char ebp[4];
    unsigned char res[4];
    unsigned char bl, bh, ebx_b2, ebx_b3;
    unsigned char dl, dh, edx_b2, edx_b3;
    unsigned char cl, ch, ecx_b2, ecx_b3;
    unsigned char al, ah, eax_b2, eax_b3;
  } h;
} __dpmi_regs;

typedef struct {
  unsigned long handle;
  unsigned long size;
  unsigned long address;
} __dpmi_meminfo;

typedef struct {
  unsigned short offset16;
  unsigned short segment;
} __dpmi_raddr;

int __dpmi_get_segment_base_address(int _selector, unsigned long *_addr);
int __dpmi_set_segment_limit(int _selector, unsigned long _limit);
int __dpmi_free_dos_memory(int _selector);
int __dpmi_simulate_real_mode_interrupt(int _vector, __dpmi_regs *_regs);
int __dpmi_simulate_real_mode_procedure_retf(__dpmi_regs *_regs);
int __dpmi_simulate_real_mode_procedure_iret(__dpmi_regs *_regs);
int __dpmi_free_real_mode_callback(__dpmi_raddr *_addr);
int __dpmi_free_memory(unsigned long _handle);
int __dpmi_allocate_linear_memory(__dpmi_meminfo *_info, int _commit);
int __dpmi_set_page_attr( unsigned long handle, unsigned long ofs, unsigned long pages, short attr);
int __dpmi_map_physical_device(unsigned long handle, unsigned long ofs, unsigned long pages, unsigned long physaddr);
int __dpmi_physical_address_mapping(__dpmi_meminfo *_info);
int __dpmi_free_physical_address_mapping(__dpmi_meminfo *_info);
int __dpmi_set_coprocessor_emulation(int _flags);

#ifdef cplusplus
}
#endif
