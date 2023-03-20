# SBEMU
Sound blaster emulation for AC97/HDA/SBLive; a fork of crazii's SBEMU: https://github.com/crazii/SBEMU

Works with unmodified HDPMI32i, making it compatible with HX.

Supported Sound cards:
 * Intel ICH / nForce
 * Intel HDA
 * VIA VT82C686, VT8233
 * SB Live/SB Audigy

Emulated modes/cards:
8-bit, 16-bit, mono, stereo, high-speed;
Sound blaster 1.0, 2.0, Pro, Pro2, 16.

Requirements:
 * HDPMI32i - DPMI host with port trapping; protected-mode
 * JEMMEX 5.84 - V86 monitor with port trapping; v86-mode
 
SBEMU uses some source codes from:
 * MPXPlay: https://mpxplay.sourceforge.net/, for sound card drivers
 * DOSBox: https://www.dosbox.com/, for OPL3 FM emulation
