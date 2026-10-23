//-------------------------------------------------------------------------

#ifndef _EFXMGR_H
#define _EFXMGR_H

#define VOL_8BIT 0x100
#define VOL_7BIT 0x80
#define VOL_6BIT 0x40
#define VOL_5BIT 0x20
#define VOL_4BIT 0x10

#define A_WRITE_EFX(a, b, c) emu10k1_writeptr((a), A_MICROCODEBASE + (b), 0, (c))

#define A_OP(op, z, w, x, y) \
{ \
 A_WRITE_EFX(card, (pc) * 2, ((x) << 12) | (y)); \
 A_WRITE_EFX(card, (pc) * 2 + 1, ((op) << 24 ) | ((z) << 12) | (w)); \
 ++pc; \
}

#define L_WRITE_EFX(a, b, c) emu10k1_writeptr((a), MICROCODEBASE + (b), 0, (c))

#define L_OP(op, z, w, x, y) \
{ \
 L_WRITE_EFX(card, (pc) * 2, ((x) << 10) | (y)); \
 L_WRITE_EFX(card, (pc) * 2 + 1, ((op) << 20) | ((z) << 10) | (w)); \
 ++pc; \
}

#endif // _EFXMGR.H
