# Results: ATmega328P @ 16 MHz, simavr cycle counts, 128 bytes per phase

| Variant | Encrypt c/B | Decrypt c/B | Key setup (cycles) | Flash (B) | RAM (B) | Self-test | Unattacked rounds | Security margin | Balance* |
|---|---:|---:|---:|---:|---:|---|---|---:|---:|
| SPECK64/128 (asm) | 128.9 | 139.5 | 8241 | 1544 | 108 | yes | 7/27 | 26 % | 20.11 |
| ChaCha20 (C) | 757.8 | 757.8 | 229 | 4202 | 44 | yes | 13/20 | 65 % | 8.58 |
| ChaCha20 (byte rotations) | 757.8 | 757.8 | 229 | 4202 | 44 | yes | 13/20 | 65 % | 8.58 |
| SPECK64/128 (C) | 855.2 | 885.2 | 8241 | 866 | 108 | yes | 7/27 | 26 % | 3.03 |
| PRESENT-80 (tables+LPM) | 1769.7 | 2702.2 | 27217 | 3170 | 256 | yes | 5/31 | 16 % | 0.91 |
| PRESENT-80 (naive) | 22468.5 | 22297.7 | 27217 | 1240 | 256 | yes | 5/31 | 16 % | 0.07 |

*Balance = security margin (%) / (encrypt cycles per byte / 100). Higher is better. It is only one possible way to combine the two metrics - discuss it in the report.

Security margin = (total rounds - rounds of the best published attack) / total rounds, taken from data/security.csv (edit that file with the latest literature).
Flash/RAM = firmware size minus the empty baseline firmware; stack usage is not included.

Fastest variant: **SPECK64/128 (asm)** (128.9 c/B). Best balance: **SPECK64/128 (asm)**.
