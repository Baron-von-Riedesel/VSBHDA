@echo off
jwasm -nologo -mz -Fl SBLSAVE.ASM
jwasm -nologo -mz -DMODE=1 -FlSBLLOAD.lst -FoSBLLOAD.EXE SBLSAVE.ASM
