
# create vsbhda16.exe with Open Watcom and JWasm.
# to create the binary, enter
#   wmake -f ow16.mak
# optionally, for a debug version, enter
#   wmake -f ow16.mak debug=1

!ifndef DEBUG
DEBUG=0
!endif

WATCOM=\ow20
# use OW v2 (0) or OW v1.9 (1)
USE19=0

# activate next line if FM synth should be deactivated
#NOFM=1

CC=$(WATCOM)\binnt\wcc386
CPP=$(WATCOM)\binnt\wpp386
LINK=$(WATCOM)\binnt\wlink
#LINK=jwlink
LIB=$(WATCOM)\binnt\wlib
ASM=jwasm.exe

NAME=vsbhda16
NAME2=sndcard

!if $(DEBUG)
OUTD=ow16d
C_DEBUG_FLAGS=-D_DEBUG
A_DEBUG_FLAGS=-D_DEBUG -Fl=$*
!else
OUTD=ow16
C_DEBUG_FLAGS=-D_LOG
A_DEBUG_FLAGS=
!endif

!if $(USE19)
OW19=-DOW19
!endif

OBJFILES = &
	$(OUTD)/main.obj		$(OUTD)/sndisr.obj		$(OUTD)/ptrap.obj		$(OUTD)/linear.obj		$(OUTD)/pic.obj &
	$(OUTD)/vsb.obj			$(OUTD)/vdma.obj		$(OUTD)/virq.obj		$(OUTD)/vmpu.obj		$(OUTD)/tsf.obj &
!ifndef NOFM
	$(OUTD)/dbopl.obj		$(OUTD)/vopl3.obj &
!endif
	$(OUTD)/stackio.obj		$(OUTD)/stackisr.obj	$(OUTD)/sbisr.obj		$(OUTD)/int31.obj		$(OUTD)/rmwrap.obj		$(OUTD)/mixer.obj &
	$(OUTD)/hapi.obj		$(OUTD)/dprintf.obj		$(OUTD)/vioout.obj		$(OUTD)/djdpmi.obj		$(OUTD)/uninst.obj &
	$(OUTD)/auimp16.obj		$(OUTD)/ldmod16.obj		$(OUTD)/sbrk.obj		$(OUTD)/malloc.obj		$(OUTD)/rte200.obj &
	$(OUTD)/fileacc.obj

OBJFILES2 = &
	$(OUTD)/ac97mix.obj		$(OUTD)/au_cards.obj &
	$(OUTD)/dmabuff.obj		$(OUTD)/pcibios.obj		$(OUTD)/physmem.obj		$(OUTD)/timer.obj &
	$(OUTD)/sc_e1371.obj	$(OUTD)/sc_ich.obj		$(OUTD)/sc_inthd.obj	$(OUTD)/sc_via82.obj	$(OUTD)/sc_sbliv.obj	$(OUTD)/sc_sbl24.obj &
	$(OUTD)/djdpmi.obj		$(OUTD)/dprintf.obj		$(OUTD)/vioout.obj		$(OUTD)/sbrk.obj		$(OUTD)/malloc.obj &
	$(OUTD)/libmain.obj   

C_OPT_FLAGS=-q -oxa -ms -ecc -5s -fp5 -fpi87 -wcd=111
# OW's wpp386 doesn't like the -ecc option ("function modifier cannot be used ...")
CPP_OPT_FLAGS=-q -oxa -ms -bc -5s -fp5 -fpi87 
C_EXTRA_FLAGS=-DNOTFLAT
!ifdef NOFM
C_EXTRA_FLAGS= $(C_EXTRA_FLAGS) -DNOFM
!endif

INCLUDES=-I$(WATCOM)\h
LIBS=

{src}.asm{$(OUTD)}.obj
	@$(ASM) -q -DNOTFLAT -Istartup -D?MODEL=small $(A_DEBUG_FLAGS) -Fo$@ $<

{src}.c{$(OUTD)}.obj
	@$(CC) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) -os $(C_EXTRA_FLAGS) $(CFLAGS) -Isrc $(INCLUDES) -fo=$@ $<

{src}.cpp{$(OUTD)}.obj
	@$(CPP) $(C_DEBUG_FLAGS) $(CPP_OPT_FLAGS) -os $(C_EXTRA_FLAGS) $(CPPFLAGS) -Isrc $(INCLUDES) -fo=$@ $<

{mpxplay}.c{$(OUTD)}.obj
	@$(CC) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CFLAGS) -Impxplay -Isrc $(INCLUDES) -fo=$@ $<

{startup}.asm{$(OUTD)}.obj
	@$(ASM) -q -zcw -DNOTFLAT -D?MODEL=small $(OW19) $(A_DEBUG_FLAGS) -Fo$@ $<

{startup}.c{$(OUTD)}.obj
	@$(CC) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CFLAGS) $(INCLUDES) -fo=$@ $<

all: $(OUTD) $(OUTD)\$(NAME).exe $(OUTD)\$(NAME2).drv

$(OUTD):
	@mkdir $(OUTD)

$(OUTD)\$(NAME).exe: $(OUTD)\$(NAME).lib $(OUTD)\cstrt16x.obj $(OUTD)\init1632.obj
	@$(LINK) @<<
format dos 
file $(OUTD)\cstrt16x, $(OUTD)\main, $(OUTD)\init1632 name $@
libpath $(WATCOM)\lib386\dos;$(WATCOM)\lib386
lib $*.lib
op q,statics,m=$*.map
disable 80
<<

