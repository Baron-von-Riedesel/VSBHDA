
# Create vsbhda16.exe with Open Watcom and JWasm.
# Enter
#   wmake -f ow16.mak
# Optionally, for a debug version, enter
#   wmake -f ow16.mak debug=1
#
# The build process is supposed to be run in either Windows or DOS.
# For DOS, however, it's necessary to install the HX runtime package
# and setup HX for full Win32 emulation:
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

!ifndef USEJWL
USEJWL=1
!endif

DYNAMIC=1

# activate next line if FM synth should be deactivated
#NOFM=1

NAME=VSBHDA16

CC=$(WATCOM)\binnt\wcc386.exe
CPP=$(WATCOM)\binnt\wpp386.exe
!if $(USEJWL)
LINK=jwlink.exe
!else
LINK=$(WATCOM)\binnt\wlink.exe
!endif
LIB=$(WATCOM)\binnt\wlib.exe
ASM=jwasm.exe

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

LOADDRVOBJ=$(OUTD)/loaddrv.obj

OBJFILES = &
	$(OUTD)/main.obj		$(OUTD)/sndisr.obj		$(OUTD)/ptrap.obj		$(OUTD)/linear.obj		$(OUTD)/pic.obj &
	$(OUTD)/vsb.obj			$(OUTD)/vdma.obj		$(OUTD)/virq.obj		$(OUTD)/vmpu.obj		$(OUTD)/tsf.obj &
	$(OUTD)/adpcm.obj &
!ifndef NOFM
	$(OUTD)/dbopl.obj		$(OUTD)/vopl3.obj &
!endif
	$(OUTD)/stackio.obj		$(OUTD)/stackisr.obj	$(OUTD)/sbisr.obj		$(OUTD)/int31.obj		$(OUTD)/rmwrap.obj		$(OUTD)/mixer.obj &
	$(OUTD)/hapi.obj		$(OUTD)/dprintf.obj		$(OUTD)/vioout.obj		$(OUTD)/dpmi.obj		$(OUTD)/dpmiisr.obj		$(OUTD)/uninst.obj &
	$(OUTD)/ldmod16.obj		$(OUTD)/sbrk.obj		$(OUTD)/malloc.obj		$(OUTD)/rte200.obj &
	$(OUTD)/au_cards.obj &
	$(LOADDRVOBJ) &
	$(OUTD)/fileacc.obj		$(OUTD)/logfile.obj		$(OUTD)/getenv.obj		$(OUTD)/strtol.obj		$(OUTD)/_matherr.obj

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
	@$(ASM) -q -DNOTFLAT -Isrc\startup -D?MODEL=small $(A_DEBUG_FLAGS) -Fo$@ $<

{src}.c{$(OUTD)}.obj
	@$(CC) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) -os $(C_EXTRA_FLAGS) $(CFLAGS) -Isrc $(INCLUDES) -fo=$@ $<

{src}.cpp{$(OUTD)}.obj
	@$(CPP) $(C_DEBUG_FLAGS) $(CPP_OPT_FLAGS) -os $(C_EXTRA_FLAGS) $(CPPFLAGS) -Isrc $(INCLUDES) -fo=$@ $<

{src\hw}.c{$(OUTD)}.obj
	@$(CC) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CFLAGS) -DDYNAMIC -Isrc\hw -Isrc $(INCLUDES) -fo=$@ $<

{src\startup}.asm{$(OUTD)}.obj
	@$(ASM) -q -zcw -DNOTFLAT -D?MODEL=small $(OW19) $(A_DEBUG_FLAGS) -Fo$@ $<

{src\startup}.c{$(OUTD)}.obj
	@$(CC) $(C_DEBUG_FLAGS) $(C_OPT_FLAGS) $(C_EXTRA_FLAGS) $(CFLAGS) $(INCLUDES) -fo=$@ $<

all: $(OUTD) $(OUTD)\$(NAME).EXE

$(OUTD):
	@mkdir $(OUTD)

$(OUTD)\$(NAME).EXE: $(OUTD)\$(NAME).lib $(OUTD)\cstrt16x.obj $(OUTD)\init1632.obj
	@$(LINK) @<<
format dos 
file $(OUTD)\init1632.obj, $(OUTD)\cstrt16x.obj, $(OUTD)\main.obj name $*.EXE
libpath $(WATCOM)\lib386\dos;$(WATCOM)\lib386
lib $*.lib
op q,statics,m=$*.map
disable 80
<<

$(OUTD)\$(NAME).lib: $(OBJFILES)
	@$(LIB) -q -b -n $(OUTD)\$(NAME).lib $(OBJFILES)

$(OUTD)/au_cards.obj:  src\hw\au_cards.c

$(OUTD)/adpcm.obj:     src\adpcm.c
$(OUTD)/auimp16.obj:   src\auimp16.asm
$(OUTD)/auexp16.obj:   src\auexp16.asm
$(OUTD)/dpmi.obj:      src\dpmi.asm
$(OUTD)/dpmiisr.obj:   src\dpmiisr.asm
$(OUTD)/dprintf.obj:   src\dprintf.asm
$(OUTD)/fileacc.obj:   src\fileacc.asm
$(OUTD)/hapi.obj:      src\hapi.asm
$(OUTD)/int31.obj:     src\int31.asm
$(OUTD)/linear.obj:    src\linear.c
$(OUTD)/logfile.obj:   src\logfile.asm
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

$(OUTD)/cstrt16x.obj:  src\startup\cstrt16x.asm
$(OUTD)/dstrt16x.obj:  src\startup\dstrt16x.asm
$(OUTD)/ldmod16.obj:   src\startup\ldmod16.asm
$(OUTD)/init1632.obj:  src\startup\init1632.asm
$(OUTD)/getenv.obj:    src\startup\getenv.asm
$(OUTD)/malloc.obj:    src\startup\malloc.asm
$(OUTD)/sbrk.obj:      src\startup\sbrk.asm
$(OUTD)/strtol.obj:    src\startup\strtol.asm
$(OUTD)/_matherr.obj:  src\startup\_matherr.asm
$(OUTD)/libmain.obj:   src\startup\libmain.c

# the 16-bit code is included in binary format into rmwrap.asm.

$(OUTD)/rmwrap.obj:    src\rmwrap.asm src\rmcode1.asm src\rmcode2.asm
	@$(ASM) -q -bin -Fl$(OUTD)\ -Fo$(OUTD)\rmcode1.bin src\rmcode1.asm
	@$(ASM) -q -bin -Fl$(OUTD)\ -Fo$(OUTD)\rmcode2.bin src\rmcode2.asm
	@$(ASM) -q -DNOTFLAT -D?MODEL=small $(OW19) -Fl$(OUTD)\ -Fo$@ -DOUTD=$(OUTD) src\rmwrap.asm

clean: .SYMBOLIC
	@del $(OUTD)\$(NAME).lib
	@del $(OUTD)\$(NAME).EXE
	@del $(OUTD)\*.obj
	@del $(OUTD)\*.map
	@del $(OUTD)\*.lst
	@del $(OUTD)\rmcode?.bin
