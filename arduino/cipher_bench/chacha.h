/* ChaCha20 (RFC 7539 / 8439): 256-bit key, 96-bit nonce, 32-bit block counter.
 * chacha20_xor encrypts or decrypts (same operation) len bytes in place.
 */
#ifndef CHACHA_H
#define CHACHA_H
#include <stdint.h>

/* Generic C: rotations written as (x << n) | (x >> (32 - n)). */
void chacha20_xor(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter,
                  uint8_t *buf, uint16_t len);

/* Same cipher, rotations built from byte moves:
 * rotl16 = swap halves, rotl12 = rotr4(rotl16), rotl8 = byte shift, rotl7 = rotr1(rotl8). */
void chacha20_xor_fast(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter,
                       uint8_t *buf, uint16_t len);

#endif
