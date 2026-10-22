
# Create sound drivers with Open Watcom and JWasm.
# Enter
#   make -f LinuxDrv.mak
# Optionally, for a debug version, enter
#   make -f LinuxDrv.mak DEBUG=1

# 1. Adjust WATCOM - directory where Open Watcom is installed.
# 2. Adjust USEJWL - should be 0 if JWLink isn't available
#                    and OW's WLink is to be used.

ifndef DEBUG
DEBUG=0
endif

ifndef WATCOM
WATCOM=$(HOME)/Watcom
endif

# use jwlink (1) or wlink (0)
ifndef USEJWL
USEJWL=1
endif

CC=$(WATCOM)/binl/wcc386
CPP=$(WATCOM)/binl/wpp386
LIB=$(WATCOM)/binl/wlib
ASM=jwasm

ifeq ($(USEJWL),1)
LINK=jwlink
else
LINK=$(WATCOM)/binl/wlink
endif

ifeq ($(DEBUG),1)
OUTD=owdrvd
C_DEBUG_FLAGS=-D_DEBUG
A_DEBUG_FLAGS=-D_DEBUG -Fl$* -Sg
else
OUTD=owdrv
C_DEBUG_FLAGS=
A_DEBUG_FLAGS=
endif

C_FLAGS=-q -oxa -ms -ecc -5s -fp5 -fpi87 -wcd=111
C_EXTRA_FLAGS=
LD_FLAGS=

INCLUDES=-Iinclude -I$(WATCOM)/h 
LIBS=$(OUTD)/drvhlp.lib $(WATCOM)/lib386/dos/clib3s.lib

all: $(OUTD) $(OUTD)/CS4281.DRV $(OUTD)/ES1370.DRV $(OUTD)/ES1371.DRV $(OUTD)/ICH.DRV $(OUTD)/HDA.DRV $(OUTD)/SBGEN.DRV $(OUTD)/SBXFI.DRV $(OUTD)/VIA82.DRV

$(OUTD):
	@mkdir $(OUTD)

HLPOBJ = \
	$(OUTD)/PHYSMEM.obj   $(OUTD)/DMABUFF.obj  $(OUTD)/PCIBIOS.obj  $(OUTD)/TIMER.obj   $(OUTD)/AC97MIX.obj \
	$(OUTD)/LIBMAIN32.obj $(OUTD)/DPMI30X.obj  $(OUTD)/DPMI507.obj $(OUTD)/DPMI80X.obj \
	$(OUTD)/DPRINTF.obj $(OUTD)/VIOOUT.obj     $(OUTD)/MALLOC.obj   $(OUTD)/SBRK.obj

$(OUTD)/%.obj: srcdrv/%.ASM
	@$(ASM) -q $(A_DEBUG_FLAGS) -Fo$@ $<

$(OUTD)/%.obj: srcdrv/startup/%.ASM
	@$(ASM) -q -zcw $(A_DEBUG_FLAGS) -Fo$@ $<

$(OUTD)/%.obj: srcdrv/%.C
	@$(CC) $(C_FLAGS) $(C_DEBUG_FLAGS) $(C_EXTRA_FLAGS) -Isrcdrv $(INCLUDES) -fo=$@ $<

$(OUTD)/CS4281.DRV: $(OUTD)/SC_CS4281.obj  $(OUTD)/drvhlp.lib
	@jwlink format dos \
file { $(OUTD)/SC_CS4281.obj } \
name $@ \
alias __sndcard_info=_CS4281_sndcard_info \
lib { $(LIBS) } \
disable 1014 \
op q,statics,m=$(OUTD)/CS4281.map,nodefaultlibs

$(OUTD)/ES1370.DRV: $(OUTD)/SC_E1370.obj  $(OUTD)/drvhlp.lib
	@jwlink format dos \
file { $(OUTD)/SC_E1370.obj } \
name $@ \
alias __sndcard_info=_ES1370_sndcard_info \
lib { $(LIBS) } \
disable 1014 \
op q,statics,m=$(OUTD)/ES1370.map,nodefaultlibs

$(OUTD)/ES1371.DRV: $(OUTD)/SC_E1371.obj  $(OUTD)/drvhlp.lib
	@jwlink format dos \
file { $(OUTD)/SC_E1371.obj } \
name $@ \
alias __sndcard_info=_ES1371_sndcard_info \
lib { $(LIBS) } \
disable 1014 \
op q,statics,m=$(OUTD)/ES1371.map,nodefaultlibs

$(OUTD)/ICH.DRV: $(OUTD)/SC_ICH.obj  $(OUTD)/drvhlp.lib
	@jwlink format dos \
file { $(OUTD)/SC_ICH.obj } \
name $@ \
alias __sndcard_info=_ICH_sndcard_info \
lib { $(LIBS) } \
disable 1014 \
op q,statics,m=$(OUTD)/ICH.map,nodefaultlibs

$(OUTD)/HDA.DRV: $(OUTD)/SC_INTHD.obj  $(OUTD)/drvhlp.lib
	@jwlink format dos \
file { $(OUTD)/SC_INTHD.obj } \
name $@ \
alias __sndcard_info=_HDA_sndcard_info \
lib { $(LIBS) } \
disable 1014 \
op q,statics,m=$(OUTD)/HDA.map,nodefaultlibs

$(OUTD)/SBGEN.DRV: $(OUTD)/SC_SBGEN.obj $(OUTD)/SBLIVE.obj $(OUTD)/SBLIVE24.obj $(OUTD)/drvhlp.lib
	@jwlink format dos \
file { $(OUTD)/SBLIVE.obj $(OUTD)/SBLIVE24.obj $(OUTD)/SC_SBGEN.obj } \
name $@ \
alias __sndcard_info=_SBGEN_sndcard_info \
lib { $(LIBS) } \
disable 1014 \
op q,statics,m=$(OUTD)/SBGEN.map,nodefaultlibs

$(OUTD)/SBXFI.DRV: $(OUTD)/SC_SBXFI.obj $(OUTD)/CTHW20K1.obj $(OUTD)/CTHW20K2.obj $(OUTD)/drvhlp.lib
	@jwlink format dos \
file { $(OUTD)/SC_SBXFI.obj $(OUTD)/CTHW20K1.obj $(OUTD)/CTHW20K2.obj } \
name $@ \
alias __sndcard_info=_SBXFI_sndcard_info \
lib { $(LIBS) } \
disable 1014 \
op q,statics,m=$(OUTD)/SBXFI.map,nodefaultlibs

$(OUTD)/VIA82.DRV: $(OUTD)/SC_VIA82.obj  $(OUTD)/drvhlp.lib
	@jwlink format dos \
file { $(OUTD)/SC_VIA82.obj } \
name $@ \
alias __sndcard_info=_VIA82XX_sndcard_info \
lib { $(LIBS) } \
disable 1014 \
op q,statics,m=$(OUTD)/via82.map,nodefaultlibs

$(OUTD)/drvhlp.lib: $(HLPOBJ)
	@$(LIB) -q -b -n $@ $(HLPOBJ)

clean:
	@rm $(OUTD)/*.DRV
	@rm $(OUTD)/*.obj
	@rm $(OUTD)/*.lib
	@rm $(OUTD)/*.map
