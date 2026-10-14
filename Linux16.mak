
# Create vsbhda16.exe with Open Watcom and JWasm.
# Enter
#   make -f Linux16.mak
# Optionally, for a debug version, enter
#   make -f Linux16.mak DEBUG=1

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
ifndef USEJWL
USEJWL=1
endif

# activate next line if FM synth should be deactivated
#NOFM=1

NAME=VSBHDA16

CC=$(WATCOM)/binl/wcc386
CPP=$(WATCOM)/binl/wpp386
ifeq ($(USEJWL),1)
LINK=jwlink
else
LINK=$(WATCOM)/binl/wlink
endif
LIB=$(WATCOM)/binl/wlib
ASM=jwasm

ifeq ($(DEBUG),1)
OUTD=ow16d
C_DEBUG_FLAGS=-D_DEBUG
A_DEBUG_FLAGS=-D_DEBUG -Fl=$*
else
OUTD=ow16
C_DEBUG_FLAGS=-D_LOG
A_DEBUG_FLAGS=
endif

ifeq ($(USE19),1)
OW19=-DOW19
endif

ifndef NOFM
FMOBJS=$(OUTD)/DBOPL.obj	$(OUTD)/VOPL3.obj
endif

OBJFILES = \
	$(OUTD)/MAIN.obj		$(OUTD)/SNDISR.obj		$(OUTD)/PTRAP.obj		$(OUTD)/LINEAR.obj		$(OUTD)/PIC.obj \
	$(OUTD)/VSB.obj			$(OUTD)/VDMA.obj		$(OUTD)/VIRQ.obj		$(OUTD)/VMPU.obj		$(OUTD)/TSF.obj \
	$(OUTD)/STACKIO.obj		$(OUTD)/STACKISR.obj	$(OUTD)/SBISR.obj		$(OUTD)/INT31.obj		$(OUTD)/RMWRAP.obj \
	$(OUTD)/MIXER.obj		$(OUTD)/ADPCM.obj		$(FMOBJS) \
	$(OUTD)/HAPI.obj		$(OUTD)/DPRINTF.obj		$(OUTD)/VIOOUT.obj		$(OUTD)/DPMI.obj		$(OUTD)/UNINST.obj \
	$(OUTD)/LDMOD16.obj		$(OUTD)/SBRK.obj		$(OUTD)/MALLOC.obj		$(OUTD)/RTE200.obj		$(OUTD)/AU_CARDS.obj \
	$(OUTD)/FILEACC.obj		$(OUTD)/LOGFILE.obj		$(OUTD)/GETENV.obj		$(OUTD)/STRTOL.obj		$(OUTD)/_MATHERR.obj \
	$(OUTD)/DPMIISR.obj		$(OUTD)/LOADDRV.obj

C_OPT_FLAGS=-q -oxa -ms -ecc -5s -fp5 -fpi87 -wcd=111
# OW's wpp386 doesn't like the -ecc option ("function modifier cannot be used ...")
CPP_OPT_FLAGS=-q -oxa -ms -bc -5s -fp5 -fpi87 
C_EXTRA_FLAGS=-DNOTFLAT -DNOSBXFI
ifdef NOFM
C_EXTRA_FLAGS= $(C_EXTRA_FLAGS) -DNOFM
endif

INCLUDES=-I$(WATCOM)/h
LIBS=

$(OUTD)/%.obj: src/%.ASM
	@$(ASM) -q -DNOTFLAT -Isrc/startup -D?MODEL=small $(A_DEBUG_FLAGS) -Fo$@ $<

$(OUTD)/%.obj: src/%.C
	@$(CC) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) -os $(C_EXTRA_FLAGS) $(CFLAGS) -Isrc $(INCLUDES) -fo=$@ $<

$(OUTD)/%.obj: src/%.CPP
	@$(CPP) $(C_DEBUG_FLAGS) $(CPP_OPT_FLAGS) -os $(C_EXTRA_FLAGS) $(CPPFLAGS) -Isrc $(INCLUDES) -fo=$@ $<

$(OUTD)/%.obj: src/hw/%.C
	@$(CC) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CFLAGS) -Isrc/hw -Isrc $(INCLUDES) -fo=$@ $<

$(OUTD)/%.obj: src/startup/%.ASM
	@$(ASM) -q -zcw -DNOTFLAT -D?MODEL=small $(OW19) $(A_DEBUG_FLAGS) -Fo$@ $<

$(OUTD)/%.obj: src/startup/%.C
	@$(CC) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CFLAGS) $(INCLUDES) -fo=$@ $<

all: $(OUTD) $(OUTD)/$(NAME).EXE

$(OUTD):
	@mkdir $(OUTD)

$(OUTD)/$(NAME).EXE: $(OUTD)/$(NAME).lib $(OUTD)/CSTRT16X.obj $(OUTD)/INIT1632.obj
	@$(LINK) \
