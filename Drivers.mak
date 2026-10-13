
# Create sound drivers with Open Watcom and JWasm.
# Enter
#   wmake -f Drivers.mak
# Optionally, for a debug version, enter
#   wmake -f Drivers.mak debug=1

# 1. Adjust WATCOM - directory where Open Watcom is installed.
# 2. Adjust USE19  - should be 1 if OW v1.9 is to be used.
# 3. Adjust USEJWL - should be 0 if JWLink isn't available
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
# use OW v2 (0) or OW v1.9 (1)
!ifndef USE19
USE19=0
!endif
# use jwlink (1) or wlink (0)
!ifndef USEJWL
USEJWL=1
!endif
# activate next line if FM synth should be deactivated
#NOFM=1

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
OUTD=drvd
C_DEBUG_FLAGS=-D_DEBUG
A_DEBUG_FLAGS=-D_DEBUG -Fl$* -Sg
!else
OUTD=drv
C_DEBUG_FLAGS=
A_DEBUG_FLAGS=
!endif

!if $(USE19)
OW19=-DOW19
!endif

C_FLAGS=-q -oxa -ms -ecc -5s -fp5 -fpi87 -wcd=111
C_EXTRA_FLAGS=-DNOTFLAT
LD_FLAGS=

INCLUDES=-Isrc\hw -Isrc -I$(WATCOM)\h 
LIBS=\ow20\lib386\dos\clib3s.lib

all: $(OUTD) $(OUTD)\CS4281.DRV $(OUTD)\ES1370.DRV $(OUTD)\ES1371.DRV $(OUTD)\ICH.DRV $(OUTD)\HDA.DRV $(OUTD)\SBGEN.DRV $(OUTD)\SBXFI.DRV $(OUTD)\VIA82.DRV

$(OUTD):
	@mkdir $(OUTD)

MODOBJ = &
	$(OUTD)\physmem.obj   $(OUTD)\dmabuff.obj  $(OUTD)\pcibios.obj  $(OUTD)\timer.obj   $(OUTD)\ac97mix.obj &
	$(OUTD)\libmain32.obj $(OUTD)\dpmi.obj     $(OUTD)\dprintf.obj  $(OUTD)\vioout.obj  $(OUTD)\malloc.obj &
	$(OUTD)\sbrk.obj

$(OUTD)/CS4281.DRV: $(OUTD)\sc_cs4281.obj  $(MODOBJ)
	@jwlink format dos @<<
file { $(OUTD)\sc_cs4281.obj $(MODOBJ) }
name $@
alias __sndcard_info=_CS4281_sndcard_info
lib $(LIBS)
disable 1014
op q,statics,m=$(OUTD)\cs4281.map,nodefaultlibs
<<

$(OUTD)/ES1370.DRV: $(OUTD)\sc_e1370.obj  $(MODOBJ)
	@jwlink format dos @<<
file { $(OUTD)\sc_e1370.obj $(MODOBJ) }
name $@
alias __sndcard_info=_ES1370_sndcard_info
lib $(LIBS)
disable 1014
op q,statics,m=$(OUTD)\es1370.map,nodefaultlibs
<<

$(OUTD)/ES1371.DRV: $(OUTD)\sc_e1371.obj  $(MODOBJ)
	@jwlink format dos @<<
file { $(OUTD)\sc_e1371.obj $(MODOBJ) }
name $@
alias __sndcard_info=_ES1371_sndcard_info
lib $(LIBS)
disable 1014
op q,statics,m=$(OUTD)\es1371.map,nodefaultlibs
<<

$(OUTD)/ICH.DRV: $(OUTD)\sc_ich.obj  $(MODOBJ)
	@jwlink format dos @<<
file { $(OUTD)\sc_ich.obj $(MODOBJ) }
name $@
alias __sndcard_info=_ICH_sndcard_info
lib $(LIBS)
disable 1014
op q,statics,m=$(OUTD)\ich.map,nodefaultlibs
<<

$(OUTD)/HDA.DRV: $(OUTD)\sc_inthd.obj  $(MODOBJ)
	@jwlink format dos @<<
file { $(OUTD)\sc_inthd.obj $(MODOBJ) }
name $@
alias __sndcard_info=_HDA_sndcard_info
lib $(LIBS)
disable 1014
op q,statics,m=$(OUTD)\hda.map,nodefaultlibs
<<

