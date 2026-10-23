/*
 * cipher_bench.ino - on-board benchmark of the SPECK / PRESENT / ChaCha20 variants
 * (Arduino Uno / Nano, ATmega328P @ 16 MHz).
 *
 * The cipher sources are NOT stored in this folder in git: run
 *     hardware/tools/sync_sources.sh
 * first, it copies them from ../../src (the single source of truth) into this sketch folder.
 *
 * Output (115200 baud):
 *     BENCH_V1 F_CPU=16000000
 *     selftest,speck_c,PASS ...
 *     variant,enc_cycles_per_byte
 *     speck_c,895.0 ...
 */
extern "C" {
#include "speck.h"
#include "present.h"
#include "chacha.h"
}
#include "vectors.h"

#define NBYTES 128                       // data per run: 16 blocks of 8 bytes / 2 blocks of 64 bytes
#define TRIALS 3                         // every variant is timed 3 times, the fastest trial is reported

static uint32_t b32[NBYTES / 4];         // 4-byte aligned data buffer
#define B ((uint8_t *)b32)
static uint8_t  key[32];
static uint32_t srk[SPECK_ROUNDS];
static uint8_t  prk_opt[PRESENT_RK_BYTES];
static uint8_t  prk_naive[PRESENT_RK_BYTES];
static const uint8_t cnonce[12] = { 0, 0, 0, 0, 0, 0, 0, 0x4a, 0, 0, 0, 0 };

// ---- the timed kernels: each processes NBYTES bytes ----
static void k_speck_c()      { for (uint8_t i = 0; i < NBYTES / 8; i++) speck_encrypt((uint32_t *)(B + 8 * i), srk); }
static void k_speck_asm()    { for (uint8_t i = 0; i < NBYTES / 8; i++) speck_encrypt_asm((uint32_t *)(B + 8 * i), srk); }
static void k_present_naive(){ for (uint8_t i = 0; i < NBYTES / 8; i++) present_naive_encrypt(B + 8 * i, prk_naive); }
static void k_present_opt()  { for (uint8_t i = 0; i < NBYTES / 8; i++) present_opt_encrypt(B + 8 * i, prk_opt); }
static void k_chacha_c()     { chacha20_xor(key, cnonce, 1, B, NBYTES); }
static void k_chacha_fast()  { chacha20_xor_fast(key, cnonce, 1, B, NBYTES); }

// ---- self-tests against the official vectors (so a wrong result can never be reported as a speed) ----
static bool test_speck(bool use_asm) {
  uint32_t k[4], rk[SPECK_ROUNDS], b[2], t[2];
  memcpy_P(k, SPECK_V_KEY, sizeof k);
  speck_setup(k, rk);
  memcpy_P(b, SPECK_V_PT, sizeof b);
  if (use_asm) speck_encrypt_asm(b, rk); else speck_encrypt(b, rk);
  memcpy_P(t, SPECK_V_CT, sizeof t);
  if (memcmp(b, t, sizeof b)) return false;
  if (use_asm) speck_decrypt_asm(b, rk); else speck_decrypt(b, rk);
  memcpy_P(t, SPECK_V_PT, sizeof t);
  return memcmp(b, t, sizeof b) == 0;
}

static bool test_present(bool opt) {
  uint8_t k[10], pt[8], ct[8], b[8], rk[PRESENT_RK_BYTES];
  for (uint8_t i = 0; i < PRESENT_NVEC; i++) {
    memcpy_P(k, PRESENT_V_KEY[i], 10);
    memcpy_P(pt, PRESENT_V_PT[i], 8);
    memcpy_P(ct, PRESENT_V_CT[i], 8);
    if (opt) present_opt_setup(k, rk); else present_naive_setup(k, rk);
    memcpy(b, pt, 8);
    if (opt) present_opt_encrypt(b, rk); else present_naive_encrypt(b, rk);
    if (memcmp(b, ct, 8)) return false;
    if (opt) present_opt_decrypt(b, rk); else present_naive_decrypt(b, rk);
    if (memcmp(b, pt, 8)) return false;
  }
  return true;
}

static bool test_chacha(bool fast) {
  uint8_t k[32], n[12], ks[64], z[64];
  memcpy_P(k, CHACHA_V_KEY, 32);
  memcpy_P(n, CHACHA_V_NONCE, 12);
  memcpy_P(ks, CHACHA_V_KS, 64);
  memset(z, 0, 64);
  if (fast) chacha20_xor_fast(k, n, CHACHA_V_COUNTER, z, 64);
  else      chacha20_xor(k, n, CHACHA_V_COUNTER, z, 64);
  return memcmp(z, ks, 64) == 0;
}

static void selftest(const char *name, bool ok) {
  Serial.print(F("selftest,")); Serial.print(name); Serial.println(ok ? F(",PASS") : F(",FAIL"));
}

// ---- timing: micros() has 4 us resolution, so repeat the kernel until the run lasts well over 1 ms ----
static void bench(const char *name, void (*f)(), uint16_t reps) {
  unsigned long best = 0xFFFFFFFFUL;
  for (uint8_t t = 0; t < TRIALS; t++) {
    unsigned long t0 = micros();
    for (uint16_t r = 0; r < reps; r++) f();
    unsigned long dt = micros() - t0;
    if (dt < best) best = dt;
  }
  float cpb = (float)best * (F_CPU / 1000000UL) / ((float)reps * NBYTES);
  Serial.print(name); Serial.print(','); Serial.println(cpb, 1);
}

void setup() {
  Serial.begin(115200);
  delay(1500);                           // let the serial link settle after the board reset (avoids garbage on the first line)
  Serial.print(F("BENCH_V1 F_CPU=")); Serial.println(F_CPU);

  for (uint8_t i = 0; i < 32; i++) key[i] = 0x40 + 5 * i;
  for (uint8_t i = 0; i < NBYTES; i++) B[i] = 7 * i + 3;

  selftest("speck_c", test_speck(false));
  selftest("speck_asm", test_speck(true));
  selftest("present_naive", test_present(false));
  selftest("present_opt", test_present(true));
  selftest("chacha_c", test_chacha(false));
  selftest("chacha_fast", test_chacha(true));

  uint32_t k4[4]; memcpy_P(k4, SPECK_V_KEY, sizeof k4);
  speck_setup(k4, srk);
  present_opt_setup(key, prk_opt);
  present_naive_setup(key, prk_naive);

  Serial.println(F("variant,enc_cycles_per_byte"));
  bench("speck_c", k_speck_c, 20);
  bench("speck_asm", k_speck_asm, 20);
  bench("present_naive", k_present_naive, 1);
  bench("present_opt", k_present_opt, 5);
  bench("chacha_c", k_chacha_c, 5);
  bench("chacha_fast", k_chacha_fast, 5);
  Serial.println(F("DONE"));
}

void loop() {}
