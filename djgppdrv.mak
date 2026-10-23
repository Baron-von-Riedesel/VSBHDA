
# create the drivers with DJGPP and JWasm/JWlink.

# THIS DOESN'T WORK YET - jwlink has problems with djgpp's relocations.

# OTOH, DJGPP's ld rejects the OMF modules that JWasm created.
# it's possible to make JWasm create coff modules - those ld will
# accept. However, since memory model tiny (32-bit) is used, a few source
# adjustments will be necessary.

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
C_EXTRA_FLAGS=-march=i586 -fshort-enums

INCLUDES=-Isrcdrv -Iinclude
LIBS=$(OUTD)\drvhlp.lib

$(OUTD)/%.o: srcdrv/%.c
	gcc $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -c $< -o $@

$(OUTD)/%.o: srcdrv/%.asm
	jwasm.exe -q      -Isrcdrv/startup $(A_DEBUG_FLAGS) -Fo=$@ $<

$(OUTD)/%.o: srcdrv/startup/%.asm
	jwasm.exe -q      -Isrcdrv/startup $(A_DEBUG_FLAGS) -Fo=$@ $<

all:: $(OUTD) $(OUTD)/ES1370.DRV $(OUTD)/HDA.DRV

$(OUTD):
	@mkdir $(OUTD)

$(OUTD)/ES1370.DRV: $(OUTD)/SC_E1370.o  $(OUTD)/drvhlp.lib
	@jwlink format dos \
file $(OUTD)/SC_E1370.o,$(OUTD)/LibMain32.o \
name $@ \
alias __sndcard_info=_ES1370_sndcard_info \
lib { $(LIBS) } \
disable 1014 \
op q,statics,m=$(OUTD)/ES1370.map,nodefaultlibs

$(OUTD)/HDA.DRV: $(OUTD)/SC_INTHD.o  $(OUTD)/drvhlp.lib
	@jwlink format dos \
file $(OUTD)/SC_INTHD.o,$(OUTD)/LibMain32.o \
name $@ \
alias __sndcard_info=_HDA_sndcard_info \
lib { $(LIBS) } \
disable 1014 \
op q,statics,m=$(OUTD)/HDA.map,nodefaultlibs

$(OUTD)/drvhlp.lib: $(HLPOBJ)
	jwlib -q -b -n $@ $(HLPOBJ)

$(OUTD)/SC_E1370.o:: sc_e1370.c  include/auistruc.h dmabuff.h pcibios.h ac97mix.h
	gcc $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -c $< -o $@
$(OUTD)/SC_INTHD.o:: sc_inthd.c  include/auistruc.h dmabuff.h pcibios.h
	gcc $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -c $< -o $@

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
	del $(OUTD)\$*.drv
	del $(OUTD)\$(NAME).ar
	del $(OUTD)\*.o

