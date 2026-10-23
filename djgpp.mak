
# create vsbhdad.exe with DJGPP and JWasm.
# to create a debug version, enter: make -f djgpp.mak DEBUG=1
# note that JWasm v2.17+ is needed ( understands -djgpp option )
#
# Please note: DJGPP uses CWSDPMI as its DPMI host - this works
# fine so long as just DJGPP tools are launched. However, JWasm is no
# such tool - it expects to run under a "full" DPMI host, which CWSDPMI is not.
# This may cause all sorts of errors if JWasm is launched by DJGPP's make.
# The simplest workaround is to run HDPMI32.EXE with option -r before
# DJGPP's make tool is executed.

# v2.1 introduced low-level hardware routines created as separate modules;
# to ensure that the vsbhda binary and the driver modules are compatible
# option -fshort-enums has to be set (this is the default for OW).
# The low-level drivers itself should be generated with Drivers.mak, using
# Open Watcom.

ifndef debug
debug=0
endif

NAME=VSBHDA

# set to 0 to create the (deprecated) monolithic variant
ifndef DYNAMIC
DYNAMIC=1
endif

ifeq ($(debug),1)
OUTD=djgppd
C_DEBUG_FLAGS=-D_DEBUG
A_DEBUG_FLAGS=-D_DEBUG -Fl=$(OUTD)/
else
OUTD=djgpp
C_DEBUG_FLAGS=
A_DEBUG_FLAGS=
endif

ifndef NOFM
FMOBJS=$(OUTD)/dbopl.o	$(OUTD)/vopl3.o
endif

ifeq ($(DYNAMIC),1)
LOADDRVOBJ=$(OUTD)/loaddrv.o
vpath_src=src
vpath_header=src
else
vpath_src=src srcdrv
vpath_header=src srcdrv
endif

vpath %.c $(vpath_src)
vpath %.cpp $(vpath_src)
vpath %.asm $(vpath_src)
vpath %.h $(vpath_header)
vpath_obj=./$(OUTD)/
vpath %.o $(vpath_obj)

OBJFILES = \
	$(OUTD)/main.o		$(OUTD)/sndisr.o	$(OUTD)/ptrap.o		$(OUTD)/linear.o	$(OUTD)/pic.o \
	$(OUTD)/vsb.o		$(OUTD)/vdma.o		$(OUTD)/virq.o		$(OUTD)/vmpu.o		$(OUTD)/tsf.o \
	$(OUTD)/adpcm.o		$(OUTD)/au_cards.o	$(FMOBJS)			$(LOADDRVOBJ) \
	$(OUTD)/stackio.o	$(OUTD)/stackisr.o	$(OUTD)/sbisr.o		$(OUTD)/int31.o		$(OUTD)/rmwrap.o	$(OUTD)/mixer.o \
	$(OUTD)/hapi.o		$(OUTD)/dprintf.o	$(OUTD)/vioout.o	$(OUTD)/dpmi.o		$(OUTD)/uninst.o \
	$(OUTD)/fileacc.o	$(OUTD)/logfile.o \
	$(OUTD)/dpmiisr.o

ifeq ($(DYNAMIC),0)
OBJFILES += \
	$(OUTD)/ac97mix.o	$(OUTD)/dmabuff.o	$(OUTD)/pcibios.o	$(OUTD)/physmem.o	$(OUTD)/timer.o \
	$(OUTD)/sc_e1371.o	$(OUTD)/sc_ich.o	$(OUTD)/sc_inthd.o	$(OUTD)/sc_via82.o \
	$(OUTD)/sc_sbgen.o	$(OUTD)/sblive.o	$(OUTD)/sblive24.o \
	$(OUTD)/sc_sbxfi.o	$(OUTD)/cthw20k1.o	$(OUTD)/cthw20k2.o \
	$(OUTD)/sc_e1370.o	$(OUTD)/sc_cs4281.o
endif

ifeq ($(DYNAMIC),0)
INCLUDE_DIRS=src srcdrv include
else
INCLUDE_DIRS=src include
endif

#SRC_DIRS=src srcdrv

C_OPT_FLAGS=-Os -fno-asynchronous-unwind-tables
C_EXTRA_FLAGS=-march=i586 -fshort-enums
LD_FLAGS=$(addprefix -Xlinker ,$(LD_EXTRA_FLAGS))
LD_EXTRA_FLAGS=-Map $(OUTD)/$(NAME).map

INCLUDES=$(addprefix -I,$(INCLUDE_DIRS))
LIBS=$(addprefix -l,stdcxx m)

COMPILE.asm.o=jwasm.exe -q -djgpp -Isrc/startup -D?MODEL=small -DDJGPP $(A_DEBUG_FLAGS) -Fo=$@ $<
COMPILE2.asm.o=jwasm.exe -q -zcw -djgpp -Isrc/startup -D?MODEL=small -DDJGPP $(A_DEBUG_FLAGS) -Fo=$@ $<
COMPILE.c.o=gcc $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -c $< -o $@
COMPILE.cpp.o=gcc $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -c $< -o $@

$(OUTD)/%.o: src/%.c
	$(COMPILE.c.o)

$(OUTD)/%.o: src/%.cpp
	$(COMPILE.cpp.o)

$(OUTD)/%.o: src/%.asm
	$(COMPILE.asm.o)

$(OUTD)/%.o: src/startup/%.asm
	$(COMPILE2.asm.o)

ifeq ($(DYNAMIC),0)
$(OUTD)/%.o: srcdrv/%.c
	$(COMPILE.c.o)
endif

all:: $(OUTD) $(OUTD)/$(NAME)D.EXE

$(OUTD):
	@mkdir $(OUTD)

