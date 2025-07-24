# VSBHDA
Sound blaster emulation for HDA (and AC97/SBLive); a fork of crazii's SBEMU: https://github.com/crazii/SBEMU

Works with unmodified HDPMI binaries, making it compatible with HX.

Supported Sound cards:
 * HDA ( Intel High Definition Audio )
 * Intel ICH / nForce
 * VIA VT82C686, VT8233/35/37
 * SB Live/SB Audigy

Emulated modes/cards:
8-bit, 16-bit, mono, stereo, high-speed;
Sound blaster 1.0, 2.0, Pro, Pro2, 16.

Requirements:
 * HDPMI32i - DPMI host with port trapping; 32-bit protected-mode
 * HDPMI16i - DPMI host with port trapping; 16-bit protected-mode
 * JEMMEX 5.84 - V86 monitor with port trapping; v86-mode
 
VSBHDA uses some source codes from:
 * MPXPlay: https://mpxplay.sourceforge.net/ - sound card access
 * DOSBox: https://www.dosbox.com/ - OPL3 FM emulation
 * TinySoundFont: https://github.com/schellingb/TinySoundFont - MIDI synthesizer emulation

To create the binaries, Open Watcom v2.0 is recommended. DJGPP v2.05
may also be used, but cannot create the 16-bit variant of VSBHDA.

In all cases the JWasm assembler (v2.17 or better) is also needed.
For Open Watcom, a few things from the HX development package (HXDEV)
are required - see Makefile for details.
