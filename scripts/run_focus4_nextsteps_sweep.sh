#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$ROOT_DIR/reports/movingai"
mkdir -p "$OUT_DIR"

CPU_COUNT="$(nproc)"
if [[ "$CPU_COUNT" -gt 2 ]]; then
  PARALLEL="$((CPU_COUNT - 2))"
else
  PARALLEL="$CPU_COUNT"
fi

TIME_LIMIT=60
DATA_ROOT="${DATA_ROOT:-$ROOT_DIR/../lacam/benchmarks/movingai/data_all}"
RUN_ID="$(date +%Y%m%d_%H%M%S)_lacam0_focus4_nextsteps_sweep"
RAW_CSV="$OUT_DIR/${RUN_ID}.csv"
MAPWISE_CSV="$OUT_DIR/${RUN_ID}_mapwise.csv"
SUMMARY_MD="$OUT_DIR/${RUN_ID}_summary.md"

usage() {
  cat <<EOF
Usage: $0 [options]
  --parallel N
  --time-limit N
  --data-root PATH
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --parallel) PARALLEL="$2"; shift 2 ;;
    --time-limit) TIME_LIMIT="$2"; shift 2 ;;
    --data-root) DATA_ROOT="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown arg: $1" >&2; usage; exit 1 ;;
  esac
done

if [[ ! -x "$ROOT_DIR/build/main" ]]; then
  echo "Missing binary: $ROOT_DIR/build/main" >&2
  exit 1
fi

echo "variant,config,map,scen,agents,solved,soc,makespan,sum_of_loss,comp_time" > "$RAW_CSV"

run_case() {
  local variant="$1" cfg="$2" map="$3" scen="$4" agents="$5" cargs="$6"
  local out="/tmp/lacam0_next_${RANDOM}_${RANDOM}.txt"
  "$ROOT_DIR/build/main" -m "$map" -i "$scen" -N "$agents" -t "$TIME_LIMIT" -v 0 -o "$out" $cargs >/dev/null || true
  local solved soc mk sol ct
  solved="$(awk -F= '/^solved=/{print $2}' "$out" 2>/dev/null || echo 0)"
  soc="$(awk -F= '/^soc=/{print $2}' "$out" 2>/dev/null || echo 0)"
  mk="$(awk -F= '/^makespan=/{print $2}' "$out" 2>/dev/null || echo 0)"
  sol="$(awk -F= '/^sum_of_loss=/{print $2}' "$out" 2>/dev/null || echo 0)"
  ct="$(awk -F= '/^comp_time=/{print $2}' "$out" 2>/dev/null || echo 0)"
  echo "$variant,$cfg,$(basename "$map"),$(basename "$scen"),$agents,$solved,$soc,$mk,$sol,$ct" >> "$RAW_CSV"
  rm -f "$out"
}

export ROOT_DIR TIME_LIMIT RAW_CSV
export -f run_case

declare -a MAPS=(
  "random-32-32-20.map random-32-32-20-random 300"
  "den520d.map den520d-random 700"
  "Paris_1_256.map Paris_1_256-random 1000"
  "warehouse-20-40-10-2-2.map warehouse-20-40-10-2-2-random 1000"
)

# Baseline
for m in "${MAPS[@]}"; do
  read -r map scenprefix agents <<< "$m"
  full_map="$DATA_ROOT/$map"
  echo "[$(date '+%F %T')] baseline map=$map N=$agents"
  seq 1 25 | xargs -I{} -P "$PARALLEL" bash -lc \
    "run_case 'baseline' 'X00' '$full_map' '$DATA_ROOT/scen-random/${scenprefix}-{}.scen' '$agents' '--no_pibt_bandit --no_order_bandit --no_branch_bandit --no_scheduler_bandit --no_random_bandit --no_dist_bandit'"
done

# Candidate sweeps (all are Thompson + X32-style toggles)
declare -a ORDER_MODES=("coarse3:first_only" "agent_level:first_only" "agent_level:topk")
declare -a AUTOSCALE_MODES=("off" "zscore")
declare -a LEARN_MODES=("off" "online_linear")
declare -a HIER_MODES=("off" "on")

