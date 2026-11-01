
# create the drivers with DJGPP and JWasm/JWlink.

ifndef debug
debug=0
endif

ifeq ($(debug),1)
OUTD=djdrvd
C_DEBUG_FLAGS=-D_DEBUG
A_DEBUG_FLAGS=-D_DEBUG -Fl=$(OUTD)/
else
OUTD=djdrv
C_DEBUG_FLAGS=
A_DEBUG_FLAGS=
endif

vpath_src=srcdrv
vpath_header=srcdrv

vpath %.c $(vpath_src)
vpath %.h $(vpath_header)
vpath_obj=./$(OUTD)/
vpath %.o $(vpath_obj)

HLPOBJ = \
	$(OUTD)/physmem.o   $(OUTD)/dmabuff.o  $(OUTD)/pcibios.o  $(OUTD)/timer.o   $(OUTD)/ac97mix.o \
	$(OUTD)/cthw20k1.o  $(OUTD)/cthw20k2.o $(OUTD)/sblive.o   $(OUTD)/sblive24.o \
	$(OUTD)/dpmi30x.o   $(OUTD)/dpmi80x.o  $(OUTD)/dpmi507.o  $(OUTD)/dprintf.o  $(OUTD)/vioout.o \
	$(OUTD)/libmain32.o $(OUTD)/malloc.o   $(OUTD)/sbrk.o     $(OUTD)/djcompat.o

C_OPT_FLAGS=-Os -fno-asynchronous-unwind-tables
C_EXTRA_FLAGS=-march=i586 -fshort-enums -DDYNAMIC

INCLUDES=-Isrcdrv -Iinclude
LIBS=$(OUTD)/drvhlp.ar

$(OUTD)/%.o: srcdrv/%.c
	@gcc $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -c $< -o $@

$(OUTD)/%.o: srcdrv/%.asm
	@jwasm.exe -q -djgpp -Isrcdrv/startup $(A_DEBUG_FLAGS) -Fo=$@ $<

$(OUTD)/%.o: srcdrv/startup/%.asm
	@jwasm.exe -q -djgpp -Isrcdrv/startup -DDJGPP -D?MODEL=flat $(A_DEBUG_FLAGS) -Fo=$@ $<

all:: $(OUTD) $(OUTD)/CS4281.DRV $(OUTD)/ES1370.DRV $(OUTD)/ES1371.DRV $(OUTD)/HDA.DRV $(OUTD)/ICH.DRV $(OUTD)/SBGEN.DRV $(OUTD)/SBXFI.DRV $(OUTD)/VIA82.DRV

$(OUTD):
	@mkdir $(OUTD)

# 2 link steps used; LD links a relocatable binary, jwlink links it to a DOS MZ binary.

$(OUTD)/CS4281.DRV: $(OUTD)/sc_cs4281.o  $(LIBS)
	@ld -r $(OUTD)/sc_cs4281.o $(OUTD)/LibMain32.o $(LIBS) -o $(OUTD)/tmp -T djdrv.ld -Map $@.mp --defsym __sndcard_info=_CS4281_sndcard_info
	@jwlink format dos f $(OUTD)/tmp. n $@ disable 1014 op q,m=$@.map,start=start
	@del $(OUTD)\tmp

$(OUTD)/ES1370.DRV: $(OUTD)/sc_e1370.o  $(LIBS)
	@ld -r $(OUTD)/sc_e1370.o $(OUTD)/LibMain32.o $(LIBS) -o $(OUTD)/tmp -T djdrv.ld -Map $@.mp --defsym __sndcard_info=_ES1370_sndcard_info
	@jwlink format dos f $(OUTD)/tmp. n $@ disable 1014 op q,m=$@.map,start=start
	@del $(OUTD)\tmp

$(OUTD)/ES1371.DRV: $(OUTD)/sc_e1371.o  $(LIBS)
	@ld -r $(OUTD)/sc_e1371.o $(OUTD)/LibMain32.o $(LIBS) -o $(OUTD)/tmp -T djdrv.ld -Map $@.mp --defsym __sndcard_info=_ES1371_sndcard_info
	@jwlink format dos f $(OUTD)/tmp. n $@ disable 1014 op q,m=$@.map,start=start
	@del $(OUTD)\tmp

