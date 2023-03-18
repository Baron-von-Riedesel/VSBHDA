#include <dos.h>
#include "UNTRAPIO.H"

void (*UntrappedIO_OUT_Handler)(uint16_t port, uint8_t value) = &outp;
uint8_t (*UntrappedIO_IN_Handler)(uint16_t port) = &inp;
