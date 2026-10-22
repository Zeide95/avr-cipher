/* PRESENT, table-driven variant for 8-bit AVR.
 *
 * Round = addRoundKey, sBoxLayer, pLayer. For every state byte we do
 *     t = PL[ SBOX8[ s[a] ^ rk[a] ] ]        (two LPM instructions)
 * and scatter the four 2-bit fields of t into the four output byte pairs.
 * See tools/gen_tables.py for how PL is derived from the 4x16 bit transposition.
 * The loops are unrolled by macros so that every shift count is a constant.
 */
#include <string.h>
#include "present.h"
#include "present_tables.h"

#define PRESENT_SETUP_NAME present_opt_setup
#include "present_key.inc"

#define SB8(v)  pgm_read_byte(&PRESENT_SBOX8[(v)])
#define SB8I(v) pgm_read_byte(&PRESENT_SBOX8_INV[(v)])
#define PLT(v)  pgm_read_byte(&PRESENT_PL[(v)])

/* forward: byte a of the state (after sbox) -> output bytes */
#define FWD(a)                                              \
    do {                                                    \
        uint8_t t_ = PLT(SB8(s[a] ^ rk[a]));                \
        o[(a) >> 2]       |= (uint8_t)((t_ & 3) << (2 * ((a) & 3)));        \
        o[2 + ((a) >> 2)] |= (uint8_t)(((t_ >> 2) & 3) << (2 * ((a) & 3))); \
        o[4 + ((a) >> 2)] |= (uint8_t)(((t_ >> 4) & 3) << (2 * ((a) & 3))); \
        o[6 + ((a) >> 2)] |= (uint8_t)((t_ >> 6) << (2 * ((a) & 3)));       \
    } while (0)

void present_opt_encrypt(uint8_t s[8], const uint8_t rk[PRESENT_RK_BYTES])
{
    uint8_t o[8];
    uint8_t r;

    for (r = 0; r < PRESENT_ROUNDS; r++) {
        memset(o, 0, 8);
        FWD(0); FWD(1); FWD(2); FWD(3); FWD(4); FWD(5); FWD(6); FWD(7);
        memcpy(s, o, 8);
        rk += 8;
    }
    for (r = 0; r < 8; r++)
        s[r] ^= rk[r];
}

/* inverse pLayer: input byte m, bit t goes to output bit 32*(m&1) + 4*t + (m>>1) */
#define INV(m)                                                                   \
    do {                                                                         \
        uint8_t v_ = s[m];                                                       \
        uint8_t x_;                                                              \
        x_ = v_ & 3;        o[4 * ((m) & 1) + 0] |= (uint8_t)(((x_ & 1) | ((x_ & 2) << 3)) << ((m) >> 1)); \
        x_ = (v_ >> 2) & 3; o[4 * ((m) & 1) + 1] |= (uint8_t)(((x_ & 1) | ((x_ & 2) << 3)) << ((m) >> 1)); \
        x_ = (v_ >> 4) & 3; o[4 * ((m) & 1) + 2] |= (uint8_t)(((x_ & 1) | ((x_ & 2) << 3)) << ((m) >> 1)); \
        x_ = v_ >> 6;       o[4 * ((m) & 1) + 3] |= (uint8_t)(((x_ & 1) | ((x_ & 2) << 3)) << ((m) >> 1)); \
    } while (0)

void present_opt_decrypt(uint8_t s[8], const uint8_t rk[PRESENT_RK_BYTES])
{
    uint8_t o[8];
    uint8_t r, a;

    rk += 8 * PRESENT_ROUNDS;
    for (a = 0; a < 8; a++)
        s[a] ^= rk[a];
    for (r = 0; r < PRESENT_ROUNDS; r++) {
        rk -= 8;
        memset(o, 0, 8);
        INV(0); INV(1); INV(2); INV(3); INV(4); INV(5); INV(6); INV(7);
        for (a = 0; a < 8; a++)
            s[a] = SB8I(o[a]) ^ rk[a];
    }
}
