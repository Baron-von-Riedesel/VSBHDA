# This file is automatically generated by RHIDE 1.5
# created with the command:
# gpr2mak \projects\sbemu\sbemu.gpr

OUTD=build
C_DEBUG_FLAGS=
#OUTD=debug
#C_DEBUG_FLAGS=-D_DEBUG

vpath_src=mpxplay/au_cards mpxplay/newfunc mpxplay/au_mixer sbemu
vpath %.c $(vpath_src)
vpath %.cc $(vpath_src)
vpath %.cpp $(vpath_src)
vpath %.C $(vpath_src)
vpath %.cxx $(vpath_src)
vpath %.s $(vpath_src)
vpath %.S $(vpath_src)
vpath %.p $(vpath_src)
vpath %.asm $(vpath_src)
vpath %.nsm $(vpath_src)
vpath_header=./mpxplay ./sbemu
vpath %.h $(vpath_header)
vpath %.hpp $(vpath_header)
vpath %.ha $(vpath_header)
vpath %.hd $(vpath_header)
vpath_obj=./$(OUTD)/
vpath %.o $(vpath_obj)
FLAGS_FOR_SUBPROJECTS=RHIDE_OS_="$(RHIDE_OS_)" CFLAGS="$(CFLAGS)"\
	CXXFLAGS="$(CXXFLAGS)" LDFLAGS="$(LDFLAGS)" CPPFLAGS="$(CPPFLAGS)"
RHIDE_OS=$(RHIDE_OS_)
ifeq ($(strip $(RHIDE_OS)),)
ifneq ($(strip $(DJDIR)),)
RHIDE_OS_:=DJGPP
else
RHIDE_OS_:=$(patsubst CYGWIN%,CYGWIN,$(shell uname))
endif
endif

INCLUDE_DIRS=./mpxplay ./sbemu
LIB_DIRS=
C_OPT_FLAGS=-Os
C_C_LANG_FLAGS=
C_CXX_LANG_FLAGS=
C_P_LANG_FLAGS=
C_FPC_LANG_FLAGS=
C_F_LANG_FLAGS=
C_ADA_LANG_FLAGS=
LIBS=
LD_EXTRA_FLAGS=-Map $(OUTD)/sbemu.map
C_EXTRA_FLAGS=-march=i386 -D__DOS__ -DSBEMU
LOCAL_OPT=$(subst ___~~~___, ,$(subst $(notdir $<)___,,$(filter $(notdir\
	$<)___%,$(LOCAL_OPTIONS))))

OBJFILES=$(OUTD)/main.o $(OUTD)/hdpmipt.o $(OUTD)/qemm.o    $(OUTD)/sbemu.o\
	$(OUTD)/virq.o     $(OUTD)/dbopl.o    $(OUTD)/opl3emu.o $(OUTD)/pic.o\
	$(OUTD)/stackio.o  $(OUTD)/stackisr.o $(OUTD)/int31.o   $(OUTD)/dprintf.o\
	$(OUTD)/vioout.o\
	$(OUTD)/untrapio.o $(OUTD)/vdma.o\
	$(OUTD)/dmairq.o   $(OUTD)/dpmi.o     $(OUTD)/dpmi_dj2.o\
	$(OUTD)/ac97_def.o $(OUTD)/au_cards.o\
	$(OUTD)/cv_bits.o  $(OUTD)/cv_chan.o  $(OUTD)/cv_freq.o\
	$(OUTD)/sc_e1371.o $(OUTD)/sc_ich.o\
	$(OUTD)/sc_inthd.o $(OUTD)/sc_via82.o\
	$(OUTD)/string.o\
	$(OUTD)/memory.o   $(OUTD)/nf_dpmi.o $(OUTD)/pcibios.o\
	$(OUTD)/time.o     $(OUTD)/timer.o

