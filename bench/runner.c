/* simavr cycle tracer: runs an ATmega328P firmware and prints the cycle count of every
 * timed phase.  Usage: ./runner firmware.elf
 * Output line:  RESULT setup=<cycles> enc=<cycles> dec=<cycles> flags=<n> total=<cycles>
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "sim_avr.h"
#include "sim_elf.h"

#define GPIOR0_ADDR 0x3E        /* data-space address of GPIOR0 (I/O 0x1E + 0x20) */
#define GPIOR1_ADDR 0x4A        /* data-space address of GPIOR1 (I/O 0x2A + 0x20) */
#define MAX_STEPS   2000000000ULL

int main(int argc, char **argv)
{
    elf_firmware_t f;
    avr_t *avr;
    uint64_t at[8] = { 0 };
    uint8_t last = 0;
    unsigned long long steps = 0;
    int state;

    if (argc < 2) {
        fprintf(stderr, "usage: %s firmware.elf\n", argv[0]);
        return 2;
    }
    memset(&f, 0, sizeof f);
    if (elf_read_firmware(argv[1], &f) != 0) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    avr = avr_make_mcu_by_name("atmega328p");
    if (!avr) {
        fprintf(stderr, "simavr does not know atmega328p\n");
        return 2;
    }
    avr_init(avr);
    avr_load_firmware(avr, &f);
    avr->frequency = 16000000;

    for (;;) {
        state = avr_run(avr);                       /* executes one instruction */
        if (state == cpu_Done || state == cpu_Crashed)
            break;
        uint8_t m = avr->data[GPIOR1_ADDR];
        if (m != last) {                            /* phase marker changed */
            if (m < 8)
                at[m] = avr->cycle;
            last = m;
        }
        if (++steps > MAX_STEPS) {
            fprintf(stderr, "timeout: firmware did not finish\n");
            return 3;
        }
    }
    printf("RESULT setup=%llu enc=%llu dec=%llu flags=%u total=%llu\n",
           (unsigned long long)(at[2] - at[1]),
           (unsigned long long)(at[4] - at[3]),
           (unsigned long long)(at[6] - at[5]),
           (unsigned)avr->data[GPIOR0_ADDR],
           (unsigned long long)avr->cycle);
    return state == cpu_Crashed ? 1 : 0;
}
