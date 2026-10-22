/* AVR benchmark firmware (ATmega328P). One cipher variant per build, chosen by -DVARIANT_xxx.
 *
 * Talks to the simavr runner (bench/runner.c) through two spare I/O registers
 * (they exist on the ATmega328P and are not connected to any hardware):
 *   GPIOR1 = phase marker, written with a single OUT instruction (1 cycle):
 *              1/2 = key setup start/end, 3/4 = encrypt start/end, 5/6 = decrypt start/end
 *   GPIOR0 = result flags:  bit0 = official test vectors passed, bit1 = round trip OK
 * The runner records avr->cycle at every marker change, so the cycle counts are exact
 * to within the 1-2 cycles of the OUT instruction itself.
 */
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdint.h>
#include <string.h>

#include "vectors.h"

#pragma GCC diagnostic ignored "-Wunused-function"

#define NBYTES 128                     /* amount of data per timed phase */

static uint32_t data32[NBYTES / 4];     /* 4-byte aligned data buffer */
#define data ((uint8_t *)data32)

/* ------------------------------------------------------------------ */
/* adapters: one block of glue code per cipher variant                 */
/* ------------------------------------------------------------------ */
#if defined(VARIANT_SPECK_C) || defined(VARIANT_SPECK_ASM)
  #include "speck.h"
  #define BLOCK 8
  #define KEYLEN 16
  #ifdef VARIANT_SPECK_ASM
    #define SPECK_ENC speck_encrypt_asm
    #define SPECK_DEC speck_decrypt_asm
  #else
    #define SPECK_ENC speck_encrypt
    #define SPECK_DEC speck_decrypt
  #endif
  static uint32_t rk[SPECK_ROUNDS];
  static void adapter_setup(const uint8_t *key) {
      uint32_t k[4];
      memcpy(k, key, 16);
      speck_setup(k, rk);
  }
  static void adapter_encrypt(uint8_t *p, uint8_t n) { while (n--) { SPECK_ENC((uint32_t *)p, rk); p += BLOCK; } }
  static void adapter_decrypt(uint8_t *p, uint8_t n) { while (n--) { SPECK_DEC((uint32_t *)p, rk); p += BLOCK; } }
  static uint8_t adapter_selftest(void) {
      uint32_t k[4], b[2], t[2];
      memcpy_P(k, SPECK_V_KEY, sizeof k);
      speck_setup(k, rk);
      memcpy_P(b, SPECK_V_PT, sizeof b);
      SPECK_ENC(b, rk);
      memcpy_P(t, SPECK_V_CT, sizeof t);
      if (memcmp(b, t, sizeof b)) return 0;
      SPECK_DEC(b, rk);
      memcpy_P(t, SPECK_V_PT, sizeof t);
      return memcmp(b, t, sizeof b) == 0;
  }

#elif defined(VARIANT_PRESENT_NAIVE) || defined(VARIANT_PRESENT_OPT)
  #include "present.h"
  #define BLOCK 8
  #define KEYLEN 10
  #ifdef VARIANT_PRESENT_OPT
    #define PRESENT_ENC present_opt_encrypt
    #define PRESENT_DEC present_opt_decrypt
    #define PRESENT_SETUP present_opt_setup
  #else
    #define PRESENT_ENC present_naive_encrypt
    #define PRESENT_DEC present_naive_decrypt
    #define PRESENT_SETUP present_naive_setup
  #endif
  static uint8_t rk[PRESENT_RK_BYTES];
  static void adapter_setup(const uint8_t *key) { PRESENT_SETUP(key, rk); }
  static void adapter_encrypt(uint8_t *p, uint8_t n) { while (n--) { PRESENT_ENC(p, rk); p += BLOCK; } }
  static void adapter_decrypt(uint8_t *p, uint8_t n) { while (n--) { PRESENT_DEC(p, rk); p += BLOCK; } }
  static uint8_t adapter_selftest(void) {
      uint8_t i, key[10], pt[8], ct[8], b[8];
      for (i = 0; i < PRESENT_NVEC; i++) {
          memcpy_P(key, PRESENT_V_KEY[i], 10);
          memcpy_P(pt, PRESENT_V_PT[i], 8);
          memcpy_P(ct, PRESENT_V_CT[i], 8);
          PRESENT_SETUP(key, rk);
          memcpy(b, pt, 8);
          PRESENT_ENC(b, rk);
          if (memcmp(b, ct, 8)) return 0;
          PRESENT_DEC(b, rk);
          if (memcmp(b, pt, 8)) return 0;
      }
      return 1;
  }

