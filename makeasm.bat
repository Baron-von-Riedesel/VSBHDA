@echo off
jwasm -nologo -coff -Floutput\stackio.lst  -Fooutput\stackio.o  stackio.asm
jwasm -nologo -coff -Floutput\stackisr.lst -Fooutput\stackisr.o stackisr.asm
jwasm -nologo -coff -Floutput\int31.lst    -Fooutput\int31.o    int31.asm
jwasm -nologo -mz   -Floutput\setpvi.lst   -Fooutput\setpvi.exe setpvi.asm
