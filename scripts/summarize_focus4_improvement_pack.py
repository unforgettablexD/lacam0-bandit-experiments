#!/usr/bin/env python3
import argparse
import csv
import math
import statistics
from collections import defaultdict
from pathlib import Path


def to_float(value, default=0.0):
    try:
        return float(value)
    except Exception:
        return default


def to_int(value, default=0):
    try:
        return int(float(value))
    except Exception:
        return default


def pct(sorted_vals, p):
    if not sorted_vals:
        return None
    if len(sorted_vals) == 1:
        return sorted_vals[0]
    k = (len(sorted_vals) - 1) * p
    lo = int(math.floor(k))
    hi = int(math.ceil(k))
    if lo == hi:
        return sorted_vals[lo]
    return sorted_vals[lo] + (sorted_vals[hi] - sorted_vals[lo]) * (k - lo)


def solved_only_avg_soc(rows):
    vals = [to_float(r["soc"]) for r in rows if to_int(r["solved"]) == 1 and to_float(r["soc"]) > 0]
    if not vals:
        return None
    return sum(vals) / len(vals)


def summarize_group(rows):
    n = len(rows)
    solved = sum(to_int(r["solved"]) for r in rows)
    solved_rate = (solved / n) if n else 0.0
    soc_avg_solved = solved_only_avg_soc(rows)
    comp_times = sorted(to_float(r["comp_time"]) for r in rows)
    comp_avg = (sum(comp_times) / len(comp_times)) if comp_times else None
    comp_median = statistics.median(comp_times) if comp_times else None
    comp_p95 = pct(comp_times, 0.95)
    return {
        "n": n,
        "solved": solved,
        "solved_rate": solved_rate,
        "soc_avg_solved": soc_avg_solved,
        "comp_avg": comp_avg,
        "comp_median": comp_median,
        "comp_p95": comp_p95,
    }


def fmt_num(value, nd=2):
    if value is None:
        return ""
    return f"{value:.{nd}f}"