#elif defined(VARIANT_CHACHA_C) || defined(VARIANT_CHACHA_FAST)
  #include "chacha.h"
  #define BLOCK 64
  #define KEYLEN 32
  #ifdef VARIANT_CHACHA_FAST
    #define CHACHA_XOR chacha20_xor_fast
  #else
    #define CHACHA_XOR chacha20_xor
  #endif
  static uint8_t ckey[32];
  static const uint8_t cnonce[12] = { 0, 0, 0, 0, 0, 0, 0, 0x4a, 0, 0, 0, 0 };
  static void adapter_setup(const uint8_t *key) { memcpy(ckey, key, 32); }   /* nothing to precompute */
  static void adapter_encrypt(uint8_t *p, uint8_t n) { CHACHA_XOR(ckey, cnonce, 1, p, (uint16_t)n * BLOCK); }
  static void adapter_decrypt(uint8_t *p, uint8_t n) { CHACHA_XOR(ckey, cnonce, 1, p, (uint16_t)n * BLOCK); }
  static uint8_t adapter_selftest(void) {
      uint8_t key[32], nonce[12], ks[64], z[64];
      memcpy_P(key, CHACHA_V_KEY, 32);
      memcpy_P(nonce, CHACHA_V_NONCE, 12);
      memcpy_P(ks, CHACHA_V_KS, 64);
      memset(z, 0, 64);
      CHACHA_XOR(key, nonce, CHACHA_V_COUNTER, z, 64);
      return memcmp(z, ks, 64) == 0;
  }

#elif defined(VARIANT_NONE)             /* empty baseline: measures the harness itself */
  #define BLOCK 8
  #define KEYLEN 16
  static void adapter_setup(const uint8_t *key) { (void)key; }
  static void adapter_encrypt(uint8_t *p, uint8_t n) { (void)p; (void)n; }
  static void adapter_decrypt(uint8_t *p, uint8_t n) { (void)p; (void)n; }
  static uint8_t adapter_selftest(void) { return 1; }

#else
  #error "define one of VARIANT_SPECK_C VARIANT_SPECK_ASM VARIANT_PRESENT_NAIVE VARIANT_PRESENT_OPT VARIANT_CHACHA_C VARIANT_CHACHA_FAST VARIANT_NONE"
#endif

#define NB (NBYTES / BLOCK)

int main(void)
{
    uint8_t key[32];
    uint8_t i, flags = 0, ok = 1;

#ifndef SIZE_ONLY                        /* SIZE_ONLY builds leave out the test code to measure the cipher alone */
    if (adapter_selftest())
        flags |= 1;
#endif

    for (i = 0; i < 32; i++)
        key[i] = (uint8_t)(0x40 + 5 * i);
    for (i = 0; i < NBYTES; i++)
        data[i] = (uint8_t)(7 * i + 3);

    GPIOR1 = 1;  adapter_setup(key);        GPIOR1 = 2;
    GPIOR1 = 3;  adapter_encrypt(data, NB); GPIOR1 = 4;
    GPIOR1 = 5;  adapter_decrypt(data, NB); GPIOR1 = 6;

    for (i = 0; i < NBYTES; i++)
        if (data[i] != (uint8_t)(7 * i + 3))
            ok = 0;
    if (ok)
        flags |= 2;

    GPIOR0 = flags;
    GPIOR1 = 7;

    cli();
    sleep_enable();
    sleep_cpu();                            /* sleeping with interrupts off makes simavr stop */
    for (;;) ;
}