ALL_OBJFILES=$(OUTD)/ac97_def.o $(OUTD)/au_cards.o $(OUTD)/cv_bits.o\
	$(OUTD)/cv_chan.o $(OUTD)/cv_freq.o $(OUTD)/dbopl.o\
	$(OUTD)/dmairq.o $(OUTD)/dpmi.o $(OUTD)/dpmi_dj2.o\
	$(OUTD)/hdpmipt.o $(OUTD)/main.o $(OUTD)/memory.o\
	$(OUTD)/nf_dpmi.o $(OUTD)/opl3emu.o $(OUTD)/pcibios.o $(OUTD)/pic.o\
	$(OUTD)/qemm.o $(OUTD)/sbemu.o $(OUTD)/sc_e1371.o $(OUTD)/sc_ich.o\
	$(OUTD)/sc_inthd.o $(OUTD)/sc_via82.o $(OUTD)/string.o\
	$(OUTD)/time.o $(OUTD)/timer.o $(OUTD)/untrapio.o\
	$(OUTD)/vdma.o $(OUTD)/virq.o\
	$(OUTD)/stackio.o $(OUTD)/stackisr.o $(OUTD)/int31.o $(OUTD)/dprintf.o\
	$(OUTD)/vioout.o
LIBRARIES=
SOURCE_NAME=$<
OUTFILE=$@
SPECIAL_CFLAGS=
SPECIAL_LDFLAGS=
PROG_ARGS=
SRC_DIRS=mpxplay/au_cards mpxplay/newfunc mpxplay/au_mixer sbemu
WUC=
MAIN_TARGET=$(OUTD)/sbemu.exe
PROJECT_ITEMS=ac97_def.c au_cards.c cv_bits.c cv_chan.c cv_freq.c\
	dbopl.cpp dmairq.c dpmi.c dpmi_dj2.c hdpmipt.c\
	main.c memory.c nf_dpmi.c opl3emu.cpp pcibios.c pic.c qemm.c\
	sbemu.c sc_e1371.c sc_ich.c sc_inthd.c sc_via82.c string.c\
	time.c timer.c untrapio.c vdma.c virq.c
DEFAULT_MASK=*.[acfghimnops]*
PASCAL_TYPE=GPC
GET_HOME=$(HOME)
CLEAN_FILES=$(MAIN_TARGET) $(OBJFILES)
RHIDE_ADA_BIND_FILE=$(addprefix _,$(setsuffix .c,$(OUTFILE)))
RHIDE_AR=ar
RHIDE_ARFLAGS=rcs
RHIDE_AS=$(RHIDE_GCC)
RHIDE_CO=$(shell co -q $(co_arg))
RHIDE_COMPILE.C.ii=$(RHIDE_COMPILE.cc.ii)
RHIDE_COMPILE.C.o=$(RHIDE_COMPILE.cc.o)
RHIDE_COMPILE.C.s=$(RHIDE_COMPILE.cc.s)
RHIDE_COMPILE.asm.o=$(RHIDE_COMPILE_JWASM)
RHIDE_COMPILE.c.i=$(subst -c $(SOURCE_NAME),-E\
	$(SOURCE_NAME),$(RHIDE_COMPILE_C))
RHIDE_COMPILE.c.o=$(RHIDE_COMPILE_C)
RHIDE_COMPILE.c.s=$(subst -c $(SOURCE_NAME),-S\
	$(SOURCE_NAME),$(RHIDE_COMPILE_C))
RHIDE_COMPILE.cc.ii=$(subst -c $(SOURCE_NAME),-E\
	$(SOURCE_NAME),$(RHIDE_COMPILE_CC))
RHIDE_COMPILE.cc.o=$(RHIDE_COMPILE_CC)
RHIDE_COMPILE.cc.s=$(subst -c $(SOURCE_NAME),-S\
	$(SOURCE_NAME),$(RHIDE_COMPILE_CC))
