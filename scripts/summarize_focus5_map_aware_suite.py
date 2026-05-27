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


def pct_gain(base, cand):
    if base is None or cand is None or base == 0:
        return None
    return (base - cand) * 100.0 / base


def p95(vals):
    if not vals:
        return None
    s = sorted(vals)
    return s[int(0.95 * (len(s) - 1))]


def main():
    if len(sys.argv) != 2:
        print("Usage: summarize_focus5_map_aware_suite.py <raw_csv>", file=sys.stderr)
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

    configs = sorted({r["config"] for r in rows if r["config"] != "baseline_x00"})

    out_csv = raw_csv.with_name(raw_csv.stem + "_mapwise_vs_baseline.csv")
    out_md = raw_csv.with_name(raw_csv.stem + "_summary.md")

    mapwise = []
    for cfg in configs:
        for m, n in maps:
            b = [r for r in rows if r["config"] == "baseline_x00" and r["map"] == m and int(r["agents"]) == n]
            c = [r for r in rows if r["config"] == cfg and r["map"] == m and int(r["agents"]) == n]

            b_solved = sum(int(r["solved"]) for r in b)
            c_solved = sum(int(r["solved"]) for r in c)

            b_soc = solved_only_avg(b)
            c_soc = solved_only_avg(c)
            soc_gain = pct_gain(b_soc, c_soc)

            b_ct = [float(r["comp_time"]) for r in b]
            c_ct = [float(r["comp_time"]) for r in c]

            b_med = statistics.median(b_ct) if b_ct else None
            c_med = statistics.median(c_ct) if c_ct else None
            med_gain = pct_gain(b_med, c_med)

            b_p95 = p95(b_ct)
            c_p95 = p95(c_ct)
            p95_gain = pct_gain(b_p95, c_p95)

            mapwise.append(
                {
                    "config": cfg,
                    "map": m,
                    "agents": n,
                    "baseline_solved": f"{b_solved}/{len(b)}" if b else "0/0",
                    "candidate_solved": f"{c_solved}/{len(c)}" if c else "0/0",
                    "soc_gain_pct_vs_baseline": "" if soc_gain is None else f"{soc_gain:.2f}",
                    "comp_median_gain_pct": "" if med_gain is None else f"{med_gain:.2f}",
                    "comp_p95_gain_pct": "" if p95_gain is None else f"{p95_gain:.2f}",
                }
            )

    with out_csv.open("w", newline="") as f:
        w = csv.DictWriter(
            f,
            fieldnames=[
                "config",
                "map",
                "agents",
                "baseline_solved",
                "candidate_solved",
                "soc_gain_pct_vs_baseline",
                "comp_median_gain_pct",
                "comp_p95_gain_pct",
            ],
        )
        w.writeheader()
        w.writerows(mapwise)

    ranking = []
    for cfg in configs:
        rows_cfg = [r for r in mapwise if r["config"] == cfg]
        gains = [float(r["soc_gain_pct_vs_baseline"]) for r in rows_cfg if r["soc_gain_pct_vs_baseline"]]
        meds = [float(r["comp_median_gain_pct"]) for r in rows_cfg if r["comp_median_gain_pct"]]
        p95s = [float(r["comp_p95_gain_pct"]) for r in rows_cfg if r["comp_p95_gain_pct"]]
        solved_guard = all(
            int(r["candidate_solved"].split("/")[0]) >= int(r["baseline_solved"].split("/")[0])
            for r in rows_cfg
        )
        worst_map = min(gains) if gains else -1e9
        ranking.append(
            {
                "config": cfg,
                "solved_guard": solved_guard,
                "mean_soc_gain": sum(gains) / len(gains) if gains else None,
                "worst_map_soc_gain": worst_map if gains else None,
                "mean_median_ct_gain": sum(meds) / len(meds) if meds else None,
                "mean_p95_ct_gain": sum(p95s) / len(p95s) if p95s else None,
            }
        )

    ranking.sort(
        key=lambda r: (
            0 if r["solved_guard"] else 1,
            -(r["worst_map_soc_gain"] if r["worst_map_soc_gain"] is not None else -1e9),
            -(r["mean_soc_gain"] if r["mean_soc_gain"] is not None else -1e9),
            -(r["mean_median_ct_gain"] if r["mean_median_ct_gain"] is not None else -1e9),
        )
    )

    with out_md.open("w") as f:
        f.write("# lacam0 Focus5 Map-Aware Suite\n\n")
        f.write(f"- Raw CSV: `{raw_csv}`\n")
        f.write(f"- Mapwise CSV: `{out_csv}`\n\n")
        f.write("## Aggregate Ranking (solved guard, worst-map SOC, mean SOC, runtime)\n\n")
        f.write("| Rank | Config | Solved Guard | Worst Map SOC Gain % | Mean SOC Gain % | Mean Median-CT Gain % | Mean P95-CT Gain % |\n")
        f.write("|---:|---|---|---:|---:|---:|---:|\n")
        for i, r in enumerate(ranking, start=1):
            f.write(
                f"| {i} | {r['config']} | {'yes' if r['solved_guard'] else 'no'} | "
                f"{r['worst_map_soc_gain']:.2f} | {r['mean_soc_gain']:.2f} | "
                f"{r['mean_median_ct_gain']:.2f} | {r['mean_p95_ct_gain']:.2f} |\n"
            )

        f.write("\n## Mapwise SOC Gain %\n\n")
        f.write("| Config | random | den520d | Paris | wh-2-1 | wh-2-2 |\n")
        f.write("|---|---:|---:|---:|---:|---:|\n")
        for cfg in configs:
            by_map = {r["map"]: r for r in mapwise if r["config"] == cfg}
            f.write(
                "| " + cfg + " | "
                + (by_map.get("random-32-32-20.map", {}).get("soc_gain_pct_vs_baseline") or "NA") + " | "
                + (by_map.get("den520d.map", {}).get("soc_gain_pct_vs_baseline") or "NA") + " | "
                + (by_map.get("Paris_1_256.map", {}).get("soc_gain_pct_vs_baseline") or "NA") + " | "
                + (by_map.get("warehouse-20-40-10-2-1.map", {}).get("soc_gain_pct_vs_baseline") or "NA") + " | "
                + (by_map.get("warehouse-20-40-10-2-2.map", {}).get("soc_gain_pct_vs_baseline") or "NA")
                + " |\n"
            )

    print(f"Wrote {out_csv}")
    print(f"Wrote {out_md}")


if __name__ == "__main__":
    main()
