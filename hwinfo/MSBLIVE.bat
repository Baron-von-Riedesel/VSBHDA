@echo off
if not exist ..\ow\NUL mkdir ..\ow
jwasm -mz -Fo..\ow\ SBLIVE.ASM
