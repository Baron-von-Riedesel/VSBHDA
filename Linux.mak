# Create vsbhda.exe with Open Watcom and JWasm/JWlink.
# Enter
#   make -f Linux.mak
# Optionally, for a debug version, enter
#   make -f Linux.mak DEBUG=1

# 1. Adjust WATCOM - directory where Open Watcom is installed.
# 2. Adjust USE19  - should be 1 if OW v1.9 is to be used.

ifndef DEBUG
DEBUG=0
endif

ifndef WATCOM
WATCOM=$(HOME)/Watcom
endif

# use OW v2 (0) or OW v1.9 (1)
ifndef USE19
USE19=0
endif

# use jwlink (1) or wlink (0)
ifndef USEJWL
USEJWL=1
endif

ifndef DYNAMIC
DYNAMIC=1
endif

# activate next line if FM synth should be deactivated
#NOFM=1

CC=$(WATCOM)/binl/wcc386
CPP=$(WATCOM)/binl/wpp386
LIB=$(WATCOM)/binl/wlib
ASM=jwasm

ifeq ($(USEJWL),1)
LINK=jwlink
FMTHX=hx
CONSTATTR=segment CONST readonly
CONST2ATTR=segment CONST2 readonly
else
LINK=$(WATCOM)/binl/wlink
FMTHX=
endif

NAME=VSBHDA

C_EXTRA_FLAGS=

ifeq ($(DEBUG),1)
OUTD=owd
OUTD16=ow16d
C_DEBUG_FLAGS=-D_DEBUG
A_DEBUG_FLAGS=-D_DEBUG -Fl$* -Sg
else
OUTD=ow
OUTD16=ow16
C_DEBUG_FLAGS=
A_DEBUG_FLAGS=
endif

ifeq ($(USE19),1)
OW19=-DOW19
endif

ifeq ($(DYNAMIC),1)
LOADDRVOBJ=$(OUTD)/LOADDRV.obj
MAKE16=Linux16.mak
else
C_EXTRA_FLAGS=-DFLAT
MAKE16=Linux16l.mak
endif

ifndef NOFM
FMOBJS=$(OUTD)/DBOPL.obj	$(OUTD)/VOPL3.obj
endif

OBJFILES = \
	$(OUTD)/MAIN.obj		$(OUTD)/SNDISR.obj		$(OUTD)/PTRAP.obj		$(OUTD)/LINEAR.obj		$(OUTD)/PIC.obj \
	$(OUTD)/VSB.obj			$(OUTD)/VDMA.obj		$(OUTD)/VIRQ.obj		$(OUTD)/VMPU.obj		$(OUTD)/TSF.obj \
	$(OUTD)/ADPCM.obj		$(OUTD)/AU_CARDS.obj	$(FMOBJS) 	$(LOADDRVOBJ) \
	$(OUTD)/STACKIO.obj		$(OUTD)/STACKISR.obj	$(OUTD)/SBISR.obj		$(OUTD)/INT31.obj		$(OUTD)/RMWRAP.obj		$(OUTD)/MIXER.obj \
	$(OUTD)/HAPI.obj		$(OUTD)/DPRINTF.obj		$(OUTD)/VIOOUT.obj		$(OUTD)/DPMI.obj		$(OUTD)/UNINST.obj		$(OUTD)/GETENV.obj \
	$(OUTD)/MALLOC.obj		$(OUTD)/SBRK.obj		$(OUTD)/FILEACC.obj		$(OUTD)/LOGFILE.obj		$(OUTD)/STRTOL.obj		$(OUTD)/_MATHERR.obj \
	$(OUTD)/DPMIISR.obj

ifeq ($(DYNAMIC),0)
OBJFILES += \
	$(OUTD)/AC97MIX.obj		$(OUTD)/DMABUFF.obj		$(OUTD)/PCIBIOS.obj		$(OUTD)/PHYSMEM.obj		$(OUTD)/TIMER.obj \
	$(OUTD)/SC_E1371.obj	$(OUTD)/SC_ICH.obj		$(OUTD)/SC_INTHD.obj	$(OUTD)/SC_VIA82.obj \
	$(OUTD)/SC_SBGEN.obj	$(OUTD)/SBLIVE.obj		$(OUTD)/SBLIVE24.obj \
	$(OUTD)/SC_SBXFI.obj	$(OUTD)/CTHW20K1.obj	$(OUTD)/CTHW20K2.obj \
	$(OUTD)/SC_E1370.obj	$(OUTD)/SC_CS4281.obj
