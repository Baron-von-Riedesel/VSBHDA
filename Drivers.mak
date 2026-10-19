
# Create sound drivers with Open Watcom and JWasm.
# Enter
#   wmake -f Drivers.mak
# Optionally, for a debug version, enter
#   wmake -f Drivers.mak debug=1

# 1. Adjust WATCOM - directory where Open Watcom is installed.
# 2. Adjust USEJWL - should be 0 if JWLink isn't available
#                    and OW's WLink is to be used.
#
# The Makefile assumes that the OW Win32 branch is used.
# To use it unmodified in DOS requires the HX DOS extender and
# to setup HX for full Win32 emulation:
#  C:\>HDPMI32 -a -r
#  C:\>SET DPMILDR=8
#  C:\>HXLDR32

!ifndef DEBUG
DEBUG=0
!endif

!ifndef WATCOM
WATCOM=\ow20
!endif

# use jwlink (1) or wlink (0)
!ifndef USEJWL
USEJWL=1
!endif

CC=$(WATCOM)\binnt\wcc386.exe
CPP=$(WATCOM)\binnt\wpp386.exe
LIB=$(WATCOM)\binnt\wlib.exe
ASM=jwasm.exe

!if $(USEJWL)
LINK=jwlink.exe
!else
LINK=$(WATCOM)\binnt\wlink.exe
!endif

!if $(DEBUG)
OUTD=driverd
C_DEBUG_FLAGS=-D_DEBUG
A_DEBUG_FLAGS=-D_DEBUG -Fl$* -Sg
!else
OUTD=driver
C_DEBUG_FLAGS=
A_DEBUG_FLAGS=
!endif

C_FLAGS=-q -oxa -ms -ecc -5s -fp5 -fpi87 -wcd=111
C_EXTRA_FLAGS=-DNOTFLAT
LD_FLAGS=

INCLUDES=-Iinclude -I$(WATCOM)\h
LIBS=$(OUTD)\drvhlp.lib $(WATCOM)\lib386\dos\clib3s.lib

{srcdrv\startup}.asm{$(OUTD)}.obj
	@$(ASM) -q -zcw -D?MODEL=tiny $(A_DEBUG_FLAGS) -Fo$@ $<

{srcdrv}.asm{$(OUTD)}.obj
	@$(ASM) -q -D?MODEL=tiny $(A_DEBUG_FLAGS) -Fo$@ $<

{srcdrv}.c{$(OUTD)}.obj
	@$(CC) $(C_FLAGS) $(C_DEBUG_FLAGS) $(C_EXTRA_FLAGS) -Isrcdrv $(INCLUDES) -fo=$@ $<

all: $(OUTD) $(OUTD)\CS4281.DRV $(OUTD)\ES1370.DRV $(OUTD)\ES1371.DRV $(OUTD)\ICH.DRV $(OUTD)\HDA.DRV $(OUTD)\SBGEN.DRV $(OUTD)\SBXFI.DRV $(OUTD)\VIA82.DRV

$(OUTD):
	@mkdir $(OUTD)

HLPOBJ = &
	$(OUTD)\physmem.obj   $(OUTD)\dmabuff.obj  $(OUTD)\pcibios.obj  $(OUTD)\timer.obj   $(OUTD)\ac97mix.obj &
	$(OUTD)\cthw20k1.obj  $(OUTD)\cthw20k2.obj $(OUTD)\sblive.obj   $(OUTD)\sblive24.obj &
	$(OUTD)\dpmidrv.obj   $(OUTD)\dprintf.obj  $(OUTD)\vioout.obj &
	$(OUTD)\libmain32.obj $(OUTD)\malloc.obj   $(OUTD)\sbrk.obj

$(OUTD)/CS4281.DRV: $(OUTD)\sc_cs4281.obj  $(OUTD)\drvhlp.lib
	@jwlink format dos @<<
file $(OUTD)\sc_cs4281.obj
name $@
alias __sndcard_info=_CS4281_sndcard_info
lib { $(LIBS) }
disable 1014
op q,statics,m=$*.map,nodefaultlibs
<<

$(OUTD)/ES1370.DRV: $(OUTD)\sc_e1370.obj  $(OUTD)\drvhlp.lib
	@jwlink format dos @<<
file $(OUTD)\sc_e1370.obj
name $@
alias __sndcard_info=_ES1370_sndcard_info
lib { $(LIBS) }
disable 1014
op q,statics,m=$*.map,nodefaultlibs
<<