$(OUTD)/$(NAME)D.EXE:: $(OUTD)/$(NAME).ar
	gcc -o $@ $(OUTD)/main.o $(OUTD)/$(NAME).ar $(LD_FLAGS) $(LIBS)
	strip -s $@
	exe2coff $@
	copy /b res\stub.bin + $(OUTD)\$(NAME)D $(OUTD)\$(NAME)D.EXE

$(OUTD)/$(NAME).ar:: $(OBJFILES)
	ar --target=coff-go32 r $(OUTD)/$(NAME).ar $(OBJFILES)

# to avoid problems with 16-bit relocations, the 16-bit code
# is included in binary format into rmwrap.asm.

$(OUTD)/rmwrap.o:: rmwrap.asm rmcode1.asm rmcode2.asm
	jwasm.exe -q -bin -Fl$(OUTD)/ -Fo$(OUTD)/rmcode1.bin src/rmcode1.asm
	jwasm.exe -q -bin -Fl$(OUTD)/ -Fo$(OUTD)/rmcode2.bin src/rmcode2.asm
	jwasm.exe -q -djgpp -D?MODEL=small -DOUTD=$(OUTD) -Fo$@ src/rmwrap.asm

ifeq ($(DYNAMIC),0)
$(OUTD)/ac97mix.o::  ac97mix.c   include/auistruc.h ac97mix.h
$(OUTD)/cthw20k1.o:: cthw20k1.c  cthw20kx.h
$(OUTD)/cthw20k2.o:: cthw20k2.c  cthw20kx.h
$(OUTD)/dmabuff.o::  dmabuff.c   include/auistruc.h dmabuff.h
$(OUTD)/pcibios.o::  pcibios.c   pcibios.h
$(OUTD)/physmem.o::  physmem.c
$(OUTD)/sblive24.o:: sblive24.c  include/auistruc.h dmabuff.h pcibios.h ac97mix.h sc_sbgen.h ca0106.h
$(OUTD)/sblive.o::   sblive.c    include/auistruc.h dmabuff.h pcibios.h ac97mix.h sc_sbgen.h emu10k1.h
$(OUTD)/sc_cs4281.o:: srcdrv/sc_cs4281.c include/auistruc.h dmabuff.h pcibios.h ac97mix.h dpmi.h linear.h
$(OUTD)/sc_e1371.o:: sc_e1371.c  include/auistruc.h dmabuff.h pcibios.h ac97mix.h
$(OUTD)/sc_e1370.o:: sc_e1370.c  include/auistruc.h dmabuff.h pcibios.h ak4531.h
$(OUTD)/sc_ich.o::   sc_ich.c    include/auistruc.h dmabuff.h pcibios.h ac97mix.h
$(OUTD)/sc_inthd.o:: sc_inthd.c  include/auistruc.h dmabuff.h pcibios.h hda.h
$(OUTD)/sc_sbgen.o:: sc_sbgen.c  include/auistruc.h dmabuff.h pcibios.h ac97mix.h sc_sbgen.h 
$(OUTD)/sc_sbxfi.o:: sc_sbxfi.c  include/auistruc.h dmabuff.h pcibios.h ac97mix.h cthw20kx.h
$(OUTD)/sc_via82.o:: sc_via82.c  include/auistruc.h dmabuff.h pcibios.h ac97.h
$(OUTD)/timer.o::    timer.c     include/auistruc.h timer.h
else
$(OUTD)/loaddrv.o::  loaddrv.asm
endif

$(OUTD)/adpcm.o::    adpcm.c     adpcm.h
$(OUTD)/au_cards.o:: au_cards.c  include/auistruc.h config.h
$(OUTD)/dbopl.o::    dbopl.cpp   dbopl.h
$(OUTD)/linear.o::   linear.c    linear.h platform.h
$(OUTD)/main.o::     main.c      linear.h platform.h ptrap.h vopl3.h pic.h config.h vsb.h vdma.h virq.h au_cards.h version.h
$(OUTD)/pic.o::      pic.c       pic.h platform.h ptrap.h
$(OUTD)/ptrap.o::    ptrap.c     linear.h platform.h ptrap.h config.h
$(OUTD)/sndisr.o::   sndisr.c    linear.h platform.h vopl3.h pic.h config.h vsb.h vdma.h virq.h adpcm.h au_cards.h
$(OUTD)/tsf.o::      tsf.c       src/tsf/tsf.h
$(OUTD)/vdma.o::     vdma.c      linear.h platform.h ptrap.h vdma.h config.h
$(OUTD)/virq.o::     virq.c      linear.h platform.h pic.h ptrap.h virq.h config.h
$(OUTD)/vopl3.o::    vopl3.cpp   dbopl.h vopl3.h config.h
$(OUTD)/vsb.o::      vsb.c       linear.h platform.h vsb.h config.h
$(OUTD)/vmpu.o::     vmpu.c      linear.h platform.h vmpu.h config.h

$(OUTD)/dpmi.o::     dpmi.asm
$(OUTD)/dpmiisr.o::  dpmiisr.asm
$(OUTD)/fileacc.o::  fileacc.asm
$(OUTD)/hapi.o::     hapi.asm
$(OUTD)/int31.o::    int31.asm
$(OUTD)/logfile.o::  logfile.asm
$(OUTD)/mixer.o::    mixer.asm
$(OUTD)/sbisr.o::    sbisr.asm
$(OUTD)/stackio.o::  stackio.asm
$(OUTD)/stackisr.o:: stackisr.asm
$(OUTD)/uninst.o::   uninst.asm

$(OUTD)/dprintf.o::  startup/dprintf.asm
$(OUTD)/vioout.o::   startup/vioout.asm

clean::
	del $(OUTD)\$(NAME)D.EXE
	del $(OUTD)\$(NAME).ar
	del $(OUTD)\*.o

