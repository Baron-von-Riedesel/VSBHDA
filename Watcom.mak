
# create vsbhda.exe with Open Watcom v2.0 and JWasm.
# to create the binary, enter
#   wmake -f watcom.mak
# optionally, for a debug version, enter
#   wmake -f watcom.mak DEBUG=1

# the HX DOS extender is used; that means, a few
# things from the HXDEV package are required:
#
# - loadpe.bin       pe loader stub attached to binary
# - cstrtdhx.obj     startup module linked into binary
# - patchpe.exe      patches PE signature to PX
#
# patchpe is a Win32 application; to run it in DOS, the
# HXRT package will be needed; cstrtdx.obj should be copied
# to the Open Watcom lib386\dos directory; and loadpe.bin
# will be searched by the linker in the current directory
# or in any directory contained in the PATH environment var.

!ifndef DEBUG
DEBUG=0
!endif

WATCOM=\ow20
# activate next line if FM synth should be deactivated
#NOFM=1

CC=$(WATCOM)\binnt\wcc386
CPP=$(WATCOM)\binnt\wpp386
LINK=$(WATCOM)\binnt\wlink

NAME=vsbhda

!if $(DEBUG)
OUTD=owd
C_DEBUG_FLAGS=-D_DEBUG
A_DEBUG_FLAGS=-D_DEBUG
!else
OUTD=ow
C_DEBUG_FLAGS=
A_DEBUG_FLAGS=
!endif

OBJFILES = &
	$(OUTD)/main.obj		$(OUTD)/sndisr.obj		$(OUTD)/ptrap.obj		$(OUTD)/linear.obj		$(OUTD)/pic.obj &
	$(OUTD)/vsb.obj			$(OUTD)/vdma.obj		$(OUTD)/virq.obj &
!ifndef NOFM
	$(OUTD)/dbopl.obj		$(OUTD)/vopl3.obj &
!endif
	$(OUTD)/ac97mix.obj		$(OUTD)/au_cards.obj &
	$(OUTD)/dmairq.obj		$(OUTD)/pcibios.obj		$(OUTD)/memory.obj		$(OUTD)/physmem.obj		$(OUTD)/time.obj &
	$(OUTD)/sc_e1371.obj	$(OUTD)/sc_ich.obj		$(OUTD)/sc_inthd.obj	$(OUTD)/sc_via82.obj	$(OUTD)/sc_sbliv.obj	$(OUTD)/sc_sbl24.obj &
	$(OUTD)/stackio.obj		$(OUTD)/stackisr.obj	$(OUTD)/int31.obj		$(OUTD)/rmwrap.obj		$(OUTD)/mixer.obj &
	$(OUTD)/hapi.obj		$(OUTD)/dprintf.obj		$(OUTD)/vioout.obj		$(OUTD)/djdpmi.obj		$(OUTD)/uninst.obj

C_OPT_FLAGS=-q -oxa -ecc -5s -fp5 -fpi87 -wcd=111
# OW's wpp386 doesn't like the -ecc option
CPP_OPT_FLAGS=-q -mf -bc -5s -fp5 -fpi87 
C_EXTRA_FLAGS= -DSBEMU
!ifdef NOFM
C_EXTRA_FLAGS= $(C_EXTRA_FLAGS) -DNOFM
!endif
LD_FLAGS=
LD_EXTRA_FLAGS=op M=$(OUTD)/$(NAME).map

INCLUDES=-Isrc -Impxplay -I$(WATCOM)\h
LIBS=

{src}.asm{$(OUTD)}.obj
	jwasm.exe -q -D?FLAT $(A_DEBUG_FLAGS) -Fo$@ $<

{src}.c{$(OUTD)}.obj
	$(CC) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CFLAGS) $(INCLUDES) -fo=$@ $<

{src}.cpp{$(OUTD)}.obj
	$(CPP) $(C_DEBUG_FLAGS) $(CPP_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CPPFLAGS) $(INCLUDES) -fo=$@ $<

