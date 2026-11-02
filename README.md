# VSBHDA
VSBHDA supplies Sound Blaster emulation for various sound hardware in DOS. It's a fork of crazii's [SBEMU](https://github.com/crazii/SBEMU).
Unlike SBEMU, VSBHDA works with unmodified HDPMI binaries, and hence is compatible with the [HX DOS Extender](https://github.com/Baron-von-Riedesel/HX).

## Supported Sound hardware ##
 * CS4281 chip by Cirrus Logic
 * ES1370 chip by Ensoniq ( AudioPCI )
 * Intel ICH, Nvidia nForce, SiS 7012
 * HDA ( Intel High Definition Audio )
 * SB based on ES1371/73 or CT5580 ( AudioPCI '97, Sound Blaster PCI 64/128 )
 * SB based on EMU10Kx ( SB Live, SB Audigy )
 * SB based on CA0106 ( Audigy LS/SE, Life 24, X-Fi Xtreme Audio [SB0790] )
 * SB based on EMU20Kx ( SB X-Fi, SB X-Fi Titanium ) 
 * VIA VT82C686, VT8233/35/37 [not VT8233A]

Fork VSBCMI (https://github.com/drivelling-spinel/VSBCMI/tree/main) also supports cards based on CMI 8338/8738.

## Emulated Hardware ##
 * Sound Blaster 1.0
 * Sound Blaster 2.0
 * Sound Blaster Pro
 * Sound Blaster Pro 2
 * Sound Blaster 16.

All output modes are supported: Adlib; DSP 8-Bit, 16-bit, mono, stereo, ADPCM, Direct Mode; Midi. 

## Requirements ##
 * [HDPMI32i](https://github.com/Baron-von-Riedesel/HX) - DPMI host with port trapping; 32-bit protected-mode
 * [HDPMI16i](https://github.com/Baron-von-Riedesel/HX) - DPMI host with port trapping; 16-bit protected-mode
 * [JEMM386/JEMMEX](https://github.com/Baron-von-Riedesel/Jemm) - V86 monitor with port trapping; v86-mode

VSBHDA runs in MS-DOS, FreeDOS and (E)DR-DOS.
 
## Creating the binaries ##
In case the binaries are to be created, a few makefiles are supplied:

 * Makefile/OW16.mak/Drivers.mak: for Windows/DOS using Open Watcom (v1.9 or v2.0) and JWasm/JWlink
 * Linux.mak/Linux16.mak/LinuxDrv.mak: for Linux using Open Watcom (v1.9 or v2.0) and JWasm/JWlink
 * djgpp.mak/djdrv.mak: for DOS using DJGPP v2.05 and JWasm/JWlink; cannot create the 16-bit variant of VSBHDA

The makefiles may need some (minor) adjustments, i.e. where to find Open Watcom.

**Source of tools used by the makefiles:**
 * [JWasm](https://github.com/Baron-von-Riedesel/JWasm); must be at least v2.17
 * [JWlink](https://github.com/Baron-von-Riedesel/jwlink); must be at least v2.0
 * [Open Watcom v2](https://github.com/open-watcom/open-watcom-v2/releases/tag/2024-03-01-Build)

**VSBHDA uses source code from:**
 * [MPXPlay](https://mpxplay.sourceforge.net/) - sound hardware access (mostly based on ALSA driver source)
 * [DOSBox](https://www.dosbox.com/) - OPL3 FM emulation & ADPCM decoding
 * [TinySoundFont](https://github.com/schellingb/TinySoundFont) - MIDI synthesizer emulation

## AI Policy ##
 With version 2.1, VSBHDA has been split in two parts: on one side the VSBHDA
 core binary (VSBHDA.EXE, VSBHDA16.EXE), on the other side external driver
 modules ( HDA.DRV, SBGEN.DRV, ... ). This separation is also reflected on the
 source code tree, with two branches ( src and srcdrv ). The AI policy differs
 for those two branches:

 * for the VSBHDA core, **no** LLM assisted/generated code is accepted.
 * for external driver modules, LLM assisted/generated code is acceptable if it complies with the licence of this project.

