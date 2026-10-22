/* PRESENT, textbook variant: 4-bit S-box table and a bit-by-bit pLayer.
 * Deliberately simple - it is the baseline that shows how expensive bit
 * permutations are on an 8-bit CPU without a barrel shifter.
 */
#include <string.h>
#include "present.h"
#include "present_tables.h"

#define PRESENT_SETUP_NAME present_naive_setup
#include "present_key.inc"

static void sbox_layer(uint8_t s[8], const uint8_t *tab)
{
    uint8_t a;
    for (a = 0; a < 8; a++) {
        uint8_t lo = pgm_read_byte(&tab[s[a] & 0x0F]);
        uint8_t hi = pgm_read_byte(&tab[s[a] >> 4]);
        s[a] = (uint8_t)((hi << 4) | lo);
    }
}

/* forward: output bit PBOX[i] = input bit i;  inverse: output bit i = input bit PBOX[i] */
static void player(uint8_t s[8], uint8_t inverse)
{
    uint8_t o[8];
    uint8_t i, j, bit;

    memset(o, 0, 8);
    for (i = 0; i < 64; i++) {
        j = pgm_read_byte(&PRESENT_PBOX[i]);
        if (!inverse) {
            bit = (s[i >> 3] >> (i & 7)) & 1;
            o[j >> 3] |= (uint8_t)(bit << (j & 7));
        } else {
            bit = (s[j >> 3] >> (j & 7)) & 1;
            o[i >> 3] |= (uint8_t)(bit << (i & 7));
        }
    }
    memcpy(s, o, 8);
}

static void add_key(uint8_t s[8], const uint8_t *k)
{
    uint8_t a;
    for (a = 0; a < 8; a++)
        s[a] ^= k[a];
}

void present_naive_encrypt(uint8_t s[8], const uint8_t rk[PRESENT_RK_BYTES])
{
    uint8_t r;
    for (r = 0; r < PRESENT_ROUNDS; r++) {
        add_key(s, rk + 8 * r);
        sbox_layer(s, PRESENT_SBOX4);
        player(s, 0);
    }
    add_key(s, rk + 8 * PRESENT_ROUNDS);
}

void present_naive_decrypt(uint8_t s[8], const uint8_t rk[PRESENT_RK_BYTES])
{
    uint8_t r = PRESENT_ROUNDS;
    add_key(s, rk + 8 * PRESENT_ROUNDS);
    while (r--) {
        player(s, 1);
        sbox_layer(s, PRESENT_SBOX4_INV);
        add_key(s, rk + 8 * r);
    }
}
