#!/usr/bin/env python3
"""Collects simavr cycle counts + avr-size output + security data into results/.

Inputs (made by `make bench`):  build/<variant>/run.txt, build/<variant>/size.txt
Output:  results/results.csv, results/results.md, results/cpb.png, results/tradeoff.png
"""
import csv
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
BUILD = os.path.join(ROOT, "build")
OUT = os.path.join(ROOT, "results")
NBYTES = 128  # bytes processed in every timed phase (bench/main.c)

# variant -> (cipher family for security.csv, label)
VARIANTS = [
    ("speck_c",       "speck",   "SPECK64/128 (C)"),
    ("speck_asm",     "speck",   "SPECK64/128 (asm)"),
    ("present_naive", "present", "PRESENT-80 (naive)"),
    ("present_opt",   "present", "PRESENT-80 (tables+LPM)"),
    ("chacha_c",      "chacha",  "ChaCha20 (C)"),
    ("chacha_fast",   "chacha",  "ChaCha20 (byte rotations)"),
]


def read(path):
    with open(path) as f:
        return f.read()


def parse_run(variant):
    txt = read(os.path.join(BUILD, variant, "run.txt"))
    m = re.search(r"^RESULT (.*)$", txt, re.M)
    if not m:
        sys.exit("no RESULT line in build/%s/run.txt" % variant)
    return {k: int(v) for k, v in (kv.split("=") for kv in m.group(1).split())}


def parse_size(variant):
    """avr-size (berkeley format): text data bss dec hex filename -> (flash, ram)."""
    txt = read(os.path.join(BUILD, variant, "size.txt"))
    for line in txt.splitlines():
        t = line.split()
        if len(t) >= 4 and t[0].isdigit():
            text, data, bss = int(t[0]), int(t[1]), int(t[2])
            return text + data, data + bss
    sys.exit("cannot parse build/%s/size.txt" % variant)


def load_security():
    sec = {}
    with open(os.path.join(ROOT, "data", "security.csv"), newline="") as f:
        for row in csv.DictReader(f):
            total, att = int(row["total_rounds"]), int(row["best_attack_rounds"])
            row["margin_pct"] = 100.0 * (total - att) / total
            sec[row["cipher"]] = row
    return sec


def main():
    if not os.path.isdir(BUILD):
        sys.exit("run `make bench` first")
    base_flash, base_ram = parse_size("none")
    sec = load_security()
    rows = []
    for v, fam, label in VARIANTS:
        r = parse_run(v)
        flash, ram = parse_size(v)
        s = sec[fam]
        enc = r["enc"] / NBYTES
        dec = r["dec"] / NBYTES
        rows.append({
            "variant": v,
            "label": label,
            "enc_cpb": enc,
            "dec_cpb": dec,
            "setup_cycles": r["setup"],
            "flash_bytes": flash - base_flash,
            "ram_bytes": ram - base_ram,
            "tests_ok": "yes" if r["flags"] == 3 else "NO (flags=%d)" % r["flags"],
            "rounds": "%s/%s" % (int(s["total_rounds"]) - int(s["best_attack_rounds"]), s["total_rounds"]),
            "margin_pct": s["margin_pct"],
            # higher = better: percent of security margin per 100 cycles/byte
            "balance": s["margin_pct"] / (enc / 100.0),
        })

    os.makedirs(OUT, exist_ok=True)
    cols = ["variant", "label", "enc_cpb", "dec_cpb", "setup_cycles", "flash_bytes", "ram_bytes",
            "tests_ok", "margin_pct", "balance"]
    with open(os.path.join(OUT, "results.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols, extrasaction="ignore")
        w.writeheader()
        for r in rows:
            w.writerow({k: (round(r[k], 2) if isinstance(r[k], float) else r[k]) for k in cols})

    rows_sorted = sorted(rows, key=lambda r: r["enc_cpb"])
    md = ["# Results: ATmega328P @ 16 MHz, simavr cycle counts, %d bytes per phase" % NBYTES, "",
          "| Variant | Encrypt c/B | Decrypt c/B | Key setup (cycles) | Flash (B) | RAM (B) | Self-test | "
          "Unattacked rounds | Security margin | Balance* |",
          "|---|---:|---:|---:|---:|---:|---|---|---:|---:|"]
    for r in rows_sorted:
        md.append("| %s | %.1f | %.1f | %d | %d | %d | %s | %s | %.0f %% | %.2f |" % (
            r["label"], r["enc_cpb"], r["dec_cpb"], r["setup_cycles"], r["flash_bytes"], r["ram_bytes"],
            r["tests_ok"], r["rounds"], r["margin_pct"], r["balance"]))
    best = max(rows, key=lambda r: r["balance"])
    fastest = rows_sorted[0]
    md += ["",
           "*Balance = security margin (%) / (encrypt cycles per byte / 100). Higher is better. "
           "It is only one possible way to combine the two metrics - discuss it in the report.",
           "",
           "Security margin = (total rounds - rounds of the best published attack) / total rounds, "
           "taken from data/security.csv (edit that file with the latest literature).",
           "Flash/RAM = firmware size minus the empty baseline firmware; stack usage is not included.",
           "",
           "Fastest variant: **%s** (%.1f c/B). Best balance: **%s**." % (fastest["label"], fastest["enc_cpb"], best["label"])]
    with open(os.path.join(OUT, "results.md"), "w") as f:
        f.write("\n".join(md) + "\n")
    print("\n".join(md))

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("\n(matplotlib not installed: plots skipped)")
        return

    colors = {"speck": "#1f77b4", "present": "#d62728", "chacha": "#2ca02c"}
    fam = {v: f for v, f, _ in VARIANTS}

    fig, ax = plt.subplots(figsize=(8, 4.5))
    names = [r["label"] for r in rows_sorted]
    ax.barh(names, [r["enc_cpb"] for r in rows_sorted], color=[colors[fam[r["variant"]]] for r in rows_sorted])
    ax.set_xlabel("encrypt, clock cycles per byte (lower is better)")
    ax.set_xscale("log")
    ax.invert_yaxis()
    for i, r in enumerate(rows_sorted):
        ax.text(r["enc_cpb"], i, " %.0f" % r["enc_cpb"], va="center")
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "cpb.png"), dpi=150)

    fig, ax = plt.subplots(figsize=(7, 5))
    for r in rows:
        ax.scatter(r["enc_cpb"], r["margin_pct"], s=80, color=colors[fam[r["variant"]]])
        ax.annotate(r["label"], (r["enc_cpb"], r["margin_pct"]), textcoords="offset points", xytext=(6, 4), fontsize=8)
    ax.set_xscale("log")
    ax.set_xlabel("encrypt, cycles per byte (log scale, left = faster)")
    ax.set_ylabel("security margin, % of rounds not reached by attacks (up = safer)")
    ax.set_title("Speed vs security margin, ATmega328P")
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, "tradeoff.png"), dpi=150)
    print("\nplots: results/cpb.png, results/tradeoff.png")


if __name__ == "__main__":
    main()
