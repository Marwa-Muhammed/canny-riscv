#!/usr/bin/env python3
"""
Parse the stdout of canny_rv_embedded.
Hot-path breakdown now shown as a clean table.
"""

import re
import sys

QEMU_HZ      = 1_000_000_000
CYCLES_TO_MS = 1000.0 / QEMU_HZ

PIPELINE_RE  = re.compile(r"=== Pipeline:\s*(.*?)\s*===")
STAGE_RE     = re.compile(
    r"^\s+(Stage\s+\S+:\s+[A-Za-z0-9 /]+?)\s*:\s*([0-9]+(?:\.[0-9]+)?)\s*"
    r"([A-Za-z][A-Za-z0-9_]*(?:\s+[A-Za-z][A-Za-z0-9_]*)*)"
    r"\s*(?:\([^)]*\))?\s*\(avg over \d+ runs\)"
)

PIPE_TOT_MS_RE  = re.compile(r"(L[12])\s+total time\s*:\s*([0-9]+\.[0-9]+)\s*ms")
PIPE_SINGLE_RE  = re.compile(r"(L[12])\s+single pass\s*:\s*([0-9]+\.[0-9]+)\s*ms")


def parse(log_path):
    pipeline    = None
    pipelines   = {}
    pipe_totals = {}

    with open(log_path, "r", errors="replace") as f:
        for line in f:
            pm = PIPELINE_RE.search(line)
            if pm:
                pipeline = pm.group(1).strip()
                if pipeline not in pipelines:
                    pipelines[pipeline] = []
                continue

            sm = STAGE_RE.match(line)
            if sm and pipeline:
                stage = sm.group(1).strip()
                value = float(sm.group(2))
                unit  = sm.group(3).lower().strip()
                if unit.startswith("clock_gettime"):
                    avg_ms = value / 1_000_000.0   # ns → ms
                elif unit.startswith(("rdtime", "rdcycle")):
                    avg_ms = value * CYCLES_TO_MS  # cycles → ms at 1GHz QEMU
                else:
                    avg_ms = value

                pipelines[pipeline].append((stage, avg_ms))
                continue

            m = PIPE_TOT_MS_RE.search(line)
            if m:
                key = m.group(1)
                pipe_totals.setdefault(key, {})['total_ms'] = float(m.group(2))
                continue

            m = PIPE_SINGLE_RE.search(line)
            if m:
                key = m.group(1)
                pipe_totals.setdefault(key, {})['single_ms'] = float(m.group(2))
                continue

    return pipelines, pipe_totals


# ── Helpers ─────────────────────────────────────────────────────────────

def _pipe_key(name):
    if name.startswith("L1"): return "L1"
    if name.startswith("L2"): return "L2"
    return name


def _short_stage_name(stage):
    return re.sub(r"^Stage\s+\S+:\s*", "", stage).strip()


def _pct_breakdown(rows, label="Hot-path breakdown"):
    total = sum(ms for _, ms in rows)
    if total <= 0 or len(rows) < 2:
        return

    print(f"\n{label}")
    print("┌────────────────────────────┬────────┐")
    print("│ Stage                      │  %     │")
    print("├────────────────────────────┼────────┤")

    for stage, ms in rows:
        pct = 100.0 * ms / total
        short = _short_stage_name(stage)
        print(f"│ {short:<26} │ {pct:5.1f}% │")

    print("└────────────────────────────┴────────┘")


def _totals_block_combined(opt_level, pipe_name, rv_totals, host_totals):
    key  = _pipe_key(pipe_name)
    rv_t = rv_totals.get(key, {})
    hs_t = host_totals.get(key, {})

    def fmt_ms(d, k):
        return f"{d[k]:.3f}" if k in d else "N/A"

    print(f"\n{pipe_name} Runtime [{opt_level}]")
    print("┌────────────────────────────────────────────────────────────┐")
    print(f"│ {'':<12} {'RISC-V':>12}   {'Host':>12}          │")
    print("├────────────────────────────────────────────────────────────┤")
    print(f"│ Total time   {fmt_ms(rv_t,'total_ms'):>12} ms   {fmt_ms(hs_t,'total_ms'):>12} ms    │")
    print(f"│ Single pass  {fmt_ms(rv_t,'single_ms'):>12} ms   {fmt_ms(hs_t,'single_ms'):>12} ms    │")
    print("└────────────────────────────────────────────────────────────┘")