$(OUTD)/ES1371.DRV: $(OUTD)\sc_e1371.obj  $(OUTD)\drvhlp.lib
	@jwlink format dos @<<
file $(OUTD)\sc_e1371.obj
name $@
alias __sndcard_info=_ES1371_sndcard_info
lib { $(LIBS) }
disable 1014
op q,statics,m=$*.map,nodefaultlibs
<<

$(OUTD)/ICH.DRV: $(OUTD)\sc_ich.obj  $(OUTD)\drvhlp.lib
	@jwlink format dos @<<
file $(OUTD)\sc_ich.obj
name $@
alias __sndcard_info=_ICH_sndcard_info
lib { $(LIBS) }
disable 1014
op q,statics,m=$*.map,nodefaultlibs
<<

$(OUTD)/HDA.DRV: $(OUTD)\sc_inthd.obj  $(OUTD)\drvhlp.lib
	@jwlink format dos @<<
file $(OUTD)\sc_inthd.obj
name $@
alias __sndcard_info=_HDA_sndcard_info
lib { $(LIBS) }
disable 1014
op q,statics,m=$*.map,nodefaultlibs
<<

$(OUTD)/SBGEN.DRV: $(OUTD)\sc_sbgen.obj $(OUTD)\drvhlp.lib
	@jwlink format dos @<<
file $(OUTD)\sc_sbgen
name $@
alias __sndcard_info=_SBGEN_sndcard_info
lib { $(LIBS) }
disable 1014
op q,statics,m=$*.map,nodefaultlibs
<<

$(OUTD)/SBXFI.DRV: $(OUTD)\sc_sbxfi.obj $(OUTD)\drvhlp.lib
	@jwlink format dos @<<
file $(OUTD)\sc_sbxfi
name $@
alias __sndcard_info=_SBXFI_sndcard_info
lib { $(LIBS) }
disable 1014
op q,statics,m=$*.map,nodefaultlibs
<<

$(OUTD)/VIA82.DRV: $(OUTD)\sc_via82.obj  $(OUTD)\drvhlp.lib
	@jwlink format dos @<<
file $(OUTD)\sc_via82.obj
name $@
alias __sndcard_info=_VIA82XX_sndcard_info
lib { $(LIBS) }
disable 1014
op q,statics,m=$*.map,nodefaultlibs
<<

$(OUTD)/drvhlp.lib: $(HLPOBJ)
	@$(LIB) -q -b -n $@ $(HLPOBJ)

$(OUTD)\sc_cs4281.obj: srcdrv\sc_cs4281.c
$(OUTD)\sc_e1370.obj:  srcdrv\sc_e1370.c
$(OUTD)\sc_e1371.obj:  srcdrv\sc_e1371.c
$(OUTD)\sc_ich.obj:    srcdrv\sc_ich.c
$(OUTD)\sc_inthd.obj:  srcdrv\sc_inthd.c
$(OUTD)\sc_sbgen.obj:  srcdrv\sc_sbgen.c
$(OUTD)\sc_sbxfi.obj:  srcdrv\sc_sbxfi.c
$(OUTD)\sc_via82.obj:  srcdrv\sc_via82.c

$(OUTD)\cthw20k1.obj:  srcdrv\cthw20k1.c
$(OUTD)\cthw20k2.obj:  srcdrv\cthw20k2.c
$(OUTD)\sblive.obj:    srcdrv\sblive.c
$(OUTD)\sblive24.obj:  srcdrv\sblive24.c

$(OUTD)\ac97mix.obj:   srcdrv\ac97mix.c
$(OUTD)\dmabuff.obj:   srcdrv\dmabuff.c
$(OUTD)\pcibios.obj:   srcdrv\pcibios.c
$(OUTD)\physmem.obj:   srcdrv\physmem.c
$(OUTD)\timer.obj:     srcdrv\timer.c

$(OUTD)\dpmidrv.obj:   srcdrv\dpmidrv.asm

$(OUTD)\dprintf.obj:   srcdrv\startup\dprintf.asm
$(OUTD)\libmain32.obj: srcdrv\startup\libmain32.asm
$(OUTD)\malloc.obj:    srcdrv\startup\malloc.asm
$(OUTD)\sbrk.obj:      srcdrv\startup\sbrk.asm
$(OUTD)\vioout.obj:    srcdrv\startup\vioout.asm

clean: .SYMBOLIC
	@del $(OUTD)\*.drv
	@del $(OUTD)\*.obj
	@del $(OUTD)\*.lib
	@del $(OUTD)\*.map
