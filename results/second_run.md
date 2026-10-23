# Board run: (second run, with self-tests)

| Field | Value |
|---|---|
| Board | Arduino Uno, ATmega328P @ 16 MHz |
| Toolchain | Arduino IDE 2.3.10, default flags (-Os) |
| Sketch | BENCH_V1, 3 trials per variant, fastest reported |

Self-tests: all six PASS.

| Variant | enc cycles/byte |
|---|---:|
| speck_asm | 129.6 |
| chacha_fast | 820.0 |
| chacha_c | 819.9 |
| speck_c | 891.7 |
| present_opt | 1786.7 |
| present_naive | 24109.0 |

Notes: differs from the first run by at most 1.6 %. chacha_c and chacha_fast differ by one micros() tick.
