# VSBHDA.DRV - Windows 3.1 multimedia driver for VSBVXD.386 (wave, MIDI, aux) (GNU make,
# Linux, Open Watcom).
#
#   make -C drv [WATCOM=...] [ASM=jwasm]

WATCOM ?= /opt/watcom
ASM    ?= jwasm
export WATCOM

CC   := $(WATCOM)/binl/wcc
LINK := $(WATCOM)/binl/wlink
OUT  ?= ../build/drv

# 16-bit Windows DLL, large model, SS != DS, no stack checks (the timer
# callback runs at interrupt time)
COPT := -q -bt=windows -bd -ml -zu -3 -s -os -zq -I$(WATCOM)/h -I$(WATCOM)/h/win

all: $(OUT)/VSBHDA.DRV

$(OUT):
	mkdir -p $@

$(OUT)/%.obj: %.C VSBDRV.H | $(OUT)
	$(CC) $(COPT) -fo=$@ $<

$(OUT)/%.obj: %.ASM | $(OUT)
	$(ASM) -q -Fo$@ $<

OBJS := $(addprefix $(OUT)/,VSBDRV.obj WAVE.obj MIDI.obj MIDIMAP.obj AUX.obj GENMIDI.obj VXDCALL.obj)

$(OUT)/VSBHDA.DRV: $(OBJS) VSBHDA.LNK
	$(LINK) option quiet name $@ option map=$(OUT)/VSBHDA.map \
	  $(foreach o,$(OBJS),file $(o)) @VSBHDA.LNK

clean:
	rm -rf $(OUT)

.PHONY: all clean
