# PLAYWAV.EXE - Windows 3.1 test program for VSBWAVE.DRV (Open Watcom)
WATCOM ?= /opt/watcom
export WATCOM
OUT ?= ../../build/test-win

all: $(OUT)/PLAYWAV.EXE

$(OUT):
	mkdir -p $@

$(OUT)/PLAYWAV.obj: PLAYWAV.C | $(OUT)
	$(WATCOM)/binl/wcc -q -bt=windows -ml -zw -2 -I$(WATCOM)/h -I$(WATCOM)/h/win -fo=$@ $<

$(OUT)/PLAYWAV.EXE: $(OUT)/PLAYWAV.obj
	$(WATCOM)/binl/wlink option quiet format windows name $@ file $< \
	  libpath $(WATCOM)/lib286/win libpath $(WATCOM)/lib286 library windows library mmsystem \
	  option nocaseexact option stack=8k option heapsize=4k option version=3.10

clean:
	rm -rf $(OUT)
