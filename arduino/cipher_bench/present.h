/* PRESENT-80: 64-bit block, 80-bit key, 31 rounds.
 * All values are little-endian byte arrays: byte 0 holds the least significant
 * 8 bits (so 0x5579C1387B228445 is stored as 45 84 22 7B 38 C1 79 55).
 */
#ifndef PRESENT_H
#define PRESENT_H
#include <stdint.h>

#define PRESENT_ROUNDS     31
#define PRESENT_RK_BYTES   (8 * (PRESENT_ROUNDS + 1))   /* 32 round keys * 8 bytes = 256 */

/* Key schedule: key = 10 bytes, rk = 256 bytes (same result from both functions; each variant
 * has its own copy so that the Flash accounting of the two variants stays independent). */
void present_opt_setup(const uint8_t key[10], uint8_t rk[PRESENT_RK_BYTES]);
void present_naive_setup(const uint8_t key[10], uint8_t rk[PRESENT_RK_BYTES]);

/* Optimized: 256-byte S-box and pLayer tables in Flash (LPM), unrolled pLayer. */
void present_opt_encrypt(uint8_t blk[8], const uint8_t rk[PRESENT_RK_BYTES]);
void present_opt_decrypt(uint8_t blk[8], const uint8_t rk[PRESENT_RK_BYTES]);

/* Naive: 16-entry S-box, pLayer as a loop over 64 single bits. */
void present_naive_encrypt(uint8_t blk[8], const uint8_t rk[PRESENT_RK_BYTES]);
void present_naive_decrypt(uint8_t blk[8], const uint8_t rk[PRESENT_RK_BYTES]);

#endif
