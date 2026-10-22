/* Host (x86) stand-in for <avr/pgmspace.h>: on a PC there is no separate Flash address space. */
#ifndef HOST_AVR_PGMSPACE_H
#define HOST_AVR_PGMSPACE_H
#include <stdint.h>
#include <string.h>

#define PROGMEM
#define pgm_read_byte(addr)   (*(const uint8_t *)(addr))
#define memcpy_P(d, s, n)     memcpy((d), (s), (n))

#endif