{mpxplay}.c{$(OUTD)}.obj
	$(CC) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CFLAGS) $(INCLUDES) -fo=$@ $<

all: $(OUTD) $(OUTD)\$(NAME).exe

$(OUTD):
	@mkdir $(OUTD)

$(OUTD)\$(NAME).exe: $(OUTD)\$(NAME).lib
	@$(LINK) @<<
format win pe runtime console
file $(OUTD)\main.obj name $@
libpath $(WATCOM)\lib386\dos;$(WATCOM)\lib386
libfile cstrtdhx.obj
lib $(OUTD)\$(NAME).lib
op q,m=$(OUTD)\$(NAME).map,stub=loadpe.bin,stack=0x10000,heap=0x1000
<<
	@patchpe $*.exe

$(OUTD)\$(NAME).lib: $(OBJFILES)
	@jwlib -q -b -n $(OUTD)\$(NAME).lib $(OBJFILES)

$(OUTD)/ac97mix.obj:   mpxplay\ac97mix.c
$(OUTD)/au_cards.obj:  mpxplay\au_cards.c
$(OUTD)/dmairq.obj:    mpxplay\dmairq.c
$(OUTD)/physmem.obj:   mpxplay\physmem.c
$(OUTD)/memory.obj:    mpxplay\memory.c
$(OUTD)/pcibios.obj:   mpxplay\pcibios.c
$(OUTD)/sc_e1371.obj:  mpxplay\sc_e1371.c
$(OUTD)/sc_ich.obj:    mpxplay\sc_ich.c
$(OUTD)/sc_inthd.obj:  mpxplay\sc_inthd.c
$(OUTD)/sc_sbl24.obj:  mpxplay\sc_sbl24.c
$(OUTD)/sc_sbliv.obj:  mpxplay\sc_sbliv.c
$(OUTD)/sc_via82.obj:  mpxplay\sc_via82.c
$(OUTD)/time.obj:      mpxplay\time.c
$(OUTD)/djdpmi.obj:    src\djdpmi.asm
$(OUTD)/dprintf.obj:   src\dprintf.asm
$(OUTD)/hapi.obj:      src\hapi.asm
$(OUTD)/int31.obj:     src\int31.asm
$(OUTD)/linear.obj:    src\linear.c
$(OUTD)/main.obj:      src\main.c
$(OUTD)/mixer.obj:     src\mixer.asm
$(OUTD)/pic.obj:       src\pic.c
$(OUTD)/ptrap.obj:     src\ptrap.c
$(OUTD)/sndisr.obj:    src\sndisr.c
$(OUTD)/stackio.obj:   src\stackio.asm
$(OUTD)/stackisr.obj:  src\stackisr.asm
$(OUTD)/uninst.obj:    src\uninst.asm
$(OUTD)/vdma.obj:      src\vdma.c
$(OUTD)/vioout.obj:    src\vioout.asm
$(OUTD)/virq.obj:      src\virq.c
$(OUTD)/vsb.obj:       src\vsb.c
!ifndef NOFM
$(OUTD)/dbopl.obj:     src\dbopl.cpp

$(OUTD)/vopl3.obj:     src\vopl3.cpp
	$(CPP) $(C_DEBUG_FLAGS) -q -mf -bc -ecc -5s -fp5 -fpi87 $(C_EXTRA_FLAGS) $(CPPFLAGS) $(INCLUDES) -fo=$@ $<
!endif

# to avoid any issues with 16-bit relocations in PE binaries,
# the 16-bit code is included in binary format into rmwrap.asm.

$(OUTD)/rmwrap.obj:    src\rmwrap.asm src\rmcode.asm
	jwasm.exe -q -bin -Fl$(OUTD)\ -Fo$(OUTD)\rmcode.bin src\rmcode.asm
	jwasm.exe -q -Fo$@ -DOUTD=$(OUTD) src\rmwrap.asm

clean: .SYMBOLIC
	@del $(OUTD)\$(NAME).exe
	@del $(OUTD)\$(NAME).lib
	@del $(OUTD)\*.obj
