/* Key schedule shared by both PRESENT variants.
 * It is #included (not compiled separately) so that every variant carries exactly one
 * copy of the 4-bit S-box in Flash.  The includer must define PRESENT_SETUP_NAME and have
 * included <string.h>, "present.h" and "present_tables.h" beforehand.
 */

/* 80-bit key register (little-endian bytes k[0..9]).
 * Round key i = bits 79..16 = bytes k[2..9].
 * Update: rotate left by 61, S-box on bits 79..76, XOR round counter into bits 19..15.
 */
void PRESENT_SETUP_NAME(const uint8_t key[10], uint8_t rk[PRESENT_RK_BYTES])
{
    uint8_t k[10], t[10];
    uint8_t r, i, n, c;

    memcpy(k, key, 10);
    for (r = 0; r < PRESENT_ROUNDS + 1; r++) {
        memcpy(rk + 8 * r, k + 2, 8);
        if (r == PRESENT_ROUNDS)
            break;

        /* rotate left 61 == rotate right 19 == right 16 (two bytes) + right 3 */
        for (i = 0; i < 10; i++)
            t[i] = k[(i + 2 >= 10) ? i - 8 : i + 2];
        for (n = 0; n < 3; n++) {
            c = t[0] & 1;
            for (i = 0; i < 9; i++)
                t[i] = (uint8_t)((t[i] >> 1) | (t[i + 1] << 7));
            t[9] = (uint8_t)((t[9] >> 1) | (c << 7));
        }

        /* S-box on the top nibble */
        t[9] = (uint8_t)((pgm_read_byte(&PRESENT_SBOX4[t[9] >> 4]) << 4) | (t[9] & 0x0F));

        /* round counter (r + 1) into bits 19..15 */
        c = r + 1;
        t[1] ^= (uint8_t)((c & 1) << 7);
        t[2] ^= (uint8_t)(c >> 1);

        memcpy(k, t, 10);
    }
}
