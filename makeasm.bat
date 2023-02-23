@echo off
jwasm -nologo -djgpp -Floutput\stackio.lst  -Fooutput\stackio.o    stackio.asm
jwasm -nologo -djgpp -Floutput\stackisr.lst -Fooutput\stackisr.o   stackisr.asm
jwasm -nologo -djgpp -Floutput\int31.lst    -Fooutput\int31.o      int31.asm
jwasm -nologo -djgpp -Floutput\dprintf.lst  -Fooutput\dprintf.o    dprintf.asm
jwasm -nologo -djgpp -Floutput\output.lst   -Fooutput\output.o     output.asm
jwasm -nologo -mz    -Floutput\setpvi.lst   -Fooutput\setpvi.exe   setpvi.asm
jwasm -nologo -mz    -Floutput\resetpvi.lst -Fooutput\resetpvi.exe resetpvi.asm
