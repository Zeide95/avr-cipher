#include "chacha.h"

/* ---- rotation flavours ---- */
#define ROTL(x, n)  (((x) << (n)) | ((x) >> (32 - (n))))

#define G16(x)  ROTL(x, 16)
#define G12(x)  ROTL(x, 12)
#define G8(x)   ROTL(x, 8)
#define G7(x)   ROTL(x, 7)

/* "fast" flavour: a rotation by a multiple of 8 is only a byte shuffle on AVR,
 * so compose the odd rotations from such a shuffle and a very short bit rotation. */
#define INL static inline __attribute__((always_inline))
INL uint32_t rotl16(uint32_t x) { return (x << 16) | (x >> 16); }
INL uint32_t rotl8(uint32_t x)  { return (x << 8)  | (x >> 24); }
INL uint32_t rotr4(uint32_t x)  { return (x >> 4)  | (x << 28); }
INL uint32_t rotr1(uint32_t x)  { return (x >> 1)  | (x << 31); }

#define F16(x)  rotl16(x)
#define F12(x)  rotr4(rotl16(x))
#define F8(x)   rotl8(x)
#define F7(x)   rotr1(rotl8(x))

/* ---- quarter round / double round ---- */
#define QR(a, b, c, d, R16, R12, R8, R7)        \
    x[a] += x[b]; x[d] ^= x[a]; x[d] = R16(x[d]); \
    x[c] += x[d]; x[b] ^= x[c]; x[b] = R12(x[b]); \
    x[a] += x[b]; x[d] ^= x[a]; x[d] = R8(x[d]);  \
    x[c] += x[d]; x[b] ^= x[c]; x[b] = R7(x[b]);

#define DOUBLE_ROUND(R16, R12, R8, R7)          \
    QR(0, 4,  8, 12, R16, R12, R8, R7)          \
    QR(1, 5,  9, 13, R16, R12, R8, R7)          \
    QR(2, 6, 10, 14, R16, R12, R8, R7)          \
    QR(3, 7, 11, 15, R16, R12, R8, R7)          \
    QR(0, 5, 10, 15, R16, R12, R8, R7)          \
    QR(1, 6, 11, 12, R16, R12, R8, R7)          \
    QR(2, 7,  8, 13, R16, R12, R8, R7)          \
    QR(3, 4,  9, 14, R16, R12, R8, R7)

static uint32_t ld32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* One 64-byte keystream block from the 16-word state. */
#define DEFINE_BLOCK(NAME, R16, R12, R8, R7)                          \
static void NAME(const uint32_t in[16], uint8_t out[64])              \
{                                                                     \
    uint32_t x[16];                                                   \
    uint8_t i;                                                        \
    for (i = 0; i < 16; i++) x[i] = in[i];                            \
    for (i = 0; i < 10; i++) { DOUBLE_ROUND(R16, R12, R8, R7) }       \
    for (i = 0; i < 16; i++) {                                        \
        uint32_t w = x[i] + in[i];                                    \
        out[4 * i + 0] = (uint8_t)w;                                  \
        out[4 * i + 1] = (uint8_t)(w >> 8);                           \
        out[4 * i + 2] = (uint8_t)(w >> 16);                          \
        out[4 * i + 3] = (uint8_t)(w >> 24);                          \
    }                                                                 \
}

#define DEFINE_XOR(NAME, BLOCKFN)                                                   \
void NAME(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter,         \
          uint8_t *buf, uint16_t len)                                               \
{                                                                                   \
    uint32_t st[16];                                                                \
    uint8_t ks[64];                                                                 \
    uint8_t i, n;                                                                   \
    st[0] = 0x61707865UL; st[1] = 0x3320646eUL;                                     \
    st[2] = 0x79622d32UL; st[3] = 0x6b206574UL;                                     \
    for (i = 0; i < 8; i++) st[4 + i] = ld32(key + 4 * i);                          \
    for (i = 0; i < 3; i++) st[13 + i] = ld32(nonce + 4 * i);                       \
    while (len) {                                                                   \
        st[12] = counter++;                                                         \
        BLOCKFN(st, ks);                                                            \
        n = (len > 64) ? 64 : (uint8_t)len;                                         \
        for (i = 0; i < n; i++) buf[i] ^= ks[i];                                    \
        buf += n;                                                                   \
        len -= n;                                                                   \
    }                                                                               \
}

DEFINE_BLOCK(block_generic, G16, G12, G8, G7)
DEFINE_BLOCK(block_fast,    F16, F12, F8, F7)

DEFINE_XOR(chacha20_xor,      block_generic)
DEFINE_XOR(chacha20_xor_fast, block_fast)