$(OUTD)\$(NAME2).drv: $(OUTD)\$(NAME2).lib $(OUTD)\dstrt16x.obj $(OUTD)\auexp16.obj
	@$(LINK) @<<
format dos 
file $(OUTD)\dstrt16x,$(OUTD)\auexp16.obj name $@
libpath $(WATCOM)\lib386\dos;$(WATCOM)\lib386
lib $*.lib
op q,statics,m=$*.map
disable 80
<<

$(OUTD)\$(NAME).lib: $(OBJFILES)
	@$(LIB) -q -b -n $(OUTD)\$(NAME).lib $(OBJFILES)

$(OUTD)\$(NAME2).lib: $(OBJFILES2)
	@$(LIB) -q -b -n $(OUTD)\$(NAME2).lib $(OBJFILES2)

$(OUTD)/ac97mix.obj:   mpxplay\ac97mix.c
$(OUTD)/au_cards.obj:  mpxplay\au_cards.c
$(OUTD)/dmabuff.obj:   mpxplay\dmabuff.c
$(OUTD)/physmem.obj:   mpxplay\physmem.c
$(OUTD)/pcibios.obj:   mpxplay\pcibios.c
$(OUTD)/sc_e1371.obj:  mpxplay\sc_e1371.c
$(OUTD)/sc_ich.obj:    mpxplay\sc_ich.c
$(OUTD)/sc_inthd.obj:  mpxplay\sc_inthd.c
$(OUTD)/sc_sbl24.obj:  mpxplay\sc_sbl24.c
$(OUTD)/sc_sbliv.obj:  mpxplay\sc_sbliv.c
$(OUTD)/sc_via82.obj:  mpxplay\sc_via82.c
$(OUTD)/timer.obj:     mpxplay\timer.c

$(OUTD)/auimp16.obj:   src\auimp16.asm
$(OUTD)/auexp16.obj:   src\auexp16.asm
$(OUTD)/djdpmi.obj:    src\djdpmi.asm
$(OUTD)/dprintf.obj:   src\dprintf.asm
$(OUTD)/fileacc.obj:   src\fileacc.asm
$(OUTD)/hapi.obj:      src\hapi.asm
$(OUTD)/int31.obj:     src\int31.asm
$(OUTD)/linear.obj:    src\linear.c
$(OUTD)/main.obj:      src\main.c
$(OUTD)/mixer.obj:     src\mixer.asm
$(OUTD)/pic.obj:       src\pic.c
$(OUTD)/ptrap.obj:     src\ptrap.c
$(OUTD)/rte200.obj:    src\rte200.asm
$(OUTD)/sbisr.obj:     src\sbisr.asm
$(OUTD)/sndisr.obj:    src\sndisr.c
$(OUTD)/stackio.obj:   src\stackio.asm
$(OUTD)/stackisr.obj:  src\stackisr.asm
$(OUTD)/tsf.obj:       src\tsf.c
$(OUTD)/uninst.obj:    src\uninst.asm
$(OUTD)/vdma.obj:      src\vdma.c
$(OUTD)/vioout.obj:    src\vioout.asm
$(OUTD)/virq.obj:      src\virq.c
$(OUTD)/vmpu.obj:      src\vmpu.c
$(OUTD)/vsb.obj:       src\vsb.c
!ifndef NOFM
$(OUTD)/dbopl.obj:     src\dbopl.cpp

$(OUTD)/vopl3.obj:     src\vopl3.cpp
	@$(CPP) $(C_DEBUG_FLAGS) -q -oxa -ms -bc -ecc -5s -fp5 -fpi87 $(C_EXTRA_FLAGS) $(CPPFLAGS) $(INCLUDES) -fo=$@ $<
!endif

$(OUTD)/cstrt16x.obj:  startup\cstrt16x.asm
$(OUTD)/dstrt16x.obj:  startup\dstrt16x.asm
$(OUTD)/ldmod16.obj:   startup\ldmod16.asm
$(OUTD)/init1632.obj:  startup\init1632.asm
$(OUTD)/malloc.obj:    startup\malloc.asm
$(OUTD)/sbrk.obj:      startup\sbrk.asm
$(OUTD)/libmain.obj:   startup\libmain.c

# the 16-bit code is included in binary format into rmwrap.asm.

$(OUTD)/rmwrap.obj:    src\rmwrap.asm src\rmcode1.asm src\rmcode2.asm
	@$(ASM) -q -bin -Fl$(OUTD)\ -Fo$(OUTD)\rmcode1.bin src\rmcode1.asm
	@$(ASM) -q -bin -Fl$(OUTD)\ -Fo$(OUTD)\rmcode2.bin src\rmcode2.asm
	@$(ASM) -q -DNOTFLAT -D?MODEL=small -Fl$(OUTD)\ -Fo$@ -DOUTD=$(OUTD) src\rmwrap.asm

clean: .SYMBOLIC
	@del $(OUTD)\$(NAME).lib
	@del $(OUTD)\$(NAME2).lib
	@del $(OUTD)\$(NAME).exe
	@del $(OUTD)\$(NAME2).drv
	@del $(OUTD)\*.obj
	@del $(OUTD)\*.map
	@del $(OUTD)\*.lst
	@del $(OUTD)\rmcode?.bin
