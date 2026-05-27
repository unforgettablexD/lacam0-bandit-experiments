#!/usr/bin/env python3
import csv
import statistics
import sys
from pathlib import Path


def solved_only_avg(rows):
    vals = [float(r["soc"]) for r in rows if int(r["solved"]) == 1]
    if not vals:
        return None
    return sum(vals) / len(vals)


def gain_pct(base, cand):
    if base is None or cand is None or base == 0:
        return None
    return (base - cand) * 100.0 / base


def main():
    if len(sys.argv) != 2:
        print("Usage: summarize_focus5_map_aware_vs_baseline.py <raw_csv>", file=sys.stderr)
        sys.exit(1)

    raw_csv = Path(sys.argv[1]).resolve()
    if not raw_csv.exists():
        print(f"missing file: {raw_csv}", file=sys.stderr)
        sys.exit(1)

    rows = list(csv.DictReader(raw_csv.open()))
    maps = [
        ("random-32-32-20.map", 300),
        ("den520d.map", 700),
        ("Paris_1_256.map", 1000),
        ("warehouse-20-40-10-2-1.map", 1000),
        ("warehouse-20-40-10-2-2.map", 1000),
    ]

    out_csv = raw_csv.with_name(raw_csv.stem + "_mapwise_vs_baseline.csv")
    out_md = raw_csv.with_name(raw_csv.stem + "_summary.md")

    out_rows = []
    for m, n in maps:
        b = [r for r in rows if r["config"] == "baseline_x00" and r["map"] == m and int(r["agents"]) == n]
        c = [r for r in rows if r["config"] == "mapaware_x35" and r["map"] == m and int(r["agents"]) == n]

        b_solved = sum(int(r["solved"]) for r in b)
        c_solved = sum(int(r["solved"]) for r in c)

        b_soc = solved_only_avg(b)
        c_soc = solved_only_avg(c)
        soc_gain = gain_pct(b_soc, c_soc)

        b_ct_vals = [float(r["comp_time"]) for r in b]
        c_ct_vals = [float(r["comp_time"]) for r in c]
        b_ct_med = statistics.median(b_ct_vals) if b_ct_vals else None
        c_ct_med = statistics.median(c_ct_vals) if c_ct_vals else None
        ct_med_gain = gain_pct(b_ct_med, c_ct_med)

        b_ct_p95 = None
        c_ct_p95 = None
        if b_ct_vals:
            s = sorted(b_ct_vals)
            b_ct_p95 = s[int(0.95 * (len(s) - 1))]
        if c_ct_vals:
            s = sorted(c_ct_vals)
            c_ct_p95 = s[int(0.95 * (len(s) - 1))]
        ct_p95_gain = gain_pct(b_ct_p95, c_ct_p95)

        out_rows.append(
            {
                "map": m,
                "agents": n,
                "baseline_solved": f"{b_solved}/{len(b)}" if b else "0/0",
                "candidate_solved": f"{c_solved}/{len(c)}" if c else "0/0",
                "baseline_avg_soc_solved_only": "" if b_soc is None else f"{b_soc:.2f}",
                "candidate_avg_soc_solved_only": "" if c_soc is None else f"{c_soc:.2f}",
                "soc_gain_pct_vs_baseline": "" if soc_gain is None else f"{soc_gain:.2f}",
                "baseline_comp_median": "" if b_ct_med is None else f"{b_ct_med:.6f}",
                "candidate_comp_median": "" if c_ct_med is None else f"{c_ct_med:.6f}",
                "comp_median_gain_pct": "" if ct_med_gain is None else f"{ct_med_gain:.2f}",
                "baseline_comp_p95": "" if b_ct_p95 is None else f"{b_ct_p95:.6f}",
                "candidate_comp_p95": "" if c_ct_p95 is None else f"{c_ct_p95:.6f}",
                "comp_p95_gain_pct": "" if ct_p95_gain is None else f"{ct_p95_gain:.2f}",
            }
        )

    with out_csv.open("w", newline="") as f:
        w = csv.DictWriter(
            f,
            fieldnames=[
                "map",
                "agents",
                "baseline_solved",
                "candidate_solved",
                "baseline_avg_soc_solved_only",
                "candidate_avg_soc_solved_only",
                "soc_gain_pct_vs_baseline",
                "baseline_comp_median",
                "candidate_comp_median",
                "comp_median_gain_pct",
                "baseline_comp_p95",
                "candidate_comp_p95",
                "comp_p95_gain_pct",
            ],
        )
        w.writeheader()
        w.writerows(out_rows)

    with out_md.open("w") as f:
        f.write("# lacam0 Focus5 Baseline vs Map-Aware X35\n\n")
        f.write(f"- Raw CSV: `{raw_csv}`\n")
        f.write(f"- Mapwise CSV: `{out_csv}`\n\n")
        f.write("Map-aware policy:\n")
        f.write("- `den520d` and `Paris_1_256`: baseline (all bandits off)\n")
        f.write("- `random-32-32-20` and both `warehouse` maps: X35\n\n")
        f.write("| Map | Agents | Baseline Solved | Map-Aware Solved | Baseline SOC (Solved-Only) | Map-Aware SOC (Solved-Only) | SOC Gain % | Median CT Gain % | P95 CT Gain % |\n")
        f.write("|---|---:|---:|---:|---:|---:|---:|---:|---:|\n")
        for r in out_rows:
            f.write(
                f"| {r['map'].replace('.map', '')} | {r['agents']} | {r['baseline_solved']} | {r['candidate_solved']} | "
                f"{r['baseline_avg_soc_solved_only'] or 'NA'} | {r['candidate_avg_soc_solved_only'] or 'NA'} | "
                f"{(r['soc_gain_pct_vs_baseline'] + '%') if r['soc_gain_pct_vs_baseline'] else 'NA'} | "
                f"{(r['comp_median_gain_pct'] + '%') if r['comp_median_gain_pct'] else 'NA'} | "
                f"{(r['comp_p95_gain_pct'] + '%') if r['comp_p95_gain_pct'] else 'NA'} |\n"
            )

    print(f"Wrote {out_csv}")
    print(f"Wrote {out_md}")


if __name__ == "__main__":
    main()