for om in "${ORDER_MODES[@]}"; do
  order_mode="${om%%:*}"
  order_reward="${om##*:}"
  for autoscale in "${AUTOSCALE_MODES[@]}"; do
    for learn in "${LEARN_MODES[@]}"; do
      for hier in "${HIER_MODES[@]}"; do
        variant="th_x32_o${order_mode}_or${order_reward}_a${autoscale}_l${learn}_h${hier}"
        cargs="--bandit_policy thompson --no_order_bandit --no_branch_bandit --no_scheduler_bandit --no_random_bandit --no_dist_bandit --order_bandit_mode ${order_mode} --order_agent_reward ${order_reward} --reward_autoscale ${autoscale} --reward_weight_learning ${learn}"
        if [[ "$hier" == "on" ]]; then
          cargs="$cargs --bandit_hierarchy"
        fi
        for m in "${MAPS[@]}"; do
          read -r map scenprefix agents <<< "$m"
          full_map="$DATA_ROOT/$map"
          echo "[$(date '+%F %T')] $variant map=$map N=$agents"
          seq 1 25 | xargs -I{} -P "$PARALLEL" bash -lc \
            "run_case '$variant' 'X32' '$full_map' '$DATA_ROOT/scen-random/${scenprefix}-{}.scen' '$agents' '$cargs'"
        done
      done
    done
  done
done

python3 - << 'PY' "$RAW_CSV" "$MAPWISE_CSV" "$SUMMARY_MD"
import csv, sys
from collections import defaultdict

raw_csv, mapwise_csv, summary_md = sys.argv[1:4]
rows = list(csv.DictReader(open(raw_csv)))

def solved_only_avg(rr):
    vals = [float(r["soc"]) for r in rr if int(r["solved"]) == 1]
    return sum(vals)/len(vals) if vals else None

group = defaultdict(list)
for r in rows:
    key = (r["variant"], r["map"], int(r["agents"]))
    group[key].append(r)

baseline = {}
for (v,m,a), rr in group.items():
    if v == "baseline":
        baseline[(m,a)] = rr

out = []
for (v,m,a), rr in sorted(group.items()):
    if v == "baseline":
        continue
    b = baseline[(m,a)]
    bsol = sum(int(x["solved"]) for x in b)
    csol = sum(int(x["solved"]) for x in rr)
    bavg = solved_only_avg(b)
    cavg = solved_only_avg(rr)
    gain = None
    if bavg and cavg:
        gain = (bavg - cavg) * 100.0 / bavg
    out.append({
        "variant": v, "map": m, "agents": a,
        "baseline_solved": f"{bsol}/{len(b)}",
        "candidate_solved": f"{csol}/{len(rr)}",
        "baseline_avg_soc_solved_only": "" if bavg is None else f"{bavg:.2f}",
        "candidate_avg_soc_solved_only": "" if cavg is None else f"{cavg:.2f}",
        "soc_gain_pct_vs_baseline": "" if gain is None else f"{gain:.2f}",
    })

with open(mapwise_csv, "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=list(out[0].keys()))
    w.writeheader()
    w.writerows(out)

# compute simple aggregate score by variant over 4 maps
agg = defaultdict(lambda: {"gain_sum":0.0, "gain_n":0, "guard_ok":True})
for r in out:
    v = r["variant"]
    bsol = int(r["baseline_solved"].split("/")[0])
    csol = int(r["candidate_solved"].split("/")[0])
    if csol < bsol:
        agg[v]["guard_ok"] = False
    if r["soc_gain_pct_vs_baseline"]:
        agg[v]["gain_sum"] += float(r["soc_gain_pct_vs_baseline"])
        agg[v]["gain_n"] += 1

ranked = []
for v, s in agg.items():
    mean_gain = s["gain_sum"]/s["gain_n"] if s["gain_n"] else -1e9
    ranked.append((s["guard_ok"], mean_gain, v))
ranked.sort(key=lambda x: (x[0], x[1]), reverse=True)

with open(summary_md, "w") as f:
    f.write("# lacam0 Focus4 Next-Steps Sweep\n\n")
    f.write(f"- Raw CSV: `{raw_csv}`\n")
    f.write(f"- Mapwise CSV: `{mapwise_csv}`\n\n")
    f.write("## Top Variants (solved-guard first, then mean SOC gain)\n\n")
    f.write("| Rank | Variant | Solved Guard (all maps) | Mean SOC Gain % |\n")
    f.write("|---:|---|---|---:|\n")
    for i, (guard, mg, v) in enumerate(ranked[:15], start=1):
        f.write(f"| {i} | `{v}` | {'yes' if guard else 'no'} | {mg:.2f} |\n")

print("Wrote", mapwise_csv)
print("Wrote", summary_md)
PY

echo "Done"
echo "RawCSV: $RAW_CSV"
echo "MapwiseCSV: $MAPWISE_CSV"
echo "SummaryMD: $SUMMARY_MD"
