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
EPS=0.05
RUN_ID="$(date +%Y%m%d_%H%M%S)_lacam0_focus4_x32_policy_suite"

LOCAL_DATA_ROOT="$ROOT_DIR/benchmarks/movingai/data_all"
LEGACY_DATA_ROOT="$ROOT_DIR/../lacam/benchmarks/movingai/data_all"
if [[ -z "${DATA_ROOT:-}" ]]; then
  if [[ -d "$LOCAL_DATA_ROOT" ]]; then
    DATA_ROOT="$LOCAL_DATA_ROOT"
  else
    DATA_ROOT="$LEGACY_DATA_ROOT"
  fi
fi

CSV="$OUT_DIR/${RUN_ID}.csv"

usage() {
  cat <<EOF
Usage: $0 [options]
  --parallel N
  --time-limit N
  --eps FLOAT
  --run-id STRING
  --data-root PATH
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --parallel) PARALLEL="$2"; shift 2 ;;
    --time-limit) TIME_LIMIT="$2"; shift 2 ;;
    --eps) EPS="$2"; shift 2 ;;
    --run-id) RUN_ID="$2"; CSV="$OUT_DIR/${RUN_ID}.csv"; shift 2 ;;
    --data-root) DATA_ROOT="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown arg: $1" >&2; usage; exit 1 ;;
  esac
done

if [[ ! -x "$ROOT_DIR/build/main" ]]; then
  echo "Missing binary: $ROOT_DIR/build/main" >&2
  exit 1
fi

echo "config,policy,map,scen,agents,solved,soc,makespan,sum_of_loss,comp_time" > "$CSV"

run_case() {
  local cfg="$1" policy="$2" map="$3" scen="$4" agents="$5" cargs="$6"
  local out="/tmp/lacam0_policy_${RANDOM}_${RANDOM}.txt"
  "$ROOT_DIR/build/main" -m "$map" -i "$scen" -N "$agents" -t "$TIME_LIMIT" -v 0 -o "$out" $cargs >/dev/null || true
  local solved soc mk sol ct
  solved="$(awk -F= '/^solved=/{print $2}' "$out" 2>/dev/null || echo 0)"
  soc="$(awk -F= '/^soc=/{print $2}' "$out" 2>/dev/null || echo 0)"
  mk="$(awk -F= '/^makespan=/{print $2}' "$out" 2>/dev/null || echo 0)"
  sol="$(awk -F= '/^sum_of_loss=/{print $2}' "$out" 2>/dev/null || echo 0)"
  ct="$(awk -F= '/^comp_time=/{print $2}' "$out" 2>/dev/null || echo 0)"
  echo "$cfg,$policy,$(basename "$map"),$(basename "$scen"),$agents,$solved,$soc,$mk,$sol,$ct" >> "$CSV"
  rm -f "$out"
}

export ROOT_DIR TIME_LIMIT CSV
export -f run_case

declare -a MAPS=(
  "random-32-32-20.map random-32-32-20-random 300"
  "den520d.map den520d-random 700"
  "Paris_1_256.map Paris_1_256-random 1000"
  "warehouse-20-40-10-2-1.map warehouse-20-40-10-2-1-random 1000"
  "warehouse-20-40-10-2-2.map warehouse-20-40-10-2-2-random 1000"
)

declare -a POLICIES=(
  "baseline_x00 baseline --no_pibt_bandit --no_order_bandit --no_branch_bandit --no_scheduler_bandit --no_random_bandit --no_dist_bandit"
  "ucb1_x32 ucb1 --bandit_policy ucb1 --no_order_bandit --no_branch_bandit --no_scheduler_bandit --no_random_bandit --no_dist_bandit"
  "thompson_x32 thompson --bandit_policy thompson --no_order_bandit --no_branch_bandit --no_scheduler_bandit --no_random_bandit --no_dist_bandit"
  "epsilon_x32 epsilon_greedy --bandit_policy epsilon_greedy --bandit_epsilon ${EPS} --bandit_epsilon_final ${EPS} --bandit_epsilon_decay_steps 0 --no_order_bandit --no_branch_bandit --no_scheduler_bandit --no_random_bandit --no_dist_bandit"
  "random_x32 random_uniform --bandit_policy random_uniform --no_order_bandit --no_branch_bandit --no_scheduler_bandit --no_random_bandit --no_dist_bandit"
)

echo "[$(date '+%F %T')] run_id=$RUN_ID parallel=$PARALLEL time_limit=$TIME_LIMIT eps=$EPS data_root=$DATA_ROOT"

for p in "${POLICIES[@]}"; do
  cfg="$(echo "$p" | awk '{print $1}')"
  policy="$(echo "$p" | awk '{print $2}')"
  cargs="$(echo "$p" | cut -d' ' -f3-)"
  for m in "${MAPS[@]}"; do
    read -r map scenprefix agents <<< "$m"
    full_map="$DATA_ROOT/$map"
    echo "[$(date '+%F %T')] cfg=$cfg map=$map N=$agents"
    seq 1 25 | xargs -I{} -P "$PARALLEL" bash -lc \
      "run_case '$cfg' '$policy' '$full_map' '$DATA_ROOT/scen-random/${scenprefix}-{}.scen' '$agents' '$cargs'"
  done
done

python3 "$ROOT_DIR/scripts/summarize_focus4_policy_suite_x32.py" "$CSV"
echo "Done: $CSV"
