# VSBHDA
Sound blaster emulation for HDA (and AC97/SBLive); a fork of crazii's SBEMU: https://github.com/crazii/SBEMU

Works with unmodified HDPMI32i, making it compatible with HX.

Supported Sound cards:
 * HDA ( Intel High Definition Audio )
 * Intel ICH / nForce
 * VIA VT82C686, VT8233/35/37
 * SB Live/SB Audigy

Emulated modes/cards:
8-bit, 16-bit, mono, stereo, high-speed;
Sound blaster 1.0, 2.0, Pro, Pro2, 16.

Requirements:
 * HDPMI32i - DPMI host with port trapping; protected-mode
 * JEMMEX 5.84 - V86 monitor with port trapping; v86-mode
 
VSBHDA uses some source codes from:
 * MPXPlay: https://mpxplay.sourceforge.net/, for sound card access
 * DOSBox: https://www.dosbox.com/, for OPL3 FM emulation

To create the binary, one of 2 tool chains may be used:
 * DJGPP v2.05
 * Open Watcom 2.0

In both cases the JWasm assembler (v2.17 or better) is also needed.
For Open Watcom, a few things from the HX development package (HXDEV)
are required - see Watcom.mak for details.
