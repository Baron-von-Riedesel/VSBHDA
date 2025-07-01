
# create vsbhdad.exe with DJGPP and JWasm.
# to create a debug version, enter: make -f djgpp.mak DEBUG=1
# note that JWasm v2.17+ is needed ( understands -djgpp option )

ifndef DEBUG
DEBUG=0
endif

NAME=vsbhda

ifeq ($(DEBUG),1)
OUTD=djgppd
C_DEBUG_FLAGS=-D_DEBUG
else
OUTD=djgpp
C_DEBUG_FLAGS=
endif

vpath_src=src mpxplay
vpath %.c $(vpath_src)
vpath %.cpp $(vpath_src)
vpath %.asm $(vpath_src)
vpath_header=src mpxplay
vpath %.h $(vpath_header)
vpath_obj=./$(OUTD)/
vpath %.o $(vpath_obj)

OBJFILES=\
	$(OUTD)/main.o		$(OUTD)/sndisr.o	$(OUTD)/ptrap.o		$(OUTD)/dbopl.o		$(OUTD)/linear.o	$(OUTD)/pic.o\
	$(OUTD)/vsb.o		$(OUTD)/vdma.o		$(OUTD)/virq.o		$(OUTD)/vopl3.o		$(OUTD)/vmpu.o		$(OUTD)/tsf.o\
	$(OUTD)/ac97mix.o	$(OUTD)/au_cards.o\
	$(OUTD)/dmairq.o	$(OUTD)/pcibios.o	$(OUTD)/physmem.o	$(OUTD)/timer.o\
	$(OUTD)/sc_e1371.o	$(OUTD)/sc_ich.o	$(OUTD)/sc_inthd.o	$(OUTD)/sc_via82.o	$(OUTD)/sc_sbliv.o	$(OUTD)/sc_sbl24.o\
	$(OUTD)/stackio.o	$(OUTD)/stackisr.o	$(OUTD)/sbisr.o		$(OUTD)/int31.o		$(OUTD)/rmwrap.o	$(OUTD)/mixer.o\
	$(OUTD)/hapi.o		$(OUTD)/dprintf.o	$(OUTD)/vioout.o	$(OUTD)/djdpmi.o	$(OUTD)/uninst.o	$(OUTD)/fileacc.o

INCLUDE_DIRS=src mpxplay
SRC_DIRS=src mpxplay

C_OPT_FLAGS=-Os -fno-asynchronous-unwind-tables
C_EXTRA_FLAGS=-march=i586
LD_FLAGS=$(addprefix -Xlinker ,$(LD_EXTRA_FLAGS))
LD_EXTRA_FLAGS=-Map $(OUTD)/$(NAME).map

INCLUDES=$(addprefix -I,$(INCLUDE_DIRS))
LIBS=$(addprefix -l,stdcxx m)

COMPILE.asm.o=jwasm.exe -q -djgpp -Istartup -D?MODEL=small -DDJGPP -Fo$@ $<
COMPILE.c.o=gcc $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
COMPILE.cpp.o=gcc $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CPPFLAGS) $(INCLUDES) -c $< -o $@

$(OUTD)/%.o: src/%.c
	$(COMPILE.c.o)

$(OUTD)/%.o: src/%.cpp
	$(COMPILE.cpp.o)

$(OUTD)/%.o: src/%.asm
	$(COMPILE.asm.o)

$(OUTD)/%.o: mpxplay/%.c
	$(COMPILE.c.o)

all:: $(OUTD) $(OUTD)/$(NAME)d.exe

$(OUTD):
	@mkdir $(OUTD)

$(OUTD)/$(NAME)d.exe:: $(OUTD)/$(NAME).ar
	gcc -o $@ $(OUTD)/main.o $(OUTD)/$(NAME).ar $(LD_FLAGS) $(LIBS)
	strip -s $@
	exe2coff $@
	copy /b res\stub.bin + $(OUTD)\$(NAME)d $(OUTD)\$(NAME)d.exe

$(OUTD)/$(NAME).ar:: $(OBJFILES)
	ar --target=coff-go32 r $(OUTD)/$(NAME).ar $(OBJFILES)

# to avoid problems with 16-bit relocations, the 16-bit code
# is included in binary format into rmwrap.asm.

