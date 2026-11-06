/* SPECK64/128: 64-bit block, 128-bit key, 27 rounds, 32-bit words.
 * Block layout: blk[0] = x (first word of the paper's test vector), blk[1] = y.
 * Key layout:   key[0] = k0, key[1] = l0, key[2] = l1, key[3] = l2
 *               (paper: key = l2 l1 l0 k0 = 1b1a1918 13121110 0b0a0908 03020100).
 */
#ifndef SPECK_H
#define SPECK_H
#include <stdint.h>

#define SPECK_ROUNDS 27

/* Key schedule: expands 4 key words into 27 round keys (108 bytes of RAM). */
void speck_setup(const uint32_t key[4], uint32_t rk[SPECK_ROUNDS]);

/* Plain C implementation (works on host and AVR). */
void speck_encrypt(uint32_t blk[2], const uint32_t rk[SPECK_ROUNDS]);
void speck_decrypt(uint32_t blk[2], const uint32_t rk[SPECK_ROUNDS]);

/* Hand-written AVR assembly (speck_asm.S). AVR only. */
void speck_encrypt_asm(uint32_t blk[2], const uint32_t rk[SPECK_ROUNDS]);
void speck_decrypt_asm(uint32_t blk[2], const uint32_t rk[SPECK_ROUNDS]);

#endif