RHIDE_COMPILE.cpp.ii=$(RHIDE_COMPILE.cc.ii)
RHIDE_COMPILE.cpp.o=$(RHIDE_COMPILE.cc.o)
RHIDE_COMPILE.cpp.s=$(RHIDE_COMPILE.cc.s)
RHIDE_COMPILE.cxx.ii=$(RHIDE_COMPILE.cc.ii)
RHIDE_COMPILE.cxx.o=$(RHIDE_COMPILE.cc.o)
RHIDE_COMPILE.cxx.s=$(RHIDE_COMPILE.cc.s)
RHIDE_COMPILE.i.s=$(RHIDE_COMPILE.c.s)
RHIDE_COMPILE.ii.s=$(RHIDE_COMPILE.cc.s)
RHIDE_COMPILE.s.o=$(RHIDE_COMPILE_ASM)
RHIDE_COMPILE_ARCHIVE=$(RHIDE_AR) $(RHIDE_ARFLAGS) $(OUTFILE)\
	$(ALL_OBJFILES)
RHIDE_COMPILE_ASM=$(RHIDE_AS) $(RHIDE_INCLUDES)\
	$(C_OPT_FLAGS) $(LOCAL_OPT) -c $(SOURCE_NAME) -o $(OUTFILE)
RHIDE_COMPILE_ASM_FORCE=$(RHIDE_AS) $(RHIDE_INCLUDES) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) -x assembler\
	$(LOCAL_OPT) -c $(SOURCE_NAME) -o $(OUTFILE)
RHIDE_COMPILE_C=$(RHIDE_GCC) $(RHIDE_INCLUDES) $(C_DEBUG_FLAGS)\
	$(C_OPT_FLAGS) $(C_C_LANG_FLAGS) $(C_EXTRA_FLAGS)\
	$(RHIDE_OS_CFLAGS) $(CPPFLAGS) $(CFLAGS) $(LOCAL_OPT) -c\
	$(SOURCE_NAME) -o $(OUTFILE)
RHIDE_COMPILE_CC=$(RHIDE_GXX) $(RHIDE_INCLUDES) $(C_DEBUG_FLAGS)\
	$(C_OPT_FLAGS) $(C_C_LANG_FLAGS)\
	$(C_CXX_LANG_FLAGS) $(C_EXTRA_FLAGS) $(RHIDE_OS_CXXFLAGS)\
	$(CPPFLAGS) $(CXXFLAGS) $(LOCAL_OPT) -c $(SOURCE_NAME) -o\
	$(OUTFILE)
RHIDE_COMPILE_CC_FORCE=$(RHIDE_GXX) $(RHIDE_INCLUDES) $(C_DEBUG_FLAGS)\
	$(C_OPT_FLAGS) $(C_C_LANG_FLAGS)\
	$(C_CXX_LANG_FLAGS) $(C_EXTRA_FLAGS) $(RHIDE_OS_CXXFLAGS)\
	$(CPPFLAGS) $(CXXFLAGS) -x c++ $(LOCAL_OPT) -c $(SOURCE_NAME) -o\
	$(OUTFILE)
RHIDE_COMPILE_C_FORCE=$(RHIDE_GCC) $(RHIDE_INCLUDES) $(C_DEBUG_FLAGS)\
	$(C_OPT_FLAGS) $(C_C_LANG_FLAGS) $(C_EXTRA_FLAGS)\
	-x c $(RHIDE_OS_CFLAGS) $(CPPFLAGS) $(CFLAGS) $(LOCAL_OPT) -c\
	$(SOURCE_NAME) -o $(OUTFILE)

RHIDE_COMPILE_LINK=$(RHIDE_LD) $(RHIDE_LIBDIRS) $(C_EXTRA_FLAGS) -o\
	$(OUTFILE) $(OBJFILES) $(LIBRARIES) $(LDFLAGS) $(RHIDE_LDFLAGS)\
	$(RHIDE_LIBS)

RHIDE_COMPILE_JWASM=jwasm.exe -nologo -djgpp -Fo$(OUTFILE) $(SOURCE_NAME)
RHIDE_CONFIG_DIRS=. $(RHIDE_SHARE) $(GET_HOME)   $(RHIDE_CONFIG_DIRS_COMMON)\
	 $(addsuffix /SET,$(RHIDE_CONFIG_DIRS_COMMON))  $(SET_FILES)
