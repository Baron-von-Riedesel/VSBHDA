 A few simple test programs

 TEST01: DSP cmd 0x14 (8-bit mono unsigned), zigzag
 TEST02: DSP cmd 0xB6 (16-bit mono unsigned autoinit), zigzag
 TEST03: DSP cmd 0xB6 (16-bit mono signed autoinit), sine
 TEST04: DSP cmd 0x14, uses EMS page frame as sound buffer
 TEST05: FM synthesizer test
 TEST06: MIDI synthesizer test
 TEST07: DSP cmd 0x91 (8-bit mono unsigned, highspeed), zigzag
 TEST08: DSP cmds 0x75/0x74 (4-bit ADPCM single-cycle with/without ref byte)
 TEST09: DSP cmd 0x7D (4-bit ADPCM autoinit)
 CVRATE: to test the resampling part of VSBHDA
 
 The assembly binaries can be created with JWasm, using its -mz option.
 
