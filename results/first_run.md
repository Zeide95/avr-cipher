# Board run: (first run)

| Field | Value |
|---|---|
| Board | Arduino Uno / Nano (ATmega328P) |
| Clock | 16 MHz |
| Toolchain | Arduino IDE 2.3.10, default flags (`-Os`) |
| Sketch | first version (no self-test, old output format) |

Encrypt, 128 bytes per run, cycles per byte (`micros()`-based):

| Variant | cycles/byte |
|---|---:|
| speck_asm | 131.0 |
| chacha_c | 825.0 |
| chacha_fast | 825.0 |
| speck_c | 895.0 |
| present_opt (tables + LPM) | 1780.5 |
| present_naive | 23736.5 |

Notes:
* Two consecutive runs (board reset when the serial monitor opened) gave identical values.
* `chacha_c` and `chacha_fast` are identical, probably the same generated code, to be verified.
* The self-tests were added to the sketch after this run; repeat the run with the current sketch
  and store it as a new file.
