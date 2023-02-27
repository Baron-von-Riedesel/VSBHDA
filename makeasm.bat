@echo off
jwasm -nologo -djgpp -Floutput\stackio.lst  -Fooutput\stackio.o    stackio.asm
jwasm -nologo -djgpp -Floutput\stackisr.lst -Fooutput\stackisr.o   stackisr.asm
jwasm -nologo -djgpp -Floutput\int31.lst    -Fooutput\int31.o      int31.asm
jwasm -nologo -djgpp -Floutput\dprintf.lst  -Fooutput\dprintf.o    dprintf.asm
jwasm -nologo -djgpp -Floutput\vioout.lst   -Fooutput\vioout.o     vioout.asm