endif

C_OPT_FLAGS=-q -mf -oxa -ecc -5s -fp5 -fpi87 -wcd=111
# OW's wpp386 doesn't like the -ecc option
CPP_OPT_FLAGS=-q -oxa -mf -bc -5s -fp5 -fpi87 
ifdef NOFM
C_EXTRA_FLAGS= $(C_EXTRA_FLAGS) -DNOFM
endif
LD_FLAGS=
LD_EXTRA_FLAGS=op M=$(OUTD)/$(NAME).map

INCLUDES=-I$(WATCOM)/h -Iinclude
LIBS=

$(OUTD)/%.obj: src/%.ASM
	@$(ASM) -q -D?MODEL=flat -Isrc/startup $(A_DEBUG_FLAGS) -Fo$@ $<

$(OUTD)/%.obj: src/%.C
	@$(CC) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) -Isrc $(INCLUDES) -fo=$@ $<

$(OUTD)/%.obj: src/%.CPP
	@$(CPP) $(C_DEBUG_FLAGS) $(CPP_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CPPFLAGS) -Isrc $(INCLUDES) -fo=$@ $<

ifeq ($(DYNAMIC),0)
$(OUTD)/%.obj: srcdrv/%.C
	@$(CC) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) -Isrcdrv $(INCLUDES) -fo=$@ $<
endif

$(OUTD)/%.obj: src/startup/%.ASM
	@$(ASM) -q -zcw -D?MODEL=flat $(OW19) $(A_DEBUG_FLAGS) -Fo$@ $<

all: $(OUTD) $(OUTD)/$(NAME).EXE $(OUTD16)/$(NAME)16.EXE

$(OUTD):
	@mkdir $(OUTD)

$(OUTD)/$(NAME).EXE: $(OUTD)/$(NAME).lib $(OUTD)/CSTRTDHX.obj
	@$(LINK) \
format win pe $(FMTHX) runtime console \
file $(OUTD)/CSTRTDHX.obj, $(OUTD)/MAIN.obj, $(OUTD)/LINEAR.obj \
name $@ \
libpath $(WATCOM)/lib386/dos:$(WATCOM)/lib386 \
lib $(OUTD)/$(NAME).lib \
op q,m=$(OUTD)/$(NAME).map,stub=res/LOADPERO.BIN,stack=0x10000,heap=0x1000 \
$(CONSTATTR) $(CONST2ATTR)

$(OUTD16)/$(NAME)16.EXE:
	@make -f $(MAKE16) DEBUG=$(DEBUG) WATCOM=$(WATCOM) USE19=$(USE19) USEJWL=$(USEJWL)

$(OUTD)/$(NAME).lib: $(OBJFILES)
	@$(LIB) -q -b -n $(OUTD)/$(NAME).lib $(OBJFILES)

ifeq ($(DYNAMIC),0)
$(OUTD)/AC97MIX.obj:   srcdrv/AC97MIX.C
$(OUTD)/CTHW20K1.obj:  srcdrv/CTHW20K1.C
$(OUTD)/CTHW20K2.obj:  srcdrv/CTHW20K2.C
$(OUTD)/DMABUFF.obj:   srcdrv/DMABUFF.C
$(OUTD)/PHYSMEM.obj:   srcdrv/PHYSMEM.C
$(OUTD)/PCIBIOS.obj:   srcdrv/PCIBIOS.C
$(OUTD)/SBLIVE24.obj:  srcdrv/SBLIVE24.C
$(OUTD)/SBLIVE.obj:    srcdrv/SBLIVE.C
$(OUTD)/SC_CS4281.obj: srcdrv/SC_CS4281.C
$(OUTD)/SC_E1370.obj:  srcdrv/SC_E1370.C
$(OUTD)/SC_E1371.obj:  srcdrv/SC_E1371.C
$(OUTD)/SC_ICH.obj:    srcdrv/SC_ICH.C
$(OUTD)/SC_INTHD.obj:  srcdrv/SC_INTHD.C
$(OUTD)/SC_SBGEN.obj:  srcdrv/SC_SBGEN.C
$(OUTD)/SC_SBXFI.obj:  srcdrv/SC_SBXFI.C
$(OUTD)/SC_VIA82.obj:  srcdrv/SC_VIA82.C
$(OUTD)/TIMER.obj:     srcdrv/TIMER.C
else
$(OUTD)/LOADDRV.obj:   src/LOADDRV.ASM
endif

