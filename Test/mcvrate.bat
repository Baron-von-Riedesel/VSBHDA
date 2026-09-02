@echo off

rem Win32
rem \ow20\binnt\wcc386.exe -q -s -oxa -hc -i\ow20\H cvrate.c
rem jwlink format win nt f cvrate.obj libpath \ow20\lib386\nt;\ow20\lib386 lib kernel32.lib,user32.lib op q,m

rem Win32 debug
\ow20\binnt\wcc386.exe -q -s -od -d3 -hc -D_DEBUG -i\ow20\H cvrate.c
jwlink debug c op cvp format win nt f cvrate.obj libpath \ow20\lib386\nt;\ow20\lib386 lib kernel32.lib,user32.lib op q,m

rem CauseWay
rem \ow20\binnt\wcc386.exe -q -s -oxa -hc -i\ow20\H cvrate.c
rem jwlink format os2 le f cvrate.obj libpath \ow20\lib386\dos;\ow20\lib386 op q,m,stub=cwstub.exe