$(OUTD)/HDA.DRV: $(OUTD)/sc_inthd.o  $(LIBS)
	@ld -r $(OUTD)/sc_inthd.o $(OUTD)/LibMain32.o $(LIBS) -o $(OUTD)/tmp -T djdrv.ld -Map $@.mp --defsym __sndcard_info=_HDA_sndcard_info
	@jwlink format dos f $(OUTD)/tmp. n $@ disable 1014 op q,m=$@.map,start=start
	@del $(OUTD)\tmp

$(OUTD)/ICH.DRV: $(OUTD)/sc_ich.o  $(LIBS)
	@ld -r $(OUTD)/sc_ich.o   $(OUTD)/LibMain32.o $(LIBS) -o $(OUTD)/tmp -T djdrv.ld -Map $@.mp --defsym __sndcard_info=_ICH_sndcard_info
	@jwlink format dos f $(OUTD)/tmp. n $@ disable 1014 op q,m=$@.map,start=start
	@del $(OUTD)\tmp

$(OUTD)/SBGEN.DRV: $(OUTD)/sc_sbgen.o  $(LIBS)
	@ld -r $(OUTD)/sc_sbgen.o $(OUTD)/LibMain32.o $(LIBS) -o $(OUTD)/tmp -T djdrv.ld -Map $@.mp --defsym __sndcard_info=_SBGEN_sndcard_info
	@jwlink format dos f $(OUTD)/tmp. n $@ disable 1014 op q,m=$@.map,start=start
	@del $(OUTD)\tmp

$(OUTD)/SBXFI.DRV: $(OUTD)/sc_sbxfi.o  $(LIBS)
	@ld -r $(OUTD)/sc_sbxfi.o $(OUTD)/LibMain32.o $(LIBS) -o $(OUTD)/tmp -T djdrv.ld -Map $@.mp --defsym __sndcard_info=_SBXFI_sndcard_info
	@jwlink format dos f $(OUTD)/tmp. n $@ disable 1014 op q,m=$@.map,start=start
	@del $(OUTD)\tmp

$(OUTD)/VIA82.DRV: $(OUTD)/sc_via82.o  $(LIBS)
	@ld -r $(OUTD)/sc_via82.o $(OUTD)/LibMain32.o $(LIBS) -o $(OUTD)/tmp -T djdrv.ld -Map $@.mp --defsym __sndcard_info=_VIA82XX_sndcard_info
	@jwlink format dos f $(OUTD)/tmp. n $@ disable 1014 op q,m=$@.map,start=start
	@del $(OUTD)\tmp

$(OUTD)/drvhlp.ar: $(HLPOBJ)
	@ar --target=coff-go32 r $@ $(HLPOBJ)

$(OUTD)/sc_cs4281.o:: srcdrv/sc_cs4281.c include/auistruc.h dmabuff.h pcibios.h ac97mix.h
$(OUTD)/sc_e1370.o:: sc_e1370.c  include/auistruc.h dmabuff.h pcibios.h ac97mix.h
$(OUTD)/sc_e1371.o:: sc_e1371.c  include/auistruc.h dmabuff.h pcibios.h
$(OUTD)/SC_INTHD.o:: sc_inthd.c  include/auistruc.h dmabuff.h pcibios.h
$(OUTD)/SC_ICH.o::   sc_ich.c    include/auistruc.h dmabuff.h pcibios.h ac97mix.h
$(OUTD)/SC_SBGEN.o:: sc_sbgen.c  include/auistruc.h dmabuff.h pcibios.h
$(OUTD)/SC_SBXFI.o:: sc_sbxfi.c  include/auistruc.h dmabuff.h pcibios.h
$(OUTD)/SC_VIA82.o:: sc_via82.c  include/auistruc.h dmabuff.h pcibios.h

$(OUTD)/ac97mix.o::  ac97mix.c   include/auistruc.h ac97mix.h
$(OUTD)/dmabuff.o::  dmabuff.c   include/auistruc.h dmabuff.h
$(OUTD)/pcibios.o::  pcibios.c   pcibios.h
$(OUTD)/physmem.o::  physmem.c
$(OUTD)/timer.o::    timer.c     include/auistruc.h timer.h

$(OUTD)/dprintf.o::  srcdrv/startup/dprintf.asm
$(OUTD)/vioout.o::   srcdrv/startup/vioout.asm
$(OUTD)/libmain32.o::srcdrv/startup/libmain32.asm
$(OUTD)/djcompat.o:: srcdrv/startup/djcompat.asm

clean::
	del $(OUTD)\*.DRV
	del $(OUTD)\*.lib
	del $(OUTD)\*.map
	del $(OUTD)\*.o
	del $(OUTD)\*.obj