RHIDE_CONFIG_DIRS_DJGPP=$(DJDIR)/share/rhide
RHIDE_EMPTY=
RHIDE_FSDB=fsdb $(OUTFILE) $(addprefix -p ,$(SRC_DIRS)) $(PROG_ARGS)
RHIDE_GCC=gcc
RHIDE_GPROF=gprof $(OUTFILE)
RHIDE_GXX=$(RHIDE_GCC)
RHIDE_INCLUDES=$(SPECIAL_CFLAGS) $(addprefix -I,$(INCLUDE_DIRS))
RHIDE_LD=$(RHIDE_GCC)
RHIDE_LDFLAGS=$(SPECIAL_LDFLAGS) $(addprefix -Xlinker ,$(LD_EXTRA_FLAGS))
RHIDE_LD_FPC=$(RHIDE_FPC) -E+
RHIDE_LD_PASCAL=gpc
RHIDE_LIBDIRS=$(addprefix -L,$(LIB_DIRS))
RHIDE_LIBS=$(addprefix -l,$(LIBS) $(RHIDE_TYPED_LIBS) $(RHIDE_OS_LIBS))
RHIDE_PATH_SEPARATOR=$(RHIDE_PATH_SEPARATOR_$(RHIDE_OS))
RHIDE_PATH_SEPARATOR_$(RHIDE_OS)=:
RHIDE_PATH_SEPARATOR_DJGPP=;
RHIDE_PATH_SEPARATOR_DJGPP=:
RHIDE_RM=rm
RHIDE_SHARED_LDFLAGS=$(RHIDE_SHARED_LDFLAGS_$(RHIDE_OS))
RHIDE_SHARED_LDFLAGS_$(RHIDE_OS)=
RHIDE_SHARED_LDFLAGS_DJGPP=
RHIDE_SPACE=$(RHIDE_EMPTY) $(RHIDE_EMPTY)
RHIDE_STANDARD_INCLUDES=$(RHIDE_STANDARD_INCLUDES_$(RHIDE_OS))
RHIDE_STANDARD_INCLUDES_$(RHIDE_OS)=$(addprefix /usr/,include include/sys\
	include/g++ include/g++/std)
RHIDE_STANDARD_INCLUDES_DJGPP=$(addprefix $(DJDIR)/,include include/sys\
	lang/cxx lang/cxx/std)
RHIDE_STANDARD_INCLUDES_DJGPP=$(addprefix /usr/,include include/sys\
	include/g++ include/g++/std)
RHIDE_TYPED_LIBS=$(foreach\
	suff,$(RHIDE_TYPED_LIBS_SUFFIXES),$(RHIDE_TYPED_LIBS$(suff)))
RHIDE_TYPED_LIBS.C=$(RHIDE_TYPED_LIBS.cc)
RHIDE_TYPED_LIBS.cc=$(RHIDE_TYPED_LIBS_$(RHIDE_OS).cc)
RHIDE_TYPED_LIBS.cpp=$(RHIDE_TYPED_LIBS.cc)
RHIDE_TYPED_LIBS.cxx=$(RHIDE_TYPED_LIBS.cc)
RHIDE_TYPED_LIBS.ii=$(RHIDE_TYPED_LIBS.cc)
RHIDE_TYPED_LIBS.l=fl
RHIDE_TYPED_LIBS_$(RHIDE_OS).cc=stdc++ m
RHIDE_TYPED_LIBS_DJGPP.cc=stdcxx m
RHIDE_TYPED_LIBS_DJGPP.cc=stdcxx m
RHIDE_TYPED_LIBS_DJGPP.cc=stdcxx m
RHIDE_TYPED_LIBS_DJGPP.cpp=stdcxx m
RHIDE_TYPED_LIBS_DJGPP.cxx=stdcxx m
RHIDE_TYPED_LIBS_SUFFIXES=$(sort $(foreach item,$(PROJECT_ITEMS),$(suffix\
	$(item))))
%.o: %.c
	$(RHIDE_COMPILE.c.o)
%.o: %.i
	$(RHIDE_COMPILE_C)