$(OUTD)/rmwrap.o:: rmwrap.asm rmcode1.asm rmcode2.asm
	jwasm.exe -q -bin -Fl$(OUTD)/ -Fo$(OUTD)/rmcode1.bin src/rmcode1.asm
	jwasm.exe -q -bin -Fl$(OUTD)/ -Fo$(OUTD)/rmcode2.bin src/rmcode2.asm
	jwasm.exe -q -djgpp -D?MODEL=small -DOUTD=$(OUTD) -Fo$@ src/rmwrap.asm

$(OUTD)/ac97mix.o::  ac97mix.c   mpxplay.h au_cards.h ac97mix.h
$(OUTD)/au_cards.o:: au_cards.c  mpxplay.h au_cards.h dmairq.h config.h
$(OUTD)/dmairq.o::   dmairq.c    mpxplay.h au_cards.h dmairq.h
$(OUTD)/pcibios.o::  pcibios.c   pcibios.h
$(OUTD)/physmem.o::  physmem.c
$(OUTD)/sc_e1371.o:: sc_e1371.c  mpxplay.h au_cards.h dmairq.h pcibios.h ac97mix.h
$(OUTD)/sc_ich.o::   sc_ich.c    mpxplay.h au_cards.h dmairq.h pcibios.h ac97mix.h
$(OUTD)/sc_inthd.o:: sc_inthd.c  mpxplay.h au_cards.h dmairq.h pcibios.h sc_inthd.h
$(OUTD)/sc_sbl24.o:: sc_sbl24.c  mpxplay.h au_cards.h dmairq.h pcibios.h ac97mix.h sc_sbl24.h emu10k1.h
$(OUTD)/sc_sbliv.o:: sc_sbliv.c  mpxplay.h au_cards.h dmairq.h pcibios.h ac97mix.h sc_sbliv.h emu10k1.h
$(OUTD)/sc_via82.o:: sc_via82.c  mpxplay.h au_cards.h dmairq.h pcibios.h ac97.h
$(OUTD)/timer.o::    timer.c     mpxplay.h au_cards.h timer.h

$(OUTD)/dbopl.o::    dbopl.cpp   dbopl.h
$(OUTD)/linear.o::   linear.c    linear.h platform.h
$(OUTD)/main.o::     main.c      linear.h platform.h ptrap.h vopl3.h pic.h config.h vsb.h vdma.h virq.h au.h version.h
$(OUTD)/pic.o::      pic.c       pic.h platform.h ptrap.h
$(OUTD)/ptrap.o::    ptrap.c     linear.h platform.h ptrap.h config.h
$(OUTD)/sndisr.o::   sndisr.c    linear.h platform.h vopl3.h pic.h config.h vsb.h vdma.h virq.h ctadpcm.h au.h
$(OUTD)/tsf.o::      tsf.c       tsf/tsf.h
$(OUTD)/vdma.o::     vdma.c      linear.h platform.h ptrap.h vdma.h config.h
$(OUTD)/virq.o::     virq.c      linear.h platform.h pic.h ptrap.h virq.h config.h
$(OUTD)/vopl3.o::    vopl3.cpp   dbopl.h vopl3.h config.h
$(OUTD)/vsb.o::      vsb.c       linear.h platform.h vsb.h config.h
$(OUTD)/vmpu.o::     vmpu.c      linear.h platform.h vmpu.h config.h

$(OUTD)/djdpmi.o::   djdpmi.asm
$(OUTD)/dprintf.o::  dprintf.asm
$(OUTD)/fileacc.o::  fileacc.asm
$(OUTD)/hapi.o::     hapi.asm
$(OUTD)/int31.o::    int31.asm
$(OUTD)/mixer.o::    mixer.asm
$(OUTD)/sbisr.o::    sbisr.asm
$(OUTD)/stackio.o::  stackio.asm
$(OUTD)/stackisr.o:: stackisr.asm
$(OUTD)/uninst.o::   uninst.asm
$(OUTD)/vioout.o::   vioout.asm

clean::
	del $(OUTD)\$(NAME)d.exe
	del $(OUTD)\$(NAME).ar
	del $(OUTD)\*.o

