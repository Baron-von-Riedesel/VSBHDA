#!/bin/bash
# Test VSBVXD.386 in Windows for Workgroups 3.11 enhanced mode (QEMU):
# copies the VxD (and test programs) to C:\VSB, adds it to SYSTEM.INI,
# starts WIN [program] and records the sound output and the VxD log.
#
#   test/win311.sh [DOS or Windows program [arguments]]
#
# Environment:
#   WIN311IMG  hard disk image with WfW 3.11 installed in C:\WINDOWS and
#              HIMEM.SYS in C:\ (required)
#   VXD        VxD to test (default build/vxd/VSBVXD.386)
#   FILES      more files to copy to C:\VSB (space separated)
#   DRV        wave driver to install (C:\WINDOWS\SYSTEM, [drivers] wave=)
#   INI        [VSBVXD] lines for SYSTEM.INI (| separated)
#   PRE        AUTOEXEC.BAT commands before Windows (| separated, > AUX)
#   CARD       QEMU sound device: hda (default), ac97, es1370, sb16, none
#   TIMEOUT    seconds until the last screenshot (default 90)
#   SHOTS      further screenshot times in seconds (space separated)
#   (a program can write its results to C:\VSB\OUT.TXT, shown at the end)
#   KEYS       QEMU sendkey commands (| separated) sent after TIMEOUT1 (60) s
#
# Output (build/test/): serial.log (AUX), vxd.log (VxD log, debug port E9h),
# sound.wav, screen*.png
set -uo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/build/test
BASE=${WIN311IMG:?set WIN311IMG to a WfW 3.11 hard disk image}
VXD=${VXD:-$ROOT/build/vxd/VSBVXD.386}
PROG=${1:-}; [ $# -gt 0 ] && shift

rm -rf "$OUT"; mkdir -p "$OUT/c/VSB"
cp "$VXD" "$OUT/c/VSB/VSBVXD.386"
[ -n "${FILES:-}" ] && cp ${FILES} "$OUT/c/VSB/"
[ -n "$PROG" ] && [ -f "$PROG" ] && { cp "$PROG" "$OUT/c/VSB/"; PROG='C:\VSB\'$(basename "$PROG"); }
printf 'org 100h\nmov dx,501h\nmov al,0\nout dx,al\nint 20h\n' > "$OUT/qexit.asm"
nasm -f bin -o "$OUT/c/QEXIT.COM" "$OUT/qexit.asm" || exit 1
cp "$BASE" "$OUT/test.img"

crlf() { tr '|' '\n' | sed 's/$/\r/'; }
echo 'DEVICE=C:\HIMEM.SYS|DOS=HIGH|FILES=40|BUFFERS=20|SHELL=C:\COMMAND.COM C:\ /P' | crlf > "$OUT/c/CONFIG.SYS"
{
    echo '@ECHO OFF'
    echo 'PATH C:\;C:\WINDOWS;C:\VSB'
    [ -n "${PRE:-}" ] && echo "$PRE" | tr '|' '\n' | sed 's/$/ > AUX/'
    echo 'ECHO --- starting Windows > AUX'
    echo "C:\\WINDOWS\\WIN $PROG $*"
    echo 'ECHO --- back from Windows > AUX'
    echo 'QEXIT'
} | crlf > "$OUT/c/AUTOEXEC.BAT"

P="$OUT/test.img@@32256"
mcopy -o -s -i "$P" "$OUT"/c/* ::/ || exit 1
mcopy -o -i "$P" ::/WINDOWS/SYSTEM.INI "$OUT/system.ini"
[ -n "${DRV:-}" ] && mcopy -o -i "$P" "$DRV" ::/WINDOWS/SYSTEM/
python3 - "$OUT/system.ini" "${INI:-}" "${DRV:+$(basename "${DRV:-}")}" <<'EOF'
import sys
f, ini, drv = sys.argv[1], sys.argv[2], sys.argv[3]
s = open(f, 'rb').read().decode('latin-1').replace('\r\n', '\n')
s = s.replace('[386Enh]\n', '[386Enh]\ndevice=C:\\VSB\\VSBVXD.386\n', 1)
if drv:
    s = s.replace('[drivers]\n', '[drivers]\nwave=' + drv.lower() + '\n', 1)
s += '\n[VSBVXD]\nLogPort=E9\n' + ''.join(l + '\n' for l in ini.split('|') if l)
open(f, 'wb').write(s.replace('\n', '\r\n').encode('latin-1'))
EOF
mcopy -o -i "$P" "$OUT/system.ini" ::/WINDOWS/SYSTEM.INI

case "${CARD:-hda}" in
hda)    SND="-device intel-hda -device hda-duplex,audiodev=snd0" ;;
hdaout) SND="-device intel-hda -device hda-output,audiodev=snd0" ;;
ac97)   SND="-device AC97,audiodev=snd0" ;;
es1370) SND="-device ES1370,audiodev=snd0" ;;
sb16)   SND="-device sb16,audiodev=snd0" ;;
none)   SND= ;;
esac

T=${TIMEOUT:-90}
{
    t=0
    for s in ${SHOTS:-}; do sleep $((s - t)); t=$s; echo "screendump $OUT/screen-$s.ppm"; done
    if [ -n "${KEYS:-}" ]; then
        sleep $(( ${TIMEOUT1:-60} - t )); t=${TIMEOUT1:-60}
        for k in $(echo "$KEYS" | tr '|' ' '); do echo "sendkey $k"; sleep 2; t=$((t + 2)); done
    fi
    sleep $((T - t > 0 ? T - t : 1)); echo "screendump $OUT/screen.ppm"; sleep 1; echo quit
} | qemu-system-i386 -machine pc ${ACCEL:--accel kvm} -m ${MEMSIZE:-32} -display none -vga std -monitor stdio \
    -drive if=ide,index=0,format=raw,file="$OUT/test.img" -boot c -nic none \
    -audiodev wav,id=snd0,path="$OUT/sound.wav" $SND \
    -serial file:"$OUT/serial.log" -debugcon file:"$OUT/vxd.log" \
    -device isa-debug-exit,iobase=0x501,iosize=0x02 ${QEMU_EXTRA:-} > "$OUT/monitor.log" 2>&1
for f in "$OUT"/*.ppm; do [ -f "$f" ] && magick "$f" "${f%.ppm}.png" && rm -f "$f"; done
tr -d '\r' < "$OUT/serial.log"
echo "=== VxD log"; cat "$OUT/vxd.log"
# output of test programs written to C:\VSB\OUT.TXT
mcopy -i "$P" ::/VSB/OUT.TXT "$OUT/out.txt" 2>/dev/null && { echo "=== C:\\VSB\\OUT.TXT"; tr -d '\r' < "$OUT/out.txt"; }
