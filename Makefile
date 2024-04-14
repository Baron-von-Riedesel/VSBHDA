
# create vsbhda.exe with Open Watcom v2.0 and JWasm.
# to create the binary, enter
#   wmake
# optionally, for a debug version, enter
#   wmake debug=1

# the HX DOS extender is used; that means, a few
# things from the HXDEV package are required:
#
# - loadpero.bin     pe loader stub attached to binary
# - cstrtdhx.obj     startup module linked into binary
# - patchpe.exe      patches PE signature to PX
#
# patchpe is a Win32 application; to run it in DOS, the
# HXRT package will be needed; cstrtdx.obj should be copied
# to the Open Watcom lib386\dos directory; and loadpero.bin
# will be searched by the linker in the current directory
# or in any directory contained in the PATH environment var.

!ifndef DEBUG
DEBUG=0
!endif

WATCOM=\ow20
# activate next line if FM synth should be deactivated
#NOFM=1

# use jwlink (1) or wlink (0)
USEJWL=1

CC=$(WATCOM)\binnt\wcc386.exe
CPP=$(WATCOM)\binnt\wpp386.exe
!if $(USEJWL)
LINK=jwlink.exe
!else
LINK=$(WATCOM)\binnt\wlink.exe
!endif
LIB=$(WATCOM)\binnt\wlib.exe
ASM=jwasm.exe

NAME=vsbhda

!if $(DEBUG)
OUTD=owd
OUTD16=ow16d
C_DEBUG_FLAGS=-D_DEBUG
A_DEBUG_FLAGS=-D_DEBUG -Fl$*
!else
OUTD=ow
OUTD16=ow16
C_DEBUG_FLAGS=-D_LOG
A_DEBUG_FLAGS=
!endif

OBJFILES = &
	$(OUTD)/main.obj		$(OUTD)/sndisr.obj		$(OUTD)/ptrap.obj		$(OUTD)/linear.obj		$(OUTD)/pic.obj &
	$(OUTD)/vsb.obj			$(OUTD)/vdma.obj		$(OUTD)/virq.obj		$(OUTD)/vmpu.obj &
!ifndef NOFM
	$(OUTD)/dbopl.obj		$(OUTD)/vopl3.obj &
!endif
	$(OUTD)/ac97mix.obj		$(OUTD)/au_cards.obj &
	$(OUTD)/dmairq.obj		$(OUTD)/pcibios.obj		$(OUTD)/memory.obj		$(OUTD)/physmem.obj		$(OUTD)/timer.obj &
	$(OUTD)/sc_e1371.obj	$(OUTD)/sc_ich.obj		$(OUTD)/sc_inthd.obj	$(OUTD)/sc_via82.obj	$(OUTD)/sc_sbliv.obj	$(OUTD)/sc_sbl24.obj &
	$(OUTD)/stackio.obj		$(OUTD)/stackisr.obj	$(OUTD)/int31.obj		$(OUTD)/rmwrap.obj		$(OUTD)/mixer.obj &
	$(OUTD)/hapi.obj		$(OUTD)/dprintf.obj		$(OUTD)/vioout.obj		$(OUTD)/djdpmi.obj		$(OUTD)/uninst.obj &
	$(OUTD)/malloc.obj		$(OUTD)/sbrk.obj
	
C_OPT_FLAGS=-q -mf -oxa -ecc -5s -fp5 -fpi87 -wcd=111
# OW's wpp386 doesn't like the -ecc option
CPP_OPT_FLAGS=-q -oxa -mf -bc -5s -fp5 -fpi87 
C_EXTRA_FLAGS=
!ifdef NOFM
C_EXTRA_FLAGS= $(C_EXTRA_FLAGS) -DNOFM
!endif
LD_FLAGS=
LD_EXTRA_FLAGS=op M=$(OUTD)/$(NAME).map

INCLUDES=-I$(WATCOM)\h
LIBS=

{src}.asm{$(OUTD)}.obj
	@$(ASM) -q -D?MODEL=flat -Istartup $(A_DEBUG_FLAGS) -Fo$@ $<

{src}.c{$(OUTD)}.obj
	@$(CC) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CFLAGS) -Isrc $(INCLUDES) -fo=$@ $<

{src}.cpp{$(OUTD)}.obj
	@$(CPP) $(C_DEBUG_FLAGS) $(CPP_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CPPFLAGS) -Isrc $(INCLUDES) -fo=$@ $<

{mpxplay}.c{$(OUTD)}.obj
	@$(CC) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CFLAGS) -Impxplay -Isrc $(INCLUDES) -fo=$@ $<

{startup}.asm{$(OUTD)}.obj
	@$(ASM) -q -zcw -D?MODEL=flat $(A_DEBUG_FLAGS) -Fo$@ $<

all: $(OUTD) $(OUTD)\$(NAME).exe $(OUTD16)\$(NAME)16.exe

$(OUTD):
	@mkdir $(OUTD)

$(OUTD)\$(NAME).exe: $(OUTD)\$(NAME).lib
	@$(LINK) @<<
format win pe runtime console
file $(OUTD)\main.obj, $(OUTD)\linear.obj
name $@
libpath $(WATCOM)\lib386\dos;$(WATCOM)\lib386
libfile cstrtdhx.obj
lib $(OUTD)\$(NAME).lib
op q,m=$(OUTD)\$(NAME).map,stub=loadpero.bin,stack=0x10000,heap=0x1000
!if $(USEJWL)
segment CONST readonly
segment CONST2 readonly
!endif
<<
	@patchpe $*.exe

$(OUTD16)\$(NAME)16.exe: .always
	@wmake -h -f OW16.mak debug=$(DEBUG)

$(OUTD)\$(NAME).lib: $(OBJFILES)
	@$(LIB) -q -b -n $(OUTD)\$(NAME).lib $(OBJFILES)

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
$(OUTD)/timer.obj:     mpxplay\timer.c
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
$(OUTD)/vmpu.obj:      src\vmpu.c
$(OUTD)/vsb.obj:       src\vsb.c
!ifndef NOFM
$(OUTD)/dbopl.obj:     src\dbopl.cpp
$(OUTD)/vopl3.obj:     src\vopl3.cpp
	@$(CPP) $(C_DEBUG_FLAGS) -q -oxa -mf -bc -ecc -5s -fp5 -fpi87 $(C_EXTRA_FLAGS) $(CPPFLAGS) $(INCLUDES) -fo=$@ $<
!endif
$(OUTD)/malloc.obj:    startup\malloc.asm
$(OUTD)/sbrk.obj:      startup\sbrk.asm


# to avoid any issues with 16-bit relocations in PE binaries,
# the 16-bit code is included in binary format into rmwrap.asm.

$(OUTD)/rmwrap.obj:    src\rmwrap.asm src\rmcode.asm
	@$(ASM) -q -bin -Fl$(OUTD)\ -Fo$(OUTD)\rmcode.bin src\rmcode.asm
	@$(ASM) -q -D?MODEL=flat -Fo$@ -DOUTD=$(OUTD) src\rmwrap.asm

clean: .SYMBOLIC
	@wmake -h -f OW16.mak debug=$(DEBUG) clean
	@del $(OUTD)\$(NAME).exe
	@del $(OUTD)\$(NAME).lib
	@del $(OUTD)\*.obj