def print_combined_table(opt_level, rv_log, host_log):
    rv_pipes,   rv_totals   = parse(rv_log)   if rv_log   else ({}, {})
    host_pipes, host_totals = parse(host_log) if host_log else ({}, {})

    rv_names   = list(rv_pipes.keys())
    host_names = list(host_pipes.keys())

    for i in range(max(len(rv_names), len(host_names))):
        rv_name   = rv_names[i]   if i < len(rv_names)   else None
        host_name = host_names[i] if i < len(host_names) else None
        pipe_label = rv_name or host_name or "Unknown"

        rv_rows   = rv_pipes.get(rv_name,    []) if rv_name   else []
        host_rows = host_pipes.get(host_name,[]) if host_name else []

        print(f"\n{'='*70}")
        print(f"Pipeline: {pipe_label}")
        print('='*70)

        st_w = max(
            max((len(s) for s, _ in rv_rows),   default=5),
            max((len(s) for s, _ in host_rows), default=5),
            len("Stage")
        )

        sep = f"  +{'-'*8}+{'-'*(st_w+2)}+{'-'*14}+{'-'*14}+"
        hdr = f"  | {'Opt':<6} | {'Stage':<{st_w}} | {'RISC-V ms':>12} | {'Host ms':>12} |"

        print(sep)
        print(hdr)
        print(sep)

        n = max(len(rv_rows), len(host_rows))
        for i in range(n):
            stage   = rv_rows[i][0]   if i < len(rv_rows)   else (host_rows[i][0] if i < len(host_rows) else "")
            rv_ms   = f"{rv_rows[i][1]:>12.4f}"   if i < len(rv_rows)   else f"{'N/A':>12}"
            host_ms = f"{host_rows[i][1]:>12.4f}" if i < len(host_rows) else f"{'N/A':>12}"
            print(f"  | {opt_level:<6} | {stage:<{st_w}} | {rv_ms} | {host_ms} |")

        print(sep)

        profile_rows = rv_rows if rv_rows else host_rows
        _pct_breakdown(profile_rows, label=f"Hot-path breakdown [{pipe_label}] (RISC-V)")

        _totals_block_combined(opt_level, pipe_label, rv_totals, host_totals)


def main():
    table_mode    = "--table"    in sys.argv
    combined_mode = "--combined" in sys.argv
    args = [a for a in sys.argv[1:] if a not in ("--table", "--combined")]

    if combined_mode:
        if len(args) != 3:
            print("usage: parse_timing.py <opt_level> <rv_log> <host_log> --combined", file=sys.stderr)
            sys.exit(1)
        opt_level, rv_log, host_log = args
        print_combined_table(opt_level, rv_log, host_log)
        return

    if len(args) != 2:
        print("usage: parse_timing.py <opt_level> <log_path> [--table]", file=sys.stderr)
        sys.exit(1)

    opt_level, log_path = args
    pipelines, pipe_totals = parse(log_path)

    if not pipelines:
        print(f"  !! no Stage lines found in {log_path}", file=sys.stderr)
        sys.exit(0)

    if table_mode:
        for pipe_name, rows in pipelines.items():
            st_w = max((len(s) for s, _ in rows), default=5)
            st_w = max(st_w, len("Stage"))
            sep = f"+{'-'*10}+{'-'*(st_w+2)}+{'-'*12}+"
            hdr = f"| {'Opt':<8} | {'Stage':<{st_w}} | {'avg_ms':>10} |"
            print(f"\n  Pipeline: {pipe_name}")
            print(sep)
            print(hdr)
            print(sep)
            for stage, avg_ms in rows:
                print(f"| {opt_level:<8} | {stage:<{st_w}} | {avg_ms:>10.4f} |")
            print(sep)
            _pct_breakdown(rows, label=f"Hot-path breakdown  [{pipe_name}]")
    else:
        # CSV mode
        for pipe_name, rows in pipelines.items():
            for stage, avg_ms in rows:
                print(f'{opt_level},"{pipe_name}","{stage}",{avg_ms:.4f}')
            key = _pipe_key(pipe_name)
            t   = pipe_totals.get(key, {})
            if 'total_ms' in t:
                print(f'{opt_level},"{pipe_name}","Total time ms",{t["total_ms"]:.4f}')
            if 'single_ms' in t:
                print(f'{opt_level},"{pipe_name}","Single pass ms",{t["single_ms"]:.4f}')


if __name__ == "__main__":
    main()