format dos \
file $(OUTD)/INIT1632.obj, $(OUTD)/CSTRT16X.obj, $(OUTD)/MAIN.obj name $@ \
libpath $(WATCOM)/lib386/dos:$(WATCOM)/lib386 \
lib $(OUTD)/$(NAME).lib \
op q,statics,m=$(OUTD)/$(NAME).map \
disable 80

$(OUTD)/$(NAME).lib: $(OBJFILES)
	@$(LIB) -q -b -n $(OUTD)/$(NAME).lib $(OBJFILES)

$(OUTD)/AU_CARDS.obj:  src/hw/AU_CARDS.C

$(OUTD)/ADPCM.obj:     src/ADPCM.C
$(OUTD)/DPMI.obj:      src/DPMI.ASM
$(OUTD)/DPMIISR.obj:   src/DPMIISR.ASM
$(OUTD)/DPRINTF.obj:   src/DPRINTF.ASM
$(OUTD)/FILEACC.obj:   src/FILEACC.ASM
$(OUTD)/HAPI.obj:      src/HAPI.ASM
$(OUTD)/INT31.obj:     src/INT31.ASM
$(OUTD)/LINEAR.obj:    src/LINEAR.C
$(OUTD)/LOADDRV.obj:   src/LOADDRV.ASM
$(OUTD)/LOGFILE.obj:   src/LOGFILE.ASM
$(OUTD)/MAIN.obj:      src/MAIN.C
$(OUTD)/MIXER.obj:     src/MIXER.ASM
$(OUTD)/PIC.obj:       src/PIC.C
$(OUTD)/PTRAP.obj:     src/PTRAP.C
$(OUTD)/RTE200.obj:    src/RTE200.ASM
$(OUTD)/SBISR.obj:     src/SBISR.ASM
$(OUTD)/SNDISR.obj:    src/SNDISR.C
$(OUTD)/STACKIO.obj:   src/STACKIO.ASM
$(OUTD)/STACKISR.obj:  src/STACKISR.ASM
$(OUTD)/TSF.obj:       src/TSF.C
$(OUTD)/UNINST.obj:    src/UNINST.ASM
$(OUTD)/VDMA.obj:      src/VDMA.C
$(OUTD)/VIOOUT.obj:    src/VIOOUT.ASM
$(OUTD)/VIRQ.obj:      src/VIRQ.C
$(OUTD)/VMPU.obj:      src/VMPU.C
$(OUTD)/VSB.obj:       src/VSB.C
ifndef NOFM
$(OUTD)/DBOPL.obj:     src/DBOPL.CPP

$(OUTD)/VOPL3.obj:     src/VOPL3.CPP
	@$(CPP) $(C_DEBUG_FLAGS) -q -oxa -ms -bc -ecc -5s -fp5 -fpi87 $(C_EXTRA_FLAGS) $(CPPFLAGS) $(INCLUDES) -fo=$@ $<
endif

$(OUTD)/CSTRT16X.obj:  src/startup/CSTRT16X.ASM
$(OUTD)/DSTRT16X.obj:  src/startup/DSTRT16X.ASM
$(OUTD)/LDMOD16.obj:   src/startup/LDMOD16.ASM
$(OUTD)/INIT1632.obj:  src/startup/INIT1632.ASM
$(OUTD)/GETENV.obj:    src/startup/GETENV.ASM
$(OUTD)/MALLOC.obj:    src/startup/MALLOC.ASM
$(OUTD)/SBRK.obj:      src/startup/SBRK.ASM
$(OUTD)/STRTOL.obj:    src/startup/STRTOL.ASM
$(OUTD)/_MATHERR.obj:  src/startup/_MATHERR.ASM
$(OUTD)/LIBMAIN.obj:   src/startup/LIBMAIN.C

# the 16-bit code is included in binary format into rmwrap.asm.

$(OUTD)/RMWRAP.obj:    src/RMWRAP.ASM src/RMCODE1.ASM src/RMCODE2.ASM
	@$(ASM) -q -bin -Fl$(OUTD)/ -Fo$(OUTD)/rmcode1.bin src/RMCODE1.ASM
	@$(ASM) -q -bin -Fl$(OUTD)/ -Fo$(OUTD)/rmcode2.bin src/RMCODE2.ASM
	@$(ASM) -q -DNOTFLAT -D?MODEL=small $(OW19) -Fl$(OUTD)/ -Fo$@ -DOUTD=$(OUTD) src/RMWRAP.ASM

clean:
	@rm -f $(OUTD)/$(NAME).lib
	@rm -f $(OUTD)/$(NAME).EXE
	@rm -f $(OUTD)/*.obj
	@rm -f $(OUTD)/*.map
	@rm -f $(OUTD)/*.lst
	@rm -f $(OUTD)/rmcode?.bin