$(OUTD)/SBGEN.DRV: $(OUTD)\sc_sbgen.obj $(OUTD)\sblive.obj $(OUTD)\sblive24.obj $(MODOBJ)
	@jwlink format dos @<<
file { $(OUTD)\sblive $(OUTD)\sblive24 $(OUTD)\sc_sbgen $(MODOBJ) }
name $@
alias __sndcard_info=_SBGEN_sndcard_info
lib $(LIBS)
disable 1014
op q,statics,m=$(OUTD)\sbgen.map,nodefaultlibs
<<

$(OUTD)/SBXFI.DRV: $(OUTD)\sc_sbxfi.obj $(OUTD)\cthw20k1.obj $(OUTD)\cthw20k2.obj $(MODOBJ)
	@jwlink format dos @<<
file { $(OUTD)\sc_sbxfi $(OUTD)\cthw20k1 $(OUTD)\cthw20k2 $(MODOBJ) }
name $@
alias __sndcard_info=_SBXFI_sndcard_info
lib $(LIBS)
disable 1014
op q,statics,m=$(OUTD)\sbxfi.map,nodefaultlibs
<<

$(OUTD)/VIA82.DRV: $(OUTD)\sc_via82.obj  $(MODOBJ)
	@jwlink format dos @<<
file { $(OUTD)\sc_via82.obj $(MODOBJ) }
name $@
alias __sndcard_info=_VIA82XX_sndcard_info
lib $(LIBS)
disable 1014
op q,statics,m=$(OUTD)\via82.map,nodefaultlibs
<<

$(OUTD)\sc_cs4281.obj: src\hw\sc_cs4281.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<
$(OUTD)\sc_e1370.obj: src\hw\sc_e1370.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<
$(OUTD)\sc_e1371.obj: src\hw\sc_e1371.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<
$(OUTD)\sc_ich.obj: src\hw\sc_ich.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<
$(OUTD)\sc_inthd.obj: src\hw\sc_inthd.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<
$(OUTD)\sc_sbgen.obj: src\hw\sc_sbgen.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<
$(OUTD)\sc_sbxfi.obj: src\hw\sc_sbxfi.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<
$(OUTD)\sc_via82.obj: src\hw\sc_via82.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<

$(OUTD)\sblive.obj: src\hw\sblive.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<
$(OUTD)\sblive24.obj: src\hw\sblive24.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<
$(OUTD)\cthw20k1.obj: src\hw\cthw20k1.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<
$(OUTD)\cthw20k2.obj: src\hw\cthw20k2.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<

$(OUTD)\dmabuff.obj: src\hw\dmabuff.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<
$(OUTD)\pcibios.obj: src\hw\pcibios.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<
$(OUTD)\timer.obj:   src\hw\timer.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<
$(OUTD)\physmem.obj: src\hw\physmem.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<
$(OUTD)\ac97mix.obj: src\hw\ac97mix.c
	@$(CC) $(C_DEBUG_FLAGS) $(C_FLAGS) $(C_EXTRA_FLAGS) $(INCLUDES) -fo=$@ $<

$(OUTD)\libmain32.obj: src\startup\libmain32.asm
	@jwasm -nologo -zcw $(A_DEBUG_FLAGS) -Fo $(OUTD)\ $<
$(OUTD)\malloc.obj:    src\startup\malloc.asm
	@jwasm -nologo -zcw $(A_DEBUG_FLAGS) -D?MODEL=tiny -Fo $(OUTD)\ $<
$(OUTD)\sbrk.obj:      src\startup\sbrk.asm
	@jwasm -nologo -zcw $(A_DEBUG_FLAGS) -D?MODEL=tiny -Fo $(OUTD)\ $<
$(OUTD)\dpmi.obj:      src\dpmi.asm
	@jwasm -nologo      $(A_DEBUG_FLAGS) -D?MODEL=tiny -Fo $(OUTD)\ $<
$(OUTD)\dprintf.obj:   src\dprintf.asm
	@jwasm -nologo      $(A_DEBUG_FLAGS) -D?MODEL=tiny -Fo $(OUTD)\ $<
$(OUTD)\vioout.obj:    src\vioout.asm
	@jwasm -nologo      $(A_DEBUG_FLAGS) -D?MODEL=tiny -Fo $(OUTD)\ $<


clean: .SYMBOLIC
	@del $(OUTD)\*.drv
	@del $(OUTD)\*.obj
	@del $(OUTD)\*.lst
	@del $(OUTD)\*.map
