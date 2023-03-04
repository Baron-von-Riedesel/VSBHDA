@echo off
jwasm -nologo -djgpp -Flbuild\stackio.lst  -Fobuild\stackio.o    sbemu\stackio.asm
jwasm -nologo -djgpp -Flbuild\stackisr.lst -Fobuild\stackisr.o   sbemu\stackisr.asm
jwasm -nologo -djgpp -Flbuild\int31.lst    -Fobuild\int31.o      sbemu\int31.asm
jwasm -nologo -djgpp -Flbuild\dprintf.lst  -Fobuild\dprintf.o    sbemu\dprintf.asm
jwasm -nologo -djgpp -Flbuild\vioout.lst   -Fobuild\vioout.o     sbemu\vioout.asm
