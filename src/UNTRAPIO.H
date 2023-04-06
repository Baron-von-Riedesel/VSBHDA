#ifndef _UNTRAPIO_H_
#define _UNTRAPIO_H_
//Untrapped IO wrapper function

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void (*UntrappedIO_OUT_Handler)(uint16_t port, uint8_t value);
uint8_t (*UntrappedIO_IN_Handler)(uint16_t port);

#define UntrappedIO_OUT( a, b ) UntrappedIO_OUT_Handler( a, b )
#define UntrappedIO_IN( a ) UntrappedIO_IN_Handler( a )

#ifdef __cplusplus
}
#endif

#endif//_UNTRAPIO_H_