
# create sbemu.exe
# to create a debug version, enter: make DEBUG=1
# note that for assembly jwasm v2.17+ is needed ( understands -djgpp option )

ifndef DEBUG
DEBUG=0
endif

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
	$(OUTD)/main.o		$(OUTD)/ptrap.o		$(OUTD)/dbopl.o		$(OUTD)/dpmihlp.o	$(OUTD)/pic.o\
	$(OUTD)/vsb.o		$(OUTD)/vdma.o		$(OUTD)/virq.o		$(OUTD)/vopl3.o\
	$(OUTD)/ac97_def.o	$(OUTD)/au_cards.o	$(OUTD)/cv_bits.o	$(OUTD)/cv_chan.o	$(OUTD)/cv_freq.o\
	$(OUTD)/dmairq.o	$(OUTD)/pcibios.o	$(OUTD)/memory.o	$(OUTD)/nf_dpmi.o	$(OUTD)/time.o\
	$(OUTD)/sc_e1371.o	$(OUTD)/sc_ich.o	$(OUTD)/sc_inthd.o	$(OUTD)/sc_via82.o	$(OUTD)/sc_sbliv.o	$(OUTD)/sc_sbl24.o\
	$(OUTD)/stackio.o	$(OUTD)/stackisr.o	$(OUTD)/int31.o		$(OUTD)/rmwrap.o\
	$(OUTD)/dprintf.o	$(OUTD)/vioout.o

INCLUDE_DIRS=src mpxplay
SRC_DIRS=src mpxplay

C_OPT_FLAGS=-Os -fno-asynchronous-unwind-tables
C_EXTRA_FLAGS=-march=i386 -DSBEMU -DSBLSUPP
LD_FLAGS=$(addprefix -Xlinker ,$(LD_EXTRA_FLAGS))
LD_EXTRA_FLAGS=-Map $(OUTD)/sbemu.map

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

all:: $(OUTD) $(OUTD)/sbemu.exe

$(OUTD):
	@mkdir $(OUTD)

$(OUTD)/sbemu.exe:: $(OUTD)/sbemu.ar
	gcc $(C_EXTRA_FLAGS) -o $@ $(OUTD)/main.o $(OUTD)/sbemu.ar $(LD_FLAGS) $(LIBS)
	strip -s $@
	exe2coff $@
	copy /b res\stub.bin + $(OUTD)\sbemu $(OUTD)\sbemu.exe

$(OUTD)/sbemu.ar:: $(OBJFILES)
	ar --target=coff-go32 r $(OUTD)/sbemu.ar $(OBJFILES)

$(OUTD)/ac97_def.o:: ac97_def.c  ac97_def.h in_file.h mpxplay.h au_cards.h au_mixer.h newfunc.h
$(OUTD)/au_cards.o:: au_cards.c  in_file.h mpxplay.h au_cards.h au_mixer.h newfunc.h dmairq.h sbemucfg.h
$(OUTD)/cv_bits.o::  cv_bits.c   in_file.h mpxplay.h au_cards.h au_mixer.h newfunc.h
$(OUTD)/cv_chan.o::  cv_chan.c   in_file.h mpxplay.h au_cards.h au_mixer.h newfunc.h
$(OUTD)/cv_freq.o::  cv_freq.c   in_file.h mpxplay.h au_cards.h au_mixer.h newfunc.h
$(OUTD)/dbopl.o::    dbopl.cpp   dbopl.h
$(OUTD)/dmairq.o::   dmairq.c    in_file.h mpxplay.h au_cards.h au_mixer.h newfunc.h dmairq.h
$(OUTD)/dpmihlp.o::  dpmihlp.c   dpmihlp.h platform.h
$(OUTD)/dprintf.o::  dprintf.asm
$(OUTD)/int31.o::    int31.asm
$(OUTD)/main.o::     main.c      ptrap.h dpmihlp.h vopl3.h pic.h platform.h sbemucfg.h vsb.h vdma.h virq.h in_file.h mpxplay.h au_cards.h au_mixer.h mix_func.h newfunc.h
$(OUTD)/memory.o::   memory.c    in_file.h mpxplay.h au_cards.h au_mixer.h newfunc.h
$(OUTD)/nf_dpmi.o::  nf_dpmi.c   newfunc.h
$(OUTD)/pcibios.o::  pcibios.c   pcibios.h newfunc.h
$(OUTD)/pic.o::      pic.c       pic.h platform.h ptrap.h
$(OUTD)/ptrap.o::    ptrap.c     ptrap.h dpmihlp.h platform.h sbemucfg.h ports.h
$(OUTD)/rmwrap.o::   rmwrap.asm
$(OUTD)/sc_e1371.o:: sc_e1371.c  ac97_def.h in_file.h mpxplay.h au_cards.h dmairq.h pcibios.h au_mixer.h newfunc.h
$(OUTD)/sc_ich.o::   sc_ich.c    ac97_def.h in_file.h mpxplay.h au_cards.h dmairq.h pcibios.h au_mixer.h newfunc.h
$(OUTD)/sc_inthd.o:: sc_inthd.c             in_file.h mpxplay.h au_cards.h dmairq.h pcibios.h au_mixer.h newfunc.h sc_inthd.h 
$(OUTD)/sc_sbl24.o:: sc_sbl24.c  ac97_def.h in_file.h mpxplay.h au_cards.h dmairq.h pcibios.h au_mixer.h newfunc.h sc_sbl24.h emu10k1.h 
$(OUTD)/sc_sbliv.o:: sc_sbliv.c  ac97_def.h in_file.h mpxplay.h au_cards.h dmairq.h pcibios.h au_mixer.h newfunc.h sc_sbliv.h emu10k1.h 
$(OUTD)/sc_via82.o:: sc_via82.c  ac97_def.h in_file.h mpxplay.h au_cards.h dmairq.h pcibios.h au_mixer.h newfunc.h
$(OUTD)/stackio.o::  stackio.asm
$(OUTD)/stackisr.o:: stackisr.asm
$(OUTD)/time.o::     time.c      in_file.h mpxplay.h au_cards.h au_mixer.h newfunc.h
$(OUTD)/vdma.o::     vdma.c      dpmihlp.h platform.h ptrap.h vdma.h sbemucfg.h
$(OUTD)/vioout.o::   vioout.asm
$(OUTD)/virq.o::     virq.c      dpmihlp.h pic.h platform.h ptrap.h virq.h sbemucfg.h
$(OUTD)/vopl3.o::    vopl3.cpp   dbopl.h vopl3.h sbemucfg.h
$(OUTD)/vsb.o::      vsb.c       dpmihlp.h platform.h vsb.h sbemucfg.h ctadpcm.h

clean::
	del $(OUTD)\sbemu.exe
	del $(OUTD)\sbemu.ar
	del $(OUTD)\*.o
