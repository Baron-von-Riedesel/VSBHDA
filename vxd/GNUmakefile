# VSBVXD.386 - Windows 3.1 enhanced mode virtual device for the VSBHDA
# Sound Blaster emulation (GNU make, Linux).
#
#   make -C vxd [WATCOM=...] [ASM=jwasm] [LINK=jwlink]
#
# Tools: Open Watcom (wcc386, wpp386, C library), JWasm and a JWlink with
# working "format windows vxd" output for Windows 3.x (self-relative fixups
# within an object must not get fixup records, no 16-bit fixups in 32-bit
# objects).

WATCOM ?= /opt/watcom
ASM    ?= jwasm
LINK   ?= jwlink
# the linker runs in $(OUT): ASM and LINK need absolute paths or the PATH
export WATCOM

CC  := $(WATCOM)/binl/wcc386
CPP := $(WATCOM)/binl/wpp386
SRC := ../src
OUT ?= ../build/vxd

# flat, cdecl, stack calling, inline FPU, no stack checks, no default libs
COPT := -q -mf -oxa -ecc -5s -fp5 -fpi87 -s -zl -zls -wcd=111 -wcd=303 -DVSBVXD
CPPOPT := -q -mf -oxa -bc -5s -fp5 -fpi87 -s -zl -zls -DVSBVXD
INC := -I. -I$(SRC) -I$(SRC)/hw -I$(WATCOM)/h

VXDOBJS := VSBVXD.obj VSBRMI.obj VXDMAIN.obj VXDLOG.obj PCIVXD.obj PLATVXD.obj \
	PTRAPVXD.obj VDMAVXD.obj VIRQVXD.obj CPPRT.obj RTVXD.obj VXDWAVE.obj

# VSBHDA emulation (src)
EMUOBJS := SNDISR.obj VSB.obj ADPCM.obj VMPU.obj VOPL3.obj DBOPL.obj

# VSBHDA sound hardware drivers (src/hw)
HWOBJS  := AU_CARDS.obj DMABUFF.obj AC97MIX.obj AK4531MIX.obj SC_INTHD.obj SC_ICH.obj \
	SC_E1370.obj SC_E1371.obj SC_CS4281.obj SC_VIA82.obj SC_SBLIV.obj SC_SBL24.obj \
	SC_SBXFI.obj CTHW20K1.obj CTHW20K2.obj

OBJS := $(addprefix $(OUT)/,$(VXDOBJS) $(EMUOBJS) $(HWOBJS))

all: $(OUT)/VSBVXD.386

$(OUT):
	mkdir -p $@

$(OUT)/%.obj: %.ASM VXD.INC | $(OUT)
	$(ASM) -q -Fo$@ $<

$(OUT)/%.obj: %.C VXDLIB.H CONFIG.H | $(OUT)
	$(CC) $(COPT) $(INC) -fo=$@ $<

$(OUT)/%.obj: $(SRC)/%.C | $(OUT)
	$(CC) $(COPT) $(INC) -fo=$@ $<

$(OUT)/%.obj: $(SRC)/hw/%.C | $(OUT)
	$(CC) $(COPT) $(INC) -fo=$@ $<

$(OUT)/%.obj: %.CPP | $(OUT)
	$(CPP) $(CPPOPT) $(INC) -fo=$@ $<

$(OUT)/%.obj: $(SRC)/%.CPP | $(OUT)
	$(CPP) $(CPPOPT) $(INC) -fo=$@ $<

$(OUT)/VSBVXD.386: $(OBJS)
	cd $(OUT) && $(LINK) format windows vxd name VSBVXD.386 option quiet option map=VSBVXD.map \
	  $(foreach o,$(OBJS),file $(notdir $(o))) \
	  libpath $(WATCOM)/lib386 libpath $(WATCOM)/lib386/dos library plib3s.lib library clib3s.lib \
	  segment _TEXT PRELOAD NONDISCARDABLE \
	  segment _DATA PRELOAD NONDISCARDABLE \
	  export VSBVXD_DDB.1=_VSBVXD_DDB
	python3 lefix.py $@

# like VSBHDA's Makefile: VOPL3.CPP (the C interface of the OPL3 emulation)
# with -ecc; wpp386 can't compile the C++ headers with -ecc (DBOPL.CPP)
$(OUT)/VOPL3.obj: $(SRC)/VOPL3.CPP | $(OUT)
	$(CPP) $(CPPOPT) -ecc $(INC) -fo=$@ $<

clean:
	rm -rf $(OUT)

.PHONY: all clean
