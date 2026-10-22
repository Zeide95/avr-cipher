# Cryptographic Co-Design: SPECK, PRESENT & ChaCha20 on an 8-bit AVR

**Research question:** which lightweight cipher implementation gives the best balance between
clock cycles per byte and security margin on an 8-bit microcontroller?

Target: ATmega328P (Arduino Uno/Nano), 16 MHz. Everything is measured in the **simavr** simulator
(exact cycle counting), so no real hardware is needed.

## 1. What is inside

| Variant | Description |
|---|---|
| `speck_c` | SPECK64/128 in plain C |
| `speck_asm` | SPECK64/128 with the round function in AVR assembly (registers R0-R31; 8-bit rotations are done by renaming registers) |
| `present_naive` | Textbook PRESENT-80: 16-entry S-box, bit permutation as a loop over 64 bits |
| `present_opt` | PRESENT-80 with tables in Flash (`LPM` instruction): byte-wide S-box + pLayer table, unrolled loops |
| `chacha_c` | ChaCha20 in C, ordinary shifts |
| `chacha_fast` | ChaCha20 with rotations built from byte shuffles (rotl16, rotl8 + a short bit rotation) |

Folder structure:

```
src/      ciphers (speck.c, speck_asm.S, present_*.c, chacha.c), vectors.h with the official test vectors
bench/    main.c - AVR firmware, runner.c - runs it in simavr and counts cycles
tests/    test_ciphers.c - CUnit tests on the PC, compat/ - avr/pgmspace.h stand-in for the PC
tools/    gen_tables.py (PRESENT tables), collect.py (table and plots)
data/     security.csv - best known attacks (edit it to match your literature)
Makefile  make check / bench / report / all
```

## 2. Installation (Ubuntu)

```bash
sudo apt update
sudo apt install -y gcc-avr binutils-avr avr-libc simavr libsimavr-dev libelf-dev \
    libcunit1-dev build-essential make python3 python3-matplotlib
```

Check: `avr-gcc --version`, `simavr --help | head -3`.
If `-lsimavr` is not found or the simulator is too old, build it from source:
```bash
git clone https://github.com/buserror/simavr.git && cd simavr && make && sudo make install
```
(The Makefile already searches for headers and the library in both `/usr/include/simavr` and `/usr/local`.)

## 3. Running

```bash
make check     # CUnit tests on the PC: official SPECK, PRESENT (4) and ChaCha20 (RFC 7539) vectors + round trips
make bench     # builds 6 firmwares + an empty baseline, runs each in simavr
make report    # results/results.md, results.csv, cpb.png, tradeoff.png
make all       # everything in sequence
```

Expected result of `make check`: in the final CUnit summary the `failed` column is zero for the `tests` and `asserts` rows. If a test fails, the measurements are meaningless.

`make bench` prints one line per variant:
```
RESULT setup=<cycles> enc=<cycles> dec=<cycles> flags=3 total=<cycles>
```
`flags=3` means the firmware passed the official test vectors **on the AVR itself** (bit 0) and decryption returned the original data (bit 1).
If `flags` is not 3, that implementation is broken and its numbers must not be used.

## 4. How the measurements work

* Every phase processes 128 bytes: 16 blocks of SPECK/PRESENT (8 bytes each) or 2 blocks of ChaCha20 (64 bytes each).
  **Cycles per byte = phase cycles / 128.**
* The firmware writes a phase number to the unused register `GPIOR1` (one `OUT` instruction, 1 cycle),
  and `bench/runner.c` checks this register after every instruction and records `avr->cycle`.
  The error is 1-2 cycles per phase, negligible over 128 bytes.
* **Flash** and **RAM** = firmware size minus the size of the empty baseline firmware (`none`),
  built with the same flags; the self-test code is not included in these figures (build with `-DSIZE_ONLY`).
  Stack usage is **not** counted (it is significant for ChaCha20: about 200 bytes for the state and keystream).
* Optimization is `-O2`. To compare code size: `make clean && make OPT=-Os bench report`.

## 5. How to read the result and what to write in the conclusion

* `results/results.md`: table sorted by speed. `results/tradeoff.png`: speed vs security margin.
* **Security margin** = (total rounds - rounds of the best known attack) / total rounds. The data is in `data/security.csv`
  with sources. I took it from Wikipedia and one IACR ePrint paper, so **check it against the
  original papers before submitting** and add recent results (especially for ChaCha20 and full-round attacks on PRESENT).
  Keep in mind that the number of attacked rounds is only comparable within one cipher: across ciphers it merely shows
  "how far cryptanalysis has progressed", not absolute strength
  (SPECK64/128 and PRESENT-80 have a 64-bit block, ChaCha20 uses a 256-bit key).
* The "Balance" column = margin (%) / (cycles per byte / 100). This is one possible metric, not the truth;
  the report can discuss alternatives (for example cycles per byte × code size, or a Pareto front on the plot).
* Qualitative effects worth verifying with the numbers: assembly gives a speedup for SPECK,
  for PRESENT the main cost is the bit permutation (Flash tables and `LPM` trade Flash for speed),
  for ChaCha20 the cost is 32-bit operations and rotations on an 8-bit core.

## 6. Limitations

* simavr counts core cycles; real boards can differ only if the code waits for peripherals (this code does not).
* One microcontroller (ATmega328P). To compare with the ATtiny85 change `MCU` in the Makefile and `avr_make_mcu_by_name` in `bench/runner.c`.
* There are no modes of operation (CBC, CTR, AEAD) and no side-channel protection: only the block/stream cipher core is measured.
* The implementations are educational: do not use them in products without an audit.

## 7. How to add your own variant

1. Put the code in `src/` and add a `#elif defined(VARIANT_MY)` block with the four adapter functions to `bench/main.c`.
2. In the `Makefile` add the name to `VARIANTS` and define `SRC_my`, `DEF_my`.
3. In `tools/collect.py` add a row to `VARIANTS`, and in `data/security.csv` a row for the cipher.