$(OUTD)/ADPCM.obj:     src/ADPCM.C
$(OUTD)/AU_CARDS.obj:  src/AU_CARDS.C
$(OUTD)/DPMI.obj:      src/DPMI.ASM
$(OUTD)/DPMIISR.obj:   src/DPMIISR.ASM
$(OUTD)/FILEACC.obj:   src/FILEACC.ASM
$(OUTD)/HAPI.obj:      src/HAPI.ASM
$(OUTD)/INT31.obj:     src/INT31.ASM
$(OUTD)/LINEAR.obj:    src/LINEAR.C
$(OUTD)/LOGFILE.obj:   src/LOGFILE.ASM
$(OUTD)/MAIN.obj:      src/MAIN.C
$(OUTD)/MIXER.obj:     src/MIXER.ASM
$(OUTD)/PIC.obj:       src/PIC.C
$(OUTD)/PTRAP.obj:     src/PTRAP.C
$(OUTD)/SBISR.obj:     src/SBISR.ASM
$(OUTD)/SNDISR.obj:    src/SNDISR.C
$(OUTD)/STACKIO.obj:   src/STACKIO.ASM
$(OUTD)/STACKISR.obj:  src/STACKISR.ASM
$(OUTD)/TSF.obj:       src/TSF.C
$(OUTD)/UNINST.obj:    src/UNINST.ASM
$(OUTD)/VDMA.obj:      src/VDMA.C
$(OUTD)/VIRQ.obj:      src/VIRQ.C
$(OUTD)/VMPU.obj:      src/VMPU.C
$(OUTD)/VSB.obj:       src/VSB.C
ifndef NOFM
$(OUTD)/DBOPL.obj:     src/DBOPL.CPP
$(OUTD)/VOPL3.obj:     src/VOPL3.CPP
	@$(CPP) $(C_DEBUG_FLAGS) -q -oxa -mf -bc -ecc -5s -fp5 -fpi87 $(C_EXTRA_FLAGS) $(CPPFLAGS) $(INCLUDES) -fo=$@ $<
endif
$(OUTD)/CSTRTDHX.obj:  src/startup/CSTRTDHX.ASM
$(OUTD)/DPRINTF.obj:   src/startup/DPRINTF.ASM
$(OUTD)/GETENV.obj:    src/startup/GETENV.ASM
$(OUTD)/MALLOC.obj:    src/startup/MALLOC.ASM
$(OUTD)/SBRK.obj:      src/startup/SBRK.ASM
$(OUTD)/STRTOL.obj:    src/startup/STRTOL.ASM
$(OUTD)/VIOOUT.obj:    src/startup/VIOOUT.ASM
$(OUTD)/_MATHERR.obj:  src/startup/_MATHERR.ASM

# to avoid any issues with 16-bit relocations in PE binaries,
# the 16-bit code is included in binary format into rmwrap.asm.

$(OUTD)/RMWRAP.obj:    src/RMWRAP.ASM src/RMCODE1.ASM src/RMCODE2.ASM
	@$(ASM) -q -bin -Fl$(OUTD)/ -Fo$(OUTD)/rmcode1.bin src/RMCODE1.ASM
	@$(ASM) -q -bin -Fl$(OUTD)/ -Fo$(OUTD)/rmcode2.bin src/RMCODE2.ASM
	@$(ASM) -q -D?MODEL=flat $(OW19) -Fo$@ -DOUTD=$(OUTD) src/RMWRAP.ASM

clean:
	@make -f $(MAKE16) DEBUG=$(DEBUG) clean
	@rm -f $(OUTD)/$(NAME).EXE
	@rm -f $(OUTD)/$(NAME).lib
	@rm -f $(OUTD)/*.obj
	@rm -f $(OUTD)/*.map
	@rm -f $(OUTD)/rmcode?.bin
