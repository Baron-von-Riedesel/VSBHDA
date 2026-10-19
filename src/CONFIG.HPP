
// config.h variant for C++
// due to the OW problem of wpp386 with option -ecc

extern "C" {

int _dprintf(const char *fmt, ... );
#ifdef _DEBUG
#define dbgprintf(x) _dprintf x
#else
#define dbgprintf(x)
#endif
}

