# VSBWAVE.DRV - Windows 3.1 wave output driver for VSBVXD.386 (GNU make,
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

all: $(OUT)/VSBWAVE.DRV

$(OUT):
	mkdir -p $@

$(OUT)/%.obj: %.C | $(OUT)
	$(CC) $(COPT) -fo=$@ $<

$(OUT)/%.obj: %.ASM | $(OUT)
	$(ASM) -q -Fo$@ $<

$(OUT)/VSBWAVE.DRV: $(OUT)/VSBWAVE.obj $(OUT)/VXDCALL.obj VSBWAVE.LNK
	$(LINK) option quiet name $@ option map=$(OUT)/VSBWAVE.map \
	  file $(OUT)/VSBWAVE.obj file $(OUT)/VXDCALL.obj @VSBWAVE.LNK

clean:
	rm -rf $(OUT)

.PHONY: all clean