%.o: %.cc
	$(RHIDE_COMPILE.cc.o)
%.o: %.cpp
	$(RHIDE_COMPILE.cpp.o)
%.o: %.cxx
	$(RHIDE_COMPILE.cxx.o)
%.o: %.C
	$(RHIDE_COMPILE.C.o)
%.o: %.ii
	$(RHIDE_COMPILE_CC)
%.o: %.s
	$(RHIDE_COMPILE.s.o)
%.o: %.S
	$(RHIDE_COMPILE_ASM)
%.s: %.c
	$(RHIDE_COMPILE.c.s)
%.s: %.i
	$(RHIDE_COMPILE.i.s)
%.s: %.cc
	$(RHIDE_COMPILE.cc.s)
%.s: %.cpp
	$(RHIDE_COMPILE.cpp.s)
%.s: %.cxx
	$(RHIDE_COMPILE.cxx.s)
%.s: %.C
	$(RHIDE_COMPILE.C.s)
%.o: %.asm
	$(RHIDE_COMPILE.asm.o)
%.o: %.nsm
	$(RHIDE_COMPILE.nsm.o)
%.i: %.c
	$(RHIDE_COMPILE.c.i)
%.s: %.c
	$(RHIDE_COMPILE.c.s)
%.ii: %.cc
	$(RHIDE_COMPILE.cc.ii)
%.s: %.cc
	$(RHIDE_COMPILE.cc.s)
%.ii: %.cpp
	$(RHIDE_COMPILE.cpp.ii)
%.s: %.cpp
	$(RHIDE_COMPILE.cpp.s)
%.ii: %.cxx
	$(RHIDE_COMPILE.cxx.ii)
%.s: %.cxx
	$(RHIDE_COMPILE.cxx.s)
%.ii: %.C
	$(RHIDE_COMPILE.C.ii)
%.s: %.C
	$(RHIDE_COMPILE.C.s)

all::

clean::
	del $(OUTD)\*.o

DEPS_0= $(OUTD)/main.o   $(OUTD)/hdpmipt.o	$(OUTD)/qemm.o	$(OUTD)/opl3emu.o	$(OUTD)/dbopl.o\
	$(OUTD)/dpmi.o		$(OUTD)/dpmi_dj2.o	$(OUTD)/pic.o	$(OUTD)/sbemu.o\
	$(OUTD)/untrapio.o	$(OUTD)/vdma.o		$(OUTD)/virq.o\
	$(OUTD)/time.o		$(OUTD)/timer.o\
	$(OUTD)/stackio.o	$(OUTD)/stackisr.o	$(OUTD)/int31.o\
	$(OUTD)/ac97_def.o	$(OUTD)/au_cards.o	$(OUTD)/cv_bits.o	$(OUTD)/cv_chan.o	$(OUTD)/cv_freq.o\
	$(OUTD)/sc_e1371.o	$(OUTD)/sc_ich.o		$(OUTD)/sc_inthd.o	$(OUTD)/sc_via82.o	$(OUTD)/string.o\
	$(OUTD)/dmairq.o	$(OUTD)/pcibios.o	$(OUTD)/memory.o		$(OUTD)/nf_dpmi.o\
	$(OUTD)/dprintf.o	$(OUTD)/vioout.o

NO_LINK=
LINK_FILES=$(filter-out $(NO_LINK),$(DEPS_0))

$(OUTD)/sbemu.exe:: $(DEPS_0)
	$(RHIDE_COMPILE_LINK)
	strip -s $(OUTFILE)
#	exe2coff $(OUTFILE)
#	copy /b $(OUTD)\stub.bin + $(OUTD)\sbemu $(OUTD)\sbemu.exe
	stubedit $(OUTFILE) minstack=64k

