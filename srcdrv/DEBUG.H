#ifndef _DEBUG_H_
#define _DEBUG_H_

#ifdef _DEBUG
int _dprintf(const char *fmt, ... );
#define dbgprintf(x) _dprintf x
void dbgcondbrk(void *);
#else
#define dbgprintf(x)
#define dbgcondbrk(x)
#endif

#endif//_DEBUG_H_
