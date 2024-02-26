
# create vsbhda.exe
# to create a debug version, enter: make DEBUG=1
# note that for assembly jwasm v2.17+ is needed ( understands -djgpp option )

ifndef DEBUG
DEBUG=0
endif

NAME=vsbhda

ifeq ($(DEBUG),1)
OUTD=debug
C_DEBUG_FLAGS=-D_DEBUG
else
OUTD=build
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
	$(OUTD)/vsb.o		$(OUTD)/vdma.o		$(OUTD)/virq.o		$(OUTD)/vopl3.o\
	$(OUTD)/ac97mix.o	$(OUTD)/au_cards.o\
	$(OUTD)/dmairq.o	$(OUTD)/pcibios.o	$(OUTD)/memory.o	$(OUTD)/physmem.o	$(OUTD)/time.o\
	$(OUTD)/sc_e1371.o	$(OUTD)/sc_ich.o	$(OUTD)/sc_inthd.o	$(OUTD)/sc_via82.o	$(OUTD)/sc_sbliv.o	$(OUTD)/sc_sbl24.o\
	$(OUTD)/stackio.o	$(OUTD)/stackisr.o	$(OUTD)/int31.o		$(OUTD)/rmwrap.o	$(OUTD)/mixer.o\
	$(OUTD)/hapi.o		$(OUTD)/dprintf.o	$(OUTD)/vioout.o	$(OUTD)/djdpmi.o

INCLUDE_DIRS=src mpxplay
SRC_DIRS=src mpxplay

C_OPT_FLAGS=-Os -fno-asynchronous-unwind-tables
C_EXTRA_FLAGS=-march=i386 -DSBEMU
LD_FLAGS=$(addprefix -Xlinker ,$(LD_EXTRA_FLAGS))
LD_EXTRA_FLAGS=-Map $(OUTD)/$(NAME).map

INCLUDES=$(addprefix -I,$(INCLUDE_DIRS))
LIBS=$(addprefix -l,stdcxx m)

COMPILE.asm.o=jwasm.exe -q -djgpp -Fo$@ $<
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

all:: $(OUTD) $(OUTD)/$(NAME).exe

$(OUTD):
	@mkdir $(OUTD)

$(OUTD)/$(NAME).exe:: $(OUTD)/$(NAME).ar
	gcc -o $@ $(OUTD)/main.o $(OUTD)/$(NAME).ar $(LD_FLAGS) $(LIBS)
	strip -s $@
	exe2coff $@
	copy /b res\stub.bin + $(OUTD)\$(NAME) $(OUTD)\$(NAME).exe

$(OUTD)/$(NAME).ar:: $(OBJFILES)
	ar --target=coff-go32 r $(OUTD)/$(NAME).ar $(OBJFILES)

$(OUTD)/ac97mix.o::  ac97mix.c   mpxplay.h au_cards.h newfunc.h ac97mix.h
$(OUTD)/au_cards.o:: au_cards.c  mpxplay.h au_cards.h newfunc.h dmairq.h config.h
$(OUTD)/dbopl.o::    dbopl.cpp   dbopl.h
$(OUTD)/djdpmi.o::   djdpmi.asm
$(OUTD)/dmairq.o::   dmairq.c    mpxplay.h au_cards.h newfunc.h dmairq.h
$(OUTD)/dprintf.o::  dprintf.asm
$(OUTD)/hapi.o::     hapi.asm
$(OUTD)/int31.o::    int31.asm
$(OUTD)/linear.o::   linear.c    linear.h platform.h
$(OUTD)/main.o::     main.c      ptrap.h  linear.h vopl3.h pic.h platform.h config.h vsb.h vdma.h virq.h mpxplay.h au_cards.h newfunc.h
$(OUTD)/physmem.o::  physmem.c   newfunc.h
$(OUTD)/memory.o::   memory.c
$(OUTD)/mixer.o::    mixer.asm
$(OUTD)/pcibios.o::  pcibios.c   pcibios.h newfunc.h
$(OUTD)/pic.o::      pic.c       pic.h platform.h ptrap.h
$(OUTD)/ptrap.o::    ptrap.c     ptrap.h linear.h platform.h config.h ports.h
$(OUTD)/rmwrap.o::   rmwrap.asm
$(OUTD)/sc_e1371.o:: sc_e1371.c  mpxplay.h au_cards.h dmairq.h pcibios.h newfunc.h ac97mix.h
$(OUTD)/sc_ich.o::   sc_ich.c    mpxplay.h au_cards.h dmairq.h pcibios.h newfunc.h ac97mix.h
$(OUTD)/sc_inthd.o:: sc_inthd.c  mpxplay.h au_cards.h dmairq.h pcibios.h newfunc.h sc_inthd.h
$(OUTD)/sc_sbl24.o:: sc_sbl24.c  mpxplay.h au_cards.h dmairq.h pcibios.h newfunc.h ac97mix.h sc_sbl24.h emu10k1.h
$(OUTD)/sc_sbliv.o:: sc_sbliv.c  mpxplay.h au_cards.h dmairq.h pcibios.h newfunc.h ac97mix.h sc_sbliv.h emu10k1.h
$(OUTD)/sc_via82.o:: sc_via82.c  mpxplay.h au_cards.h dmairq.h pcibios.h newfunc.h ac97.h
$(OUTD)/sndisr.o::   sndisr.c    linear.h vopl3.h pic.h platform.h config.h vsb.h vdma.h virq.h mpxplay.h au_cards.h newfunc.h ctadpcm.h
$(OUTD)/stackio.o::  stackio.asm
$(OUTD)/stackisr.o:: stackisr.asm
$(OUTD)/time.o::     time.c      mpxplay.h au_cards.h newfunc.h
$(OUTD)/vdma.o::     vdma.c      linear.h platform.h ptrap.h vdma.h config.h
$(OUTD)/vioout.o::   vioout.asm
$(OUTD)/virq.o::     virq.c      linear.h pic.h platform.h ptrap.h virq.h config.h
$(OUTD)/vopl3.o::    vopl3.cpp   dbopl.h vopl3.h config.h
$(OUTD)/vsb.o::      vsb.c       linear.h platform.h vsb.h config.h

clean::
	del $(OUTD)\$(NAME).exe
	del $(OUTD)\$(NAME).ar
	del $(OUTD)\*.o

