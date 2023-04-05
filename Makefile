
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

vpath_src=mpxplay sbemu
vpath %.c $(vpath_src)
vpath %.cpp $(vpath_src)
vpath %.asm $(vpath_src)
vpath_header=./sbemu ./mpxplay
vpath %.h $(vpath_header)
vpath_obj=./$(OUTD)/
vpath %.o $(vpath_obj)

OBJFILES=\
	$(OUTD)/main.o		$(OUTD)/hdpmipt.o	$(OUTD)/qemm.o		$(OUTD)/opl3emu.o\
	$(OUTD)/dbopl.o		$(OUTD)/dpmi.o		$(OUTD)/pic.o		$(OUTD)/sbemu.o\
	$(OUTD)/untrapio.o	$(OUTD)/vdma.o		$(OUTD)/virq.o\
	$(OUTD)/ac97_def.o	$(OUTD)/au_cards.o	$(OUTD)/cv_bits.o	$(OUTD)/cv_chan.o	$(OUTD)/cv_freq.o\
	$(OUTD)/dmairq.o	$(OUTD)/pcibios.o	$(OUTD)/memory.o	$(OUTD)/nf_dpmi.o	$(OUTD)/string.o	$(OUTD)/time.o\
	$(OUTD)/sc_e1371.o	$(OUTD)/sc_ich.o	$(OUTD)/sc_inthd.o	$(OUTD)/sc_via82.o	$(OUTD)/sc_sbliv.o	$(OUTD)/sc_sbl24.o\
	$(OUTD)/stackio.o	$(OUTD)/stackisr.o	$(OUTD)/int31.o\
	$(OUTD)/dprintf.o	$(OUTD)/vioout.o

INCLUDE_DIRS=./mpxplay ./sbemu
SRC_DIRS=mpxplay sbemu

C_OPT_FLAGS=-Os -fno-asynchronous-unwind-tables
C_EXTRA_FLAGS=-march=i386 -D__DOS__ -DSBEMU -DSBLSUPP
LD_FLAGS=$(addprefix -Xlinker ,$(LD_EXTRA_FLAGS))
LD_EXTRA_FLAGS=-Map $(OUTD)/sbemu.map

INCLUDES=$(addprefix -I,$(INCLUDE_DIRS))
LIBS=$(addprefix -l,stdcxx m)

COMPILE.asm.o=jwasm.exe -q -djgpp -Fo$@ $<
COMPILE.c.o=gcc $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CFLAGS) $(INCLUDES) -c $< -o $@
COMPILE.cpp.o=gcc $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CPPFLAGS) $(INCLUDES) -c $< -o $@

$(OUTD)/%.o: SBEMU/%.c
	$(COMPILE.c.o)

$(OUTD)/%.o: SBEMU/%.cpp
	$(COMPILE.cpp.o)

$(OUTD)/%.o: SBEMU/%.asm
	$(COMPILE.asm.o)

$(OUTD)/%.o: MPXPLAY/%.c
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
$(OUTD)/dpmi.o::     dpmi.c      dpmi_.h platform.h
$(OUTD)/dprintf.o::  dprintf.asm
$(OUTD)/hdpmipt.o::  hdpmipt.c   hdpmipt.h qemm.h dpmi_.h platform.h untrapio.h sbemucfg.h
$(OUTD)/int31.o::    int31.asm
$(OUTD)/main.o::     main.c      hdpmipt.h qemm.h dpmi_.h opl3emu.h pic.h platform.h sbemu.h sbemucfg.h untrapio.h vdma.h virq.h in_file.h mpxplay.h au_cards.h au_mixer.h mix_func.h newfunc.h ports.h
$(OUTD)/memory.o::   memory.c    in_file.h mpxplay.h au_cards.h au_mixer.h newfunc.h
$(OUTD)/nf_dpmi.o::  nf_dpmi.c   newfunc.h
$(OUTD)/opl3emu.o::  opl3emu.cpp dbopl.h opl3emu.h
$(OUTD)/pcibios.o::  pcibios.c   pcibios.h newfunc.h
$(OUTD)/pic.o::      pic.c       pic.h platform.h untrapio.h
$(OUTD)/qemm.o::     qemm.c      qemm.h dpmi_.h platform.h untrapio.h sbemucfg.h
$(OUTD)/sbemu.o::    sbemu.c     dpmi_.h platform.h sbemu.h sbemucfg.h
$(OUTD)/sc_e1371.o:: sc_e1371.c  ac97_def.h in_file.h mpxplay.h au_cards.h dmairq.h pcibios.h au_mixer.h newfunc.h
$(OUTD)/sc_ich.o::   sc_ich.c    ac97_def.h in_file.h mpxplay.h au_cards.h dmairq.h pcibios.h au_mixer.h newfunc.h
$(OUTD)/sc_inthd.o:: sc_inthd.c             in_file.h mpxplay.h au_cards.h dmairq.h pcibios.h au_mixer.h newfunc.h sc_inthd.h 
$(OUTD)/sc_sbl24.o:: sc_sbl24.c  ac97_def.h in_file.h mpxplay.h au_cards.h dmairq.h pcibios.h au_mixer.h newfunc.h sc_sbl24.h emu10k1.h 
$(OUTD)/sc_sbliv.o:: sc_sbliv.c  ac97_def.h in_file.h mpxplay.h au_cards.h dmairq.h pcibios.h au_mixer.h newfunc.h sc_sbliv.h emu10k1.h 
$(OUTD)/sc_via82.o:: sc_via82.c  ac97_def.h in_file.h mpxplay.h au_cards.h dmairq.h pcibios.h au_mixer.h newfunc.h
$(OUTD)/stackio.o::  stackio.asm
$(OUTD)/stackisr.o:: stackisr.asm
$(OUTD)/string.o::   string.c    in_file.h mpxplay.h au_cards.h au_mixer.h newfunc.h
$(OUTD)/time.o::     time.c      in_file.h mpxplay.h au_cards.h au_mixer.h newfunc.h
$(OUTD)/untrapio.o:: untrapio.c  untrapio.h
$(OUTD)/vdma.o::     vdma.c      dpmi_.h platform.h untrapio.h vdma.h sbemucfg.h
$(OUTD)/vioout.o::   vioout.asm
$(OUTD)/virq.o::     virq.c      dpmi_.h pic.h platform.h untrapio.h virq.h sbemucfg.h

clean::
	del $(OUTD)\sbemu.exe
	del $(OUTD)\sbemu.ar
	del $(OUTD)\*.o
