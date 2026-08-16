# VSBHDA
Sound blaster emulation for HDA (and AC97/SBLive); a fork of crazii's SBEMU: https://github.com/crazii/SBEMU

Works with unmodified HDPMI binaries, making it compatible with HX.

Supported Sound cards:
 * HDA ( Intel High Definition Audio )
 * Intel ICH, Nvidia nForce, SiS 7012
 * VIA VT82C686, VT8233/35/37 (not VT8233A)
 * SB Live, SB Audigy
 * SB based on ES1371/1373 (Ensoniq)

Fork VSBCMI (https://github.com/drivelling-spinel/VSBCMI/tree/main) also supports cards based on CMI 8338/8738.

Emulated modes/cards:
8-bit, 16-bit, mono, stereo, high-speed;
Sound Blaster 1.0, 2.0, Pro, Pro 2, 16.

Requirements:
 * HDPMI32i - DPMI host with port trapping; 32-bit protected-mode
 * HDPMI16i - DPMI host with port trapping; 16-bit protected-mode
 * JEMMEX 5.84 - V86 monitor with port trapping; v86-mode
 
VSBHDA uses some source codes from:
 * MPXPlay: https://mpxplay.sourceforge.net/ - sound card access
 * DOSBox: https://www.dosbox.com/ - OPL3 FM emulation
 * TinySoundFont: https://github.com/schellingb/TinySoundFont - MIDI synthesizer emulation

A few makefiles are supplied to create the binaries:
 * Makefile/OW16.mak: for Windows/DOS using Open Watcom (v1.9 or v2.0) and JWasm/JWlink
 * Linux.mak/Linux16.mak: for Linux using Open Watcom (v1.9 or v2.0) and JWasm/JWlink
 * djgpp.mak: for DOS using DJGPP v2.05 and JWasm; cannot create the 16-bit variant of VSBHDA

JWasm (https://github.com/Baron-von-Riedesel/JWasm) must be at least v2.17,
JWlink (https://github.com/Baron-von-Riedesel/jwlink) must be at least v2.0.
