#!/usr/bin/env python3
"""lefix.py VXD: mark all 32-bit objects of an LE VxD executable.

WIN386 of Windows 3.1 silently skips a VxD with an object that isn't
executable (Microsoft's DDK puts locked data into class 'CODE', so LINK386
never produced one); the C data segments (class DATA/BSS) give such an
object."""
import struct
import sys

OBJ_EXECUTABLE = 0x0004
OBJ_BIG = 0x2000

f = sys.argv[1]
d = bytearray(open(f, 'rb').read())
le = struct.unpack_from('<I', d, 0x3c)[0]
objtab = le + struct.unpack_from('<I', d, le + 0x40)[0]
for i in range(struct.unpack_from('<I', d, le + 0x44)[0]):
    o = objtab + 24 * i + 8
    flags = struct.unpack_from('<I', d, o)[0]
    if flags & OBJ_BIG:
        struct.pack_into('<I', d, o, flags | OBJ_EXECUTABLE)
open(f, 'wb').write(d)
