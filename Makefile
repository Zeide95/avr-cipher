# Cryptographic co-design on AVR: SPECK / PRESENT / ChaCha20
#
#   make check     unit tests on the PC (CUnit): official test vectors + round trips
#   make bench     build every variant for ATmega328P, run it in simavr, save cycles and sizes
#   make report    table + plots in results/   (runs "bench" first)
#   make all       check + bench + report
#   make clean
#
# Optional:  make OPT=-Os bench      (optimisation level, default -O2)

MCU    ?= atmega328p
F_CPU  ?= 16000000UL
OPT    ?= -O2

AVR_CC  ?= avr-gcc
AVR_SZ  ?= avr-size
AVR_OD  ?= avr-objdump
HOST_CC ?= gcc

# simavr: the Debian/Ubuntu package installs into /usr/include/simavr;
# a source build (make install) goes to /usr/local.
SIMAVR_CFLAGS ?= -I/usr/include/simavr -I/usr/local/include/simavr
SIMAVR_LIBS   ?= -L/usr/local/lib -lsimavr -lelf

AVR_CFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) $(OPT) -std=gnu11 -Wall -Wextra \
             -ffunction-sections -fdata-sections -Isrc
AVR_LDFLAGS = -mmcu=$(MCU) -Wl,--gc-sections

VARIANTS = speck_c speck_asm present_naive present_opt chacha_c chacha_fast

SRC_speck_c       = src/speck.c
SRC_speck_asm     = src/speck.c src/speck_asm.S
SRC_present_naive = src/present_naive.c
SRC_present_opt   = src/present_opt.c
SRC_chacha_c      = src/chacha.c
SRC_chacha_fast   = src/chacha.c
SRC_none          =

DEF_speck_c       = -DVARIANT_SPECK_C
DEF_speck_asm     = -DVARIANT_SPECK_ASM
DEF_present_naive = -DVARIANT_PRESENT_NAIVE
DEF_present_opt   = -DVARIANT_PRESENT_OPT
DEF_chacha_c      = -DVARIANT_CHACHA_C
DEF_chacha_fast   = -DVARIANT_CHACHA_FAST
DEF_none          = -DVARIANT_NONE

HEADERS = $(wildcard src/*.h src/*.inc) src/present_tables.h

.PHONY: all check bench report clean tables help
.DEFAULT_GOAL := help

help:
	@echo "make check   - unit tests on the PC (CUnit)"
	@echo "make bench   - build + run all variants in simavr"
	@echo "make report  - results/results.md, results.csv and plots"
	@echo "make all     - everything"
	@echo "make clean"

all: check bench report

# ---------------------------------------------------------------- tables
tables: src/present_tables.h
src/present_tables.h: tools/gen_tables.py
	python3 tools/gen_tables.py $@

# ---------------------------------------------------------------- host unit tests
build/test_ciphers: tests/test_ciphers.c src/speck.c src/present_opt.c \
                    src/present_naive.c src/chacha.c $(HEADERS) tests/compat/avr/pgmspace.h
	@mkdir -p build
	$(HOST_CC) -O1 -Wall -Wextra -Itests/compat -Isrc tests/test_ciphers.c src/speck.c \
	    src/present_opt.c src/present_naive.c src/chacha.c -o $@ -lcunit

check: build/test_ciphers
	./build/test_ciphers

# ---------------------------------------------------------------- simavr runner (host program)
runner: bench/runner.c
	$(HOST_CC) -O2 -Wall $(SIMAVR_CFLAGS) bench/runner.c -o $@ $(SIMAVR_LIBS)

# ---------------------------------------------------------------- AVR firmware, one set of rules per variant
define VARIANT_RULES
build/$(1)/fw.elf: bench/main.c $$(SRC_$(1)) $$(HEADERS)
	@mkdir -p build/$(1)
	$$(AVR_CC) $$(AVR_CFLAGS) $$(DEF_$(1)) $$(filter %.c %.S,$$^) -o $$@ $$(AVR_LDFLAGS)

build/$(1)/size.elf: bench/main.c $$(SRC_$(1)) $$(HEADERS)
	@mkdir -p build/$(1)
	$$(AVR_CC) $$(AVR_CFLAGS) $$(DEF_$(1)) -DSIZE_ONLY $$(filter %.c %.S,$$^) -o $$@ $$(AVR_LDFLAGS)

build/$(1)/size.txt: build/$(1)/size.elf
	$$(AVR_SZ) $$< > $$@

build/$(1)/run.txt: build/$(1)/fw.elf runner
	./runner $$< | tee $$@
endef
$(foreach v,$(VARIANTS) none,$(eval $(call VARIANT_RULES,$(v))))

bench: $(foreach v,$(VARIANTS) none,build/$(v)/run.txt build/$(v)/size.txt)
	@echo "bench done: see build/<variant>/run.txt and size.txt"

# optional: look at the generated assembly, e.g.  make asm V=chacha_c
asm:
	$(AVR_OD) -d build/$(V)/fw.elf | less

# ---------------------------------------------------------------- report
report: bench
	python3 tools/collect.py

clean:
	rm -rf build runner results
