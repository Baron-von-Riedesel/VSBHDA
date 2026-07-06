@echo off
\ow20\binnt\wcc386.exe -q -i\ow20\H cvrate.c
jwlink format os2 le f cvrate.obj libpath \ow20\lib386\dos libpath \ow20\lib386 op q,m,stub=cwstub.exe
