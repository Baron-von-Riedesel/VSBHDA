// CPPRT.CPP: C++ runtime pieces for VSBVXD (DBOPL/VOPL3): new/delete on
// the VxD heap (PLATVXD.C)
#include <stdlib.h>

void *operator new( size_t n ) { return malloc( n ); }
void *operator new []( size_t n ) { return malloc( n ); }
void operator delete( void *p ) { free( p ); }
void operator delete []( void *p ) { free( p ); }
