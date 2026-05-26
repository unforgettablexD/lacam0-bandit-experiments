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
RUN_ID="$(date +%Y%m%d_%H%M%S)_lacam0_focus4_baseline_vs_x32"
CSV="$OUT_DIR/${RUN_ID}.csv"

usage() {
  cat <<EOF
Usage: $0 [options]
  --parallel N
  --time-limit N
  --data-root PATH   (default: $DATA_ROOT)
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

echo "config,map,scen,agents,solved,soc,makespan,sum_of_loss,comp_time" > "$CSV"

run_case() {
  local cfg="$1" map="$2" scen="$3" agents="$4" cargs="$5"
  local out="/tmp/lacam0_case_${RANDOM}_${RANDOM}.txt"
  "$ROOT_DIR/build/main" -m "$map" -i "$scen" -N "$agents" -t "$TIME_LIMIT" -v 0 -o "$out" $cargs >/dev/null || true
  local solved soc mk sol ct
  solved="$(awk -F= '/^solved=/{print $2}' "$out" 2>/dev/null || echo 0)"
  soc="$(awk -F= '/^soc=/{print $2}' "$out" 2>/dev/null || echo 0)"
  mk="$(awk -F= '/^makespan=/{print $2}' "$out" 2>/dev/null || echo 0)"
  sol="$(awk -F= '/^sum_of_loss=/{print $2}' "$out" 2>/dev/null || echo 0)"
  ct="$(awk -F= '/^comp_time=/{print $2}' "$out" 2>/dev/null || echo 0)"
  echo "$cfg,$(basename "$map"),$(basename "$scen"),$agents,$solved,$soc,$mk,$sol,$ct" >> "$CSV"
  rm -f "$out"
}

export ROOT_DIR TIME_LIMIT CSV
export -f run_case

declare -a MAPS=(
  "random-32-32-20.map random-32-32-20-random 300"
  "den520d.map den520d-random 700"
  "Paris_1_256.map Paris_1_256-random 1000"
  "warehouse-20-40-10-2-2.map warehouse-20-40-10-2-2-random 1000"
)

echo "[$(date '+%F %T')] run_id=$RUN_ID parallel=$PARALLEL data_root=$DATA_ROOT"

for cfg in baseline_x00 thompson_x32; do
  if [[ "$cfg" == "baseline_x00" ]]; then
    CARGS="--no_pibt_bandit"
  else
    CARGS="--bandit_policy thompson --no_order_bandit --no_branch_bandit --no_scheduler_bandit --no_random_bandit --no_dist_bandit"
  fi
  for m in "${MAPS[@]}"; do
    read -r map scenprefix agents <<< "$m"
    full_map="$DATA_ROOT/$map"
    echo "[$(date '+%F %T')] cfg=$cfg map=$map N=$agents"
    seq 1 25 | xargs -I{} -P "$PARALLEL" bash -lc \
      "run_case '$cfg' '$full_map' '$DATA_ROOT/scen-random/${scenprefix}-{}.scen' '$agents' '$CARGS'"
  done
done

echo "Done: $CSV"
python3 "$ROOT_DIR/scripts/summarize_focus4_baseline_vs_x32.py" "$CSV"