DEPS_7=dbopl.cpp dbopl.h
DEPS_9=dpmi.c dpmi_.h platform.h
DEPS_10=dpmi_dj2.c dpmi_.h platform.h
DEPS_13=hdpmipt.c hdpmipt.h qemm.h dpmi_.h platform.h untrapio.h sbemucfg.h
DEPS_14=main.c hdpmipt.h qemm.h\
	dpmi_.h opl3emu.h pic.h platform.h sbemu.h sbemucfg.h untrapio.h vdma.h virq.h\
	./mpxplay/au_cards/au_cards.h ./mpxplay/au_mixer/au_mixer.h ./mpxplay/au_mixer/mix_func.h\
	./mpxplay/in_file.h ./mpxplay/mpxplay.h ./mpxplay/newfunc/newfunc.h
DEPS_16=nf_dpmi.c ./mpxplay/newfunc/newfunc.h
DEPS_17=opl3emu.cpp dbopl.h opl3emu.h
DEPS_19=pic.c pic.h platform.h untrapio.h
DEPS_20=qemm.c qemm.h dpmi_.h platform.h untrapio.h sbemucfg.h
DEPS_21=sbemu.c dpmi_.h platform.h sbemu.h sbemucfg.h
DEPS_31=untrapio.c untrapio.h
DEPS_32=vdma.c dpmi_.h platform.h untrapio.h vdma.h sbemucfg.h
DEPS_33=virq.c dpmi_.h pic.h      platform.h untrapio.h virq.h

DEPS_1=ac97_def.c\
	./mpxplay/au_cards/ac97_def.h ./mpxplay/au_cards/au_cards.h ./mpxplay/au_mixer/au_mixer.h\
	./mpxplay/in_file.h ./mpxplay/mpxplay.h ./mpxplay/newfunc/newfunc.h
DEPS_2=au_cards.c\
	./mpxplay/au_cards/au_cards.h ./mpxplay/au_cards/dmairq.h ./mpxplay/au_mixer/au_mixer.h\
	./mpxplay/in_file.h ./mpxplay/mpxplay.h ./mpxplay/newfunc/newfunc.h
DEPS_3=cv_bits.c\
	./mpxplay/au_cards/au_cards.h ./mpxplay/au_mixer/au_mixer.h ./mpxplay/in_file.h ./mpxplay/mpxplay.h\
	./mpxplay/newfunc/newfunc.h
DEPS_4=cv_chan.c\
	./mpxplay/au_cards/au_cards.h ./mpxplay/au_mixer/au_mixer.h ./mpxplay/in_file.h ./mpxplay/mpxplay.h\
	./mpxplay/newfunc/newfunc.h
DEPS_5=cv_freq.c\
	./mpxplay/au_cards/au_cards.h ./mpxplay/au_mixer/au_mixer.h ./mpxplay/in_file.h ./mpxplay/mpxplay.h\
	./mpxplay/newfunc/newfunc.h
DEPS_8=dmairq.c\
	./mpxplay/au_cards/au_cards.h ./mpxplay/au_cards/dmairq.h ./mpxplay/au_mixer/au_mixer.h\
	./mpxplay/in_file.h ./mpxplay/mpxplay.h ./mpxplay/newfunc/newfunc.h
DEPS_15=memory.c\
	./mpxplay/au_cards/au_cards.h\
	./mpxplay/au_mixer/au_mixer.h\
	./mpxplay/in_file.h ./mpxplay/mpxplay.h\
	./mpxplay/newfunc/newfunc.h
DEPS_18=pcibios.c\
	./mpxplay/au_cards/pcibios.h\
	./mpxplay/newfunc/newfunc.h
DEPS_22=sc_e1371.c\
	./mpxplay/au_cards/ac97_def.h\
	./mpxplay/au_cards/au_cards.h\
	./mpxplay/au_cards/dmairq.h\
	./mpxplay/au_cards/pcibios.h\
	./mpxplay/au_mixer/au_mixer.h\
	./mpxplay/in_file.h ./mpxplay/mpxplay.h\
	./mpxplay/newfunc/newfunc.h
DEPS_23=sc_ich.c\
	./mpxplay/au_cards/ac97_def.h\
	./mpxplay/au_cards/au_cards.h\
	./mpxplay/au_cards/dmairq.h\
	./mpxplay/au_cards/pcibios.h\
	./mpxplay/au_mixer/au_mixer.h\
	./mpxplay/in_file.h ./mpxplay/mpxplay.h\
	./mpxplay/newfunc/newfunc.h