def main():
    parser = argparse.ArgumentParser(description="Summarize lacam0 improvement pack results")
    parser.add_argument("raw_csv", help="Raw csv from run_focus4_improvement_pack.sh")
    args = parser.parse_args()

    raw_csv = Path(args.raw_csv).resolve()
    if not raw_csv.exists():
        raise SystemExit(f"missing file: {raw_csv}")

    out_mapwise = raw_csv.with_name(raw_csv.stem + "_mapwise.csv")
    out_summary = raw_csv.with_name(raw_csv.stem + "_summary.md")

    rows = list(csv.DictReader(raw_csv.open()))
    if not rows:
        raise SystemExit("raw csv has no rows")

    group = defaultdict(list)
    for r in rows:
        gk = (
            r["track"],
            r["profile"],
            r["policy"],
            r["regret_trials"],
            r["reward_autoscale"],
            r["reward_weight_learning"],
            r["time_limit"],
            r["map"],
            r["agents"],
        )
        group[gk].append(r)

    group_stats = {k: summarize_group(v) for k, v in group.items()}

    baseline_lookup = {}
    for k, stats in group_stats.items():
        (
            track,
            profile,
            policy,
            regret,
            autos,
            learn,
            time_limit,
            map_name,
            agents,
        ) = k
        if profile == "baseline_x00":
            baseline_lookup[(track, time_limit, map_name, agents)] = stats

    mapwise_rows = []
    for k, stats in sorted(group_stats.items()):
        (
            track,
            profile,
            policy,
            regret,
            autos,
            learn,
            time_limit,
            map_name,
            agents,
        ) = k
        base = baseline_lookup.get((track, time_limit, map_name, agents))
        if base is None:
            continue

        soc_gain = None
        if base["soc_avg_solved"] and stats["soc_avg_solved"]:
            soc_gain = (base["soc_avg_solved"] - stats["soc_avg_solved"]) * 100.0 / base["soc_avg_solved"]

        median_ct_gain = None
        if base["comp_median"] and stats["comp_median"]:
            median_ct_gain = (base["comp_median"] - stats["comp_median"]) * 100.0 / base["comp_median"]

        p95_ct_gain = None
        if base["comp_p95"] and stats["comp_p95"]:
            p95_ct_gain = (base["comp_p95"] - stats["comp_p95"]) * 100.0 / base["comp_p95"]

        mapwise_rows.append(
            {
                "track": track,
                "time_limit": time_limit,
                "profile": profile,
                "policy": policy,
                "regret_trials": regret,
                "reward_autoscale": autos,
                "reward_weight_learning": learn,
                "map": map_name,
                "agents": agents,
                "baseline_solved": f"{base['solved']}/{base['n']}",
                "candidate_solved": f"{stats['solved']}/{stats['n']}",
                "baseline_solved_rate": fmt_num(base["solved_rate"], 3),
                "candidate_solved_rate": fmt_num(stats["solved_rate"], 3),
                "baseline_soc_solved_avg": fmt_num(base["soc_avg_solved"], 2),
                "candidate_soc_solved_avg": fmt_num(stats["soc_avg_solved"], 2),
                "soc_gain_pct_vs_baseline": fmt_num(soc_gain, 2),
                "baseline_comp_median": fmt_num(base["comp_median"], 2),
                "candidate_comp_median": fmt_num(stats["comp_median"], 2),
                "comp_median_gain_pct": fmt_num(median_ct_gain, 2),
                "baseline_comp_p95": fmt_num(base["comp_p95"], 2),
                "candidate_comp_p95": fmt_num(stats["comp_p95"], 2),
                "comp_p95_gain_pct": fmt_num(p95_ct_gain, 2),
            }
        )

    with out_mapwise.open("w", newline="") as f:
        fieldnames = [
            "track",
            "time_limit",
            "profile",
            "policy",
            "regret_trials",
            "reward_autoscale",
            "reward_weight_learning",
            "map",
            "agents",
            "baseline_solved",
            "candidate_solved",
            "baseline_solved_rate",
            "candidate_solved_rate",
            "baseline_soc_solved_avg",
            "candidate_soc_solved_avg",
            "soc_gain_pct_vs_baseline",
            "baseline_comp_median",
            "candidate_comp_median",
            "comp_median_gain_pct",
            "baseline_comp_p95",
            "candidate_comp_p95",
            "comp_p95_gain_pct",
        ]
        w = csv.DictWriter(f, fieldnames=fieldnames)
        w.writeheader()
        w.writerows(mapwise_rows)

    agg = defaultdict(lambda: {
        "maps": 0,
        "solved_guard": True,
        "soc_gain_sum": 0.0,
        "soc_gain_n": 0,
        "median_gain_sum": 0.0,
        "median_gain_n": 0,
        "p95_gain_sum": 0.0,
        "p95_gain_n": 0,
    })

    for r in mapwise_rows:
        if r["profile"] == "baseline_x00":
            continue
        key = (
            r["track"],
            r["time_limit"],
            r["profile"],
            r["policy"],
            r["regret_trials"],
            r["reward_autoscale"],
            r["reward_weight_learning"],
        )
        a = agg[key]
        a["maps"] += 1

        b_solved = int(r["baseline_solved"].split("/")[0])
        c_solved = int(r["candidate_solved"].split("/")[0])
        if c_solved < b_solved:
            a["solved_guard"] = False

        if r["soc_gain_pct_vs_baseline"]:
            a["soc_gain_sum"] += to_float(r["soc_gain_pct_vs_baseline"])
            a["soc_gain_n"] += 1
        if r["comp_median_gain_pct"]:
            a["median_gain_sum"] += to_float(r["comp_median_gain_pct"])
            a["median_gain_n"] += 1
        if r["comp_p95_gain_pct"]:
            a["p95_gain_sum"] += to_float(r["comp_p95_gain_pct"])
            a["p95_gain_n"] += 1

    ranked = []
    for k, a in agg.items():
        mean_soc = (a["soc_gain_sum"] / a["soc_gain_n"]) if a["soc_gain_n"] else -1e9
        mean_median = (a["median_gain_sum"] / a["median_gain_n"]) if a["median_gain_n"] else -1e9
        mean_p95 = (a["p95_gain_sum"] / a["p95_gain_n"]) if a["p95_gain_n"] else -1e9
        ranked.append((a["solved_guard"], mean_soc, mean_median, mean_p95, a["maps"], k))

    ranked.sort(key=lambda x: (x[0], x[1], x[2], x[3]), reverse=True)

    with out_summary.open("w") as f:
        f.write("# lacam0 Focus4 Improvement Pack\n\n")
        f.write(f"- Raw CSV: `{raw_csv}`\n")
        f.write(f"- Mapwise CSV: `{out_mapwise}`\n")
        f.write("\n")
        f.write("## Ranking (solved guard first, then SOC gain, then runtime gains)\n\n")
        f.write("| Rank | Track | Time | Profile | Policy | Regret | Autoscale | Learning | Solved Guard | Mean SOC Gain % | Mean Median-CT Gain % | Mean P95-CT Gain % | Maps |\n")
        f.write("|---:|---|---:|---|---|---:|---|---|---|---:|---:|---:|---:|\n")
        for i, (guard, mean_soc, mean_median, mean_p95, maps, k) in enumerate(ranked[:25], start=1):
            track, tl, profile, policy, regret, autos, learn = k
            f.write(
                f"| {i} | {track} | {tl} | {profile} | {policy} | {regret} | {autos} | {learn} | "
                f"{'yes' if guard else 'no'} | {mean_soc:.2f} | {mean_median:.2f} | {mean_p95:.2f} | {maps} |\n"
            )

        f.write("\n## Notes\n\n")
        f.write("- Positive gain means candidate is better than baseline (lower SOC/comp_time).\n")
        f.write("- Solved guard requires candidate solved count to be >= baseline on every map included in that config.\n")

    print(f"Wrote {out_mapwise}")
    print(f"Wrote {out_summary}")


if __name__ == "__main__":
    main()
