/* CUnit tests (run on the PC, not on the AVR):  make check */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <CUnit/Basic.h>

#include "speck.h"
#include "present.h"
#include "chacha.h"
#include "vectors.h"

/* small deterministic PRNG so that failures are reproducible */
static uint32_t rng_state = 0x12345678UL;
static uint32_t rnd(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
static void rnd_bytes(uint8_t *p, size_t n)
{
    while (n--) *p++ = (uint8_t)rnd();
}

/* ---------------- SPECK ---------------- */
static void test_speck_vector(void)
{
    uint32_t key[4], blk[2], rk[SPECK_ROUNDS];
    memcpy_P(key, SPECK_V_KEY, sizeof key);
    speck_setup(key, rk);

    memcpy_P(blk, SPECK_V_PT, sizeof blk);
    speck_encrypt(blk, rk);
    CU_ASSERT_EQUAL(blk[0], 0x8c6fa548UL);
    CU_ASSERT_EQUAL(blk[1], 0x454e028bUL);

    speck_decrypt(blk, rk);
    CU_ASSERT_EQUAL(blk[0], 0x3b726574UL);
    CU_ASSERT_EQUAL(blk[1], 0x7475432dUL);
}

static void test_speck_roundtrip(void)
{
    int n;
    for (n = 0; n < 200; n++) {
        uint32_t key[4], rk[SPECK_ROUNDS], blk[2], orig[2];
        rnd_bytes((uint8_t *)key, sizeof key);
        rnd_bytes((uint8_t *)orig, sizeof orig);
        speck_setup(key, rk);
        memcpy(blk, orig, sizeof blk);
        speck_encrypt(blk, rk);
        CU_ASSERT_TRUE(memcmp(blk, orig, sizeof blk) != 0);
        speck_decrypt(blk, rk);
        CU_ASSERT_TRUE(memcmp(blk, orig, sizeof blk) == 0);
    }
}

/* ---------------- PRESENT ---------------- */
typedef void (*present_fn)(uint8_t *, const uint8_t *);
typedef void (*present_setup_fn)(const uint8_t *, uint8_t *);

static void present_vectors(present_setup_fn setup, present_fn enc, present_fn dec)
{
    int i;
    for (i = 0; i < PRESENT_NVEC; i++) {
        uint8_t key[10], pt[8], ct[8], blk[8], rk[PRESENT_RK_BYTES];
        memcpy_P(key, PRESENT_V_KEY[i], 10);
        memcpy_P(pt, PRESENT_V_PT[i], 8);
        memcpy_P(ct, PRESENT_V_CT[i], 8);
        setup(key, rk);
        memcpy(blk, pt, 8);
        enc(blk, rk);
        CU_ASSERT_TRUE(memcmp(blk, ct, 8) == 0);
        dec(blk, rk);
        CU_ASSERT_TRUE(memcmp(blk, pt, 8) == 0);
    }
}
static void test_present_opt_vectors(void)   { present_vectors(present_opt_setup, present_opt_encrypt, present_opt_decrypt); }
static void test_present_naive_vectors(void) { present_vectors(present_naive_setup, present_naive_encrypt, present_naive_decrypt); }

static void test_present_variants_agree(void)
{
    int n;
    for (n = 0; n < 200; n++) {
        uint8_t key[10], rk[PRESENT_RK_BYTES], rk2[PRESENT_RK_BYTES], a[8], b[8], orig[8];
        rnd_bytes(key, 10);
        rnd_bytes(orig, 8);
        present_opt_setup(key, rk);
        present_naive_setup(key, rk2);
        CU_ASSERT_TRUE(memcmp(rk, rk2, sizeof rk) == 0);
        memcpy(a, orig, 8); memcpy(b, orig, 8);
        present_opt_encrypt(a, rk);
        present_naive_encrypt(b, rk);
        CU_ASSERT_TRUE(memcmp(a, b, 8) == 0);
        present_opt_decrypt(a, rk);
        present_naive_decrypt(b, rk);
        CU_ASSERT_TRUE(memcmp(a, orig, 8) == 0);
        CU_ASSERT_TRUE(memcmp(b, orig, 8) == 0);
    }
}

/* ---------------- ChaCha20 ---------------- */
typedef void (*chacha_fn)(const uint8_t *, const uint8_t *, uint32_t, uint8_t *, uint16_t);

static void chacha_vector(chacha_fn f)
{
    uint8_t key[32], nonce[12], ks[64], zero[64];
    memcpy_P(key, CHACHA_V_KEY, 32);
    memcpy_P(nonce, CHACHA_V_NONCE, 12);
    memcpy_P(ks, CHACHA_V_KS, 64);
    memset(zero, 0, 64);
    f(key, nonce, CHACHA_V_COUNTER, zero, 64);
    CU_ASSERT_TRUE(memcmp(zero, ks, 64) == 0);
}
static void test_chacha_vector(void)      { chacha_vector(chacha20_xor); }
static void test_chacha_fast_vector(void) { chacha_vector(chacha20_xor_fast); }

static void test_chacha_variants_agree(void)
{
    int n;
    for (n = 0; n < 50; n++) {
        uint8_t key[32], nonce[12], a[200], b[200], orig[200];
        uint16_t len = (uint16_t)(1 + rnd() % 200);   /* includes partial blocks */
        uint32_t ctr = rnd();
        rnd_bytes(key, 32); rnd_bytes(nonce, 12); rnd_bytes(orig, 200);
        memcpy(a, orig, 200); memcpy(b, orig, 200);
        chacha20_xor(key, nonce, ctr, a, len);
        chacha20_xor_fast(key, nonce, ctr, b, len);
        CU_ASSERT_TRUE(memcmp(a, b, 200) == 0);
        chacha20_xor(key, nonce, ctr, a, len);          /* decrypt = encrypt */
        CU_ASSERT_TRUE(memcmp(a, orig, 200) == 0);
    }
}

/* ---------------- runner ---------------- */
int main(void)
{
    CU_pSuite s;
    if (CU_initialize_registry() != CUE_SUCCESS)
        return CU_get_error();

    s = CU_add_suite("SPECK64/128", NULL, NULL);
    CU_add_test(s, "official test vector", test_speck_vector);
    CU_add_test(s, "encrypt/decrypt round trip", test_speck_roundtrip);

    s = CU_add_suite("PRESENT-80", NULL, NULL);
    CU_add_test(s, "official vectors (table variant)", test_present_opt_vectors);
    CU_add_test(s, "official vectors (naive variant)", test_present_naive_vectors);
    CU_add_test(s, "table variant == naive variant (incl. key schedule)", test_present_variants_agree);

    s = CU_add_suite("ChaCha20", NULL, NULL);
    CU_add_test(s, "RFC 7539 block vector (generic)", test_chacha_vector);
    CU_add_test(s, "RFC 7539 block vector (fast rotations)", test_chacha_fast_vector);
    CU_add_test(s, "fast == generic, partial blocks, decrypt", test_chacha_variants_agree);

    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    {
        unsigned fails = CU_get_number_of_failures();
        CU_cleanup_registry();
        return fails ? 1 : 0;
    }
}