DEPS_24=sc_inthd.c\
	./mpxplay/au_cards/au_cards.h\
	./mpxplay/au_cards/dmairq.h\
	./mpxplay/au_cards/pcibios.h\
	./mpxplay/au_cards/sc_inthd.h\
	./mpxplay/au_mixer/au_mixer.h\
	./mpxplay/in_file.h ./mpxplay/mpxplay.h\
	./mpxplay/newfunc/newfunc.h
DEPS_25=sc_via82.c\
	./mpxplay/au_cards/ac97_def.h\
	./mpxplay/au_cards/au_cards.h\
	./mpxplay/au_cards/dmairq.h\
	./mpxplay/au_cards/pcibios.h\
	./mpxplay/au_mixer/au_mixer.h\
	./mpxplay/in_file.h ./mpxplay/mpxplay.h\
	./mpxplay/newfunc/newfunc.h
DEPS_26=string.c\
	./mpxplay/au_cards/au_cards.h\
	./mpxplay/au_mixer/au_mixer.h\
	./mpxplay/in_file.h ./mpxplay/mpxplay.h\
	./mpxplay/newfunc/newfunc.h
DEPS_29=time.c\
	./mpxplay/au_cards/au_cards.h ./mpxplay/au_mixer/au_mixer.h ./mpxplay/in_file.h ./mpxplay/mpxplay.h\
	./mpxplay/newfunc/newfunc.h
DEPS_30=timer.c\
	./mpxplay/au_cards/au_cards.h ./mpxplay/au_mixer/au_mixer.h ./mpxplay/in_file.h\
	./mpxplay/mpxplay.h ./mpxplay/newfunc/newfunc.h

$(OUTD)/ac97_def.o:: $(DEPS_1)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/au_cards.o:: $(DEPS_2)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/cv_bits.o:: $(DEPS_3)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/cv_chan.o:: $(DEPS_4)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/cv_freq.o:: $(DEPS_5)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/dbopl.o:: $(DEPS_7)
	$(RHIDE_COMPILE.cpp.o)
$(OUTD)/dmairq.o:: $(DEPS_8)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/dpmi.o:: $(DEPS_9)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/dpmi_dj2.o:: $(DEPS_10)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/hdpmipt.o:: $(DEPS_13)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/main.o:: $(DEPS_14)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/memory.o:: $(DEPS_15)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/nf_dpmi.o:: $(DEPS_16)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/opl3emu.o:: $(DEPS_17)
	$(RHIDE_COMPILE.cpp.o)
$(OUTD)/pcibios.o:: $(DEPS_18)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/pic.o:: $(DEPS_19)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/qemm.o:: $(DEPS_20)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/sbemu.o:: $(DEPS_21)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/sc_e1371.o:: $(DEPS_22)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/sc_ich.o:: $(DEPS_23)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/sc_inthd.o:: $(DEPS_24)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/sc_via82.o:: $(DEPS_25)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/string.o:: $(DEPS_26)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/time.o:: $(DEPS_29)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/timer.o:: $(DEPS_30)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/untrapio.o:: $(DEPS_31)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/vdma.o:: $(DEPS_32)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/virq.o:: $(DEPS_33)
	$(RHIDE_COMPILE.c.o)
$(OUTD)/stackio.o:: stackio.asm
	$(RHIDE_COMPILE.asm.o)
$(OUTD)/stackisr.o:: stackisr.asm
	$(RHIDE_COMPILE.asm.o)
$(OUTD)/int31.o:: int31.asm
	$(RHIDE_COMPILE.asm.o)
$(OUTD)/dprintf.o:: dprintf.asm
	$(RHIDE_COMPILE.asm.o)
$(OUTD)/vioout.o:: vioout.asm
	$(RHIDE_COMPILE.asm.o)

all:: $(OUTD)/sbemu.exe