# Hardware implementation: SPECK / PRESENT / ChaCha20 on a real Arduino

This folder holds the **on-board** part of the project *Cryptographic Co-Design*.
The same cipher sources run on a real **Arduino Uno / Nano (ATmega328P, 16 MHz)** and print their speed over USB serial.

The goal is a sanity check of the simulator numbers on real silicon, not a second, independent benchmark:
the simulator counts cycles exactly, the board measures wall-clock time with `micros()`.

## Layout

```
hardware/
├── README.md                      this file
├── arduino/
│   └── cipher_bench/
│       └── cipher_bench.ino       the sketch
|       └── ...                    cipher codes
└── results/
    └── first_run.md               recorded board runs
    └── second_run.md
```

## Requirements

* Arduino Uno or Nano (ATmega328P) and a USB cable
* Arduino IDE 2.x **or** `arduino-cli` (`arduino:avr` core)
* Linux: your user must be in the `dialout` group: `sudo usermod -aG dialout $USER`, then log out and in

## Quick start

```bash
git checkout -b hardware          # branch for this work
```

**Arduino IDE:** open `hardware/arduino/cipher_bench/cipher_bench.ino` → *Tools → Board → Arduino Uno*
(Nano: *Processor → ATmega328P*, try *Old Bootloader* if the upload fails) → choose the port → **Upload** →
*Tools → Serial Monitor* at **115200 baud**.

**arduino-cli:**

```bash
arduino-cli core install arduino:avr
arduino-cli compile --fqbn arduino:avr:uno hardware/arduino/cipher_bench
arduino-cli upload  -p /dev/ttyUSB0 --fqbn arduino:avr:uno hardware/arduino/cipher_bench   # Nano: arduino:avr:nano:cpu=atmega328old
arduino-cli monitor -p /dev/ttyUSB0 -c baudrate=115200
```

The port is `/dev/ttyACM0` on most Uno boards and `/dev/ttyUSB0` on most Nano clones.

## What you should see

```
BENCH_V1 F_CPU=16000000
selftest,speck_c,PASS
selftest,speck_asm,PASS
selftest,present_naive,PASS
selftest,present_opt,PASS
selftest,chacha_c,PASS
selftest,chacha_fast,PASS
variant,enc_cycles_per_byte
speck_c,...
...
DONE
```

* **Every `selftest` line must say `PASS`.** The self-tests run the official test vectors (SPECK paper,
  PRESENT paper, RFC 7539) *on the board*, including the assembly code. A `FAIL` means the numbers
  of that variant are meaningless.
* The speed lines are **encrypt cycles per byte** (lower is better), measured over 128 bytes.
* The board resets when the serial monitor opens, so the sketch runs again each time; it waits 1.5 s
  before printing to avoid garbage characters on the first line.

## How the measurement works

* 128 bytes are encrypted by each variant (16 blocks of 8 bytes, or 2 blocks of 64 bytes for ChaCha20).
* The kernel is repeated 1, 5 or 20 times so that a run lasts well over 1 ms (`micros()` ticks every 4 µs).
* Each variant is timed 3 times and the **fastest** trial is reported (filters out interrupt noise).
* `cycles/byte = microseconds × (F_CPU / 1e6) / (repetitions × 128)`.

### Accuracy and caveats

* The Arduino IDE builds with **`-Os`**; the simulator branch uses **`-O2`**. Compiled C variants
  (SPECK C, ChaCha20, PRESENT) can therefore differ between the two; the hand-written assembly does not.
* The Timer0 interrupt (used by `millis()`) steals roughly 0.5 % of the cycles. Taking the fastest of 3 trials
  removes most of it.
* Only encryption is timed here; key setup, decryption and Flash/RAM are measured in the simulator branch
  (`make bench`). Stack usage is not measured anywhere yet.
* Rely on the **ranking and the order of magnitude**, and take exact values from the simulator.

## Recording results

Copy the serial output into `results/<board>.md` together with: board model, clock, IDE or
`arduino-cli` version, compiler flags, and the git commit (`git rev-parse --short HEAD`).
`results/first_run.md` is the template.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `speck.h: No such file or directory` | the sketch folder is empty: run the sync script; the folder name must equal the `.ino` name |
| `Permission denied` on `/dev/ttyUSB0` | `sudo usermod -aG dialout $USER`, log out and in |
| `stk500_getsync(): not in sync` | wrong board or port; on a Nano choose *ATmega328P (Old Bootloader)* |
| garbage on the first serial line | normal after a reset; the sketch already waits 1.5 s, use 115200 baud |
| a `selftest` says `FAIL` | do not use that variant's numbers; run `make check` and `make bench` in the main project |

## Open questions / next steps

* `chacha_c` and `chacha_fast` gave **identical** times in the first run. Likely the compiler turned both rotation
  styles into the same code. Check with `avr-objdump -d` or compare the two variants in the simulator (`make bench`).
* Compare the board numbers with `results/results.md` from the main branch and explain any difference
  larger than a few percent (optimisation level first).
* Optional: cycle-exact timing with Timer1 (prescaler 1) or a GPIO pulse measured with a logic analyser.
