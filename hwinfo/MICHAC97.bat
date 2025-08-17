@echo off
if not exist ..\ow\NUL mkdir ..\ow
jwasm -mz -Fo..\ow\ ICHAC97.ASM
