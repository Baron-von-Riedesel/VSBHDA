/* linker script for drivers created with DJGPP - see djdrv.mak */
/* OUTPUT_FORMAT("a.out-i386") */
ENTRY (start)
SECTIONS
{
  .text  ALIGN(0x10) : {
    *(.text)
    *(.text.*)
    *(.gnu.linkonce.t*)
    *(.const*)
    *(.ro*)
    *(.gnu.linkonce.r*)
    etext  =  . ; PROVIDE(_etext = .) ;
    . = ALIGN(0x10);
  }
  .data  ALIGN(0x10) : {
    djgpp_first_ctor = . ;
    *(SORT(.ctors.*))
    *(.ctor)
    *(.ctors)
    djgpp_last_ctor = . ;
    djgpp_first_dtor = . ;
    *(SORT(.dtors.*))
    *(.dtor)
    *(.dtors)
    djgpp_last_dtor = . ;
    __environ = . ;
    PROVIDE(_environ = .) ;
    LONG(0) ;
    *(.data)
    *(.data.*)
    /* Ugly workaround to prevent entire .bss to have attribute CONTENT */
    /* for C++ executables. */
    *(.bss.*)
    *(.gcc_exc*)
    ___EH_FRAME_BEGIN__ = . ;
    *(.eh_fram*)
    ___EH_FRAME_END__ = . ;
    LONG(0);
    *(.gnu.linkonce.d*)
    edata  =  . ; PROVIDE(_edata = .) ;
    . = ALIGN(0x10);
  }
  .bss  SIZEOF(.data) + ADDR(.data) :
  {
    *(.bss .gnu.linkonce.b.*)
    *(COMMON)
     end = . ; PROVIDE(_end = .) ;
     . = ALIGN(0x10);
  }
  /* Discard LTO sections.  */
  /DISCARD/ : { *(.gnu.lto_*) }
  /* Stabs debugging sections.  */
  .stab 0 : { *(.stab) }
  .stabstr 0 : { *(.stabstr) }
  /* DWARF debug sections.
     Symbols in the DWARF debugging sections are relative to the beginning
     of the section so we begin them at 0.  */
  /* DWARF 1 */
  .debug          0 : { *(.debug) }
  .line           0 : { *(.line) }
  /* GNU DWARF 1 extensions */
  .debug_srcinfo  0 : { *(.debug_srcinfo) }
  .debug_sfnames  0 : { *(.debug_sfnames) }
  /* DWARF 1.1 and DWARF 2 */
  .debug_aranges  0 : { *(.debug_aranges) }
  .debug_pubnames 0 : { *(.debug_pubnames) }
  /* DWARF 2 */
  .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
  .debug_abbrev   0 : { *(.debug_abbrev) }
  .debug_line     0 : { *(.debug_line .debug_line.* .debug_line_end) }
  .debug_frame    0 : { *(.debug_frame) }
  .debug_str      0 : { *(.debug_str) }
  .debug_loc      0 : { *(.debug_loc) }
  .debug_macinfo  0 : { *(.debug_macinfo) }
  /* SGI/MIPS DWARF 2 extensions */
  .debug_weaknames 0 : { *(.debug_weaknames) }
  .debug_funcnames 0 : { *(.debug_funcnames) }
  .debug_typenames 0 : { *(.debug_typenames) }
  .debug_varnames  0 : { *(.debug_varnames) }
  /* DWARF 3 */
  .debug_pubtypes 0 : { *(.debug_pubtypes) }
  .debug_ranges   0 : { *(.debug_ranges) }
  /* DWARF Extension.  */
  .debug_macro    0 : { *(.debug_macro) }
  .debug_addr     0 : { *(.debug_addr) }
}
