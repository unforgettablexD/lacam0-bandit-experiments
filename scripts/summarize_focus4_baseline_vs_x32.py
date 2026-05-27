#!/usr/bin/env python3
import csv
import sys
from pathlib import Path


def solved_only_avg(rows):
    vals = [float(r["soc"]) for r in rows if int(r["solved"]) == 1]
    if not vals:
        return None
    return sum(vals) / len(vals)


def main():
    if len(sys.argv) != 2:
        print("Usage: summarize_focus4_baseline_vs_x32.py <raw_csv>", file=sys.stderr)
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
        c = [r for r in rows if r["config"] == "thompson_x32" and r["map"] == m and int(r["agents"]) == n]
        bsolved = sum(int(r["solved"]) for r in b)
        csolved = sum(int(r["solved"]) for r in c)
        bavg = solved_only_avg(b)
        cavg = solved_only_avg(c)
        gain = None
        if bavg and cavg:
            gain = (bavg - cavg) * 100.0 / bavg
        out_rows.append({
            "map": m,
            "agents": n,
            "baseline_solved": f"{bsolved}/{len(b)}" if b else "0/0",
            "candidate_solved": f"{csolved}/{len(c)}" if c else "0/0",
            "baseline_avg_soc_solved_only": "" if bavg is None else f"{bavg:.2f}",
            "candidate_avg_soc_solved_only": "" if cavg is None else f"{cavg:.2f}",
            "soc_gain_pct_vs_baseline": "" if gain is None else f"{gain:.2f}",
        })

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
            ],
        )
        w.writeheader()
        w.writerows(out_rows)

    with out_md.open("w") as f:
        f.write("# lacam0 Focus4 Baseline vs Thompson+X32\n\n")
        f.write(f"- Raw CSV: `{raw_csv}`\n")
        f.write(f"- Mapwise CSV: `{out_csv}`\n\n")
        f.write("| Map | Agents | Baseline Solved | Thompson+X32 Solved | Baseline Avg SOC (Solved-Only) | Thompson+X32 Avg SOC (Solved-Only) | SOC Gain vs Baseline |\n")
        f.write("|---|---:|---:|---:|---:|---:|---:|\n")
        for r in out_rows:
            f.write(
                f"| {r['map'].replace('.map','')} | {r['agents']} | {r['baseline_solved']} | {r['candidate_solved']} | "
                f"{r['baseline_avg_soc_solved_only'] or 'NA'} | {r['candidate_avg_soc_solved_only'] or 'NA'} | "
                f"{(r['soc_gain_pct_vs_baseline'] + '%') if r['soc_gain_pct_vs_baseline'] else 'NA'} |\n"
            )

    print(f"Wrote {out_csv}")
    print(f"Wrote {out_md}")


if __name__ == "__main__":
    main()
