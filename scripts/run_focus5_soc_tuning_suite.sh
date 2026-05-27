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
SCEN_COUNT=25

LOCAL_DATA_ROOT="$ROOT_DIR/benchmarks/movingai/data_all"
LEGACY_DATA_ROOT="$ROOT_DIR/../lacam/benchmarks/movingai/data_all"
if [[ -z "${DATA_ROOT:-}" ]]; then
  if [[ -d "$LOCAL_DATA_ROOT" ]]; then
    DATA_ROOT="$LOCAL_DATA_ROOT"
  else
    DATA_ROOT="$LEGACY_DATA_ROOT"
  fi
fi

RUN_ID="$(date +%Y%m%d_%H%M%S)_lacam0_focus5_soc_tuning_suite"
RAW_CSV="$OUT_DIR/${RUN_ID}.csv"
CHECKPOINT_DIR="$OUT_DIR/${RUN_ID}_checkpoints"
RESUME_RUN_ID=""

usage() {
  cat <<EOF
Usage: $0 [options]
  --parallel N
  --time-limit N
  --scenarios N
  --data-root PATH
  --run-id ID                  Resume/rebuild an existing run id
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --parallel) PARALLEL="$2"; shift 2 ;;
    --time-limit) TIME_LIMIT="$2"; shift 2 ;;
    --scenarios) SCEN_COUNT="$2"; shift 2 ;;
    --data-root) DATA_ROOT="$2"; shift 2 ;;
    --run-id) RESUME_RUN_ID="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown arg: $1" >&2; usage; exit 1 ;;
  esac
done

if [[ -n "$RESUME_RUN_ID" ]]; then
  RUN_ID="$RESUME_RUN_ID"
  RAW_CSV="$OUT_DIR/${RUN_ID}.csv"
  CHECKPOINT_DIR="$OUT_DIR/${RUN_ID}_checkpoints"
fi

if [[ ! -x "$ROOT_DIR/build/main" ]]; then
  echo "Missing binary: $ROOT_DIR/build/main" >&2
  exit 1
fi
if [[ ! -d "$DATA_ROOT" ]]; then
  echo "Missing data root: $DATA_ROOT" >&2
  exit 1
fi

mkdir -p "$CHECKPOINT_DIR"

run_case_row() {
  local cfg="$1" map_path="$2" scen_path="$3" agents="$4" cargs="$5" row_path="$6"

  local mapb scenb out solved soc mk sol ct
  mapb="$(basename "$map_path")"
  scenb="$(basename "$scen_path")"
  out="/tmp/lacam0_soc_tuning_${RANDOM}_${RANDOM}.txt"

  # shellcheck disable=SC2086
  "$ROOT_DIR/build/main" -m "$map_path" -i "$scen_path" -N "$agents" -t "$TIME_LIMIT" -v 0 -o "$out" $cargs >/dev/null || true

  solved="$(awk -F= '/^solved=/{print $2}' "$out" 2>/dev/null || echo 0)"
  soc="$(awk -F= '/^soc=/{print $2}' "$out" 2>/dev/null || echo 0)"
  mk="$(awk -F= '/^makespan=/{print $2}' "$out" 2>/dev/null || echo 0)"
  sol="$(awk -F= '/^sum_of_loss=/{print $2}' "$out" 2>/dev/null || echo 0)"
  ct="$(awk -F= '/^comp_time=/{print $2}' "$out" 2>/dev/null || echo 0)"

  echo "$cfg,$mapb,$scenb,$agents,$solved,$soc,$mk,$sol,$ct" > "$row_path"
  rm -f "$out"
}

run_map_chunk() {
  local cfg="$1" map_path="$2" scenprefix="$3" agents="$4" cargs="$5" ckpt_dir="$6"

  local mapb chunk tmp_dir chunk_tmp
  mapb="$(basename "$map_path")"
  chunk="${ckpt_dir}/${cfg}__${mapb}__N${agents}__t${TIME_LIMIT}.csvchunk"
  if [[ -f "$chunk" ]]; then
    return 0
  fi

  tmp_dir="$(mktemp -d /tmp/lacam0_soc_tuning_chunk_XXXXXX)"
  chunk_tmp="${chunk}.tmp"

  seq 1 "$SCEN_COUNT" | xargs -I{} -P "$PARALLEL" bash -lc \
    "run_case_row '$cfg' '$map_path' '$DATA_ROOT/scen-random/${scenprefix}-{}.scen' '$agents' '$cargs' '$tmp_dir/row_{}.csv'"

  find "$tmp_dir" -type f -name 'row_*.csv' -print0 | sort -z | xargs -0 -r cat > "$chunk_tmp"
  mv "$chunk_tmp" "$chunk"
  rm -rf "$tmp_dir"
}

baseline_args() {
  echo "--no_pibt_bandit --no_order_bandit --no_branch_bandit --no_scheduler_bandit --no_random_bandit --no_dist_bandit --no_events_log"
}

mask35_args() {
  local policy="$1"
  local eps="$2"
  local regret="$3"
  local args="--pibt_regret_trials ${regret} --no_events_log"
  # X35 bits: pibt=1 order=0 branch=0 scheduler=0 random=1 dist=1
  args+=" --no_order_bandit --no_branch_bandit --no_scheduler_bandit"

  case "$policy" in
    epsilon_greedy)
      args+=" --bandit_policy epsilon_greedy --bandit_epsilon ${eps} --bandit_epsilon_final ${eps} --bandit_epsilon_decay_steps 0"
      ;;
    thompson|softmax)
      args+=" --bandit_policy ${policy} --bandit_epsilon ${eps} --bandit_epsilon_final ${eps} --bandit_epsilon_decay_steps 0"
      ;;
    *)
      echo "Unknown policy: $policy" >&2
      exit 1
      ;;
  esac
  echo "$args"
}

args_for_cfg_and_map() {
  local cfg="$1"
  local map_name="$2"
  case "$cfg" in
    baseline_x00)
      baseline_args
      ;;
    x35_eps005_r5)
      case "$map_name" in
        Paris_1_256.map|den520d.map) baseline_args ;;
        *) mask35_args "epsilon_greedy" "0.05" "5" ;;
      esac
      ;;
    x35_eps002_r5)
      case "$map_name" in
        Paris_1_256.map|den520d.map) baseline_args ;;
        *) mask35_args "epsilon_greedy" "0.02" "5" ;;
      esac
      ;;
    x35_softmax_r5)
      case "$map_name" in
        Paris_1_256.map|den520d.map) baseline_args ;;
        *) mask35_args "softmax" "0.08" "5" ;;
      esac
      ;;
    x35_thompson_r5)
      case "$map_name" in
        Paris_1_256.map|den520d.map) baseline_args ;;
        *) mask35_args "thompson" "0.05" "5" ;;
      esac
      ;;
    x35_softmax_r9)
      case "$map_name" in
        Paris_1_256.map|den520d.map) baseline_args ;;
        *) mask35_args "softmax" "0.08" "9" ;;
      esac
      ;;
    *)
      echo "Unknown cfg: $cfg" >&2
      exit 1
      ;;
  esac
}

export ROOT_DIR DATA_ROOT PARALLEL SCEN_COUNT TIME_LIMIT
export -f run_case_row
export -f run_map_chunk

declare -a MAPS=(
  "random-32-32-20.map random-32-32-20-random 300"
  "den520d.map den520d-random 700"
  "Paris_1_256.map Paris_1_256-random 1000"
  "warehouse-20-40-10-2-1.map warehouse-20-40-10-2-1-random 1000"
  "warehouse-20-40-10-2-2.map warehouse-20-40-10-2-2-random 1000"
)

declare -a CONFIGS=(
  "baseline_x00"
  "x35_eps005_r5"
  "x35_eps002_r5"
  "x35_softmax_r5"
  "x35_thompson_r5"
  "x35_softmax_r9"
)

echo "[$(date '+%F %T')] run_id=$RUN_ID parallel=$PARALLEL scenarios=$SCEN_COUNT time_limit=$TIME_LIMIT data_root=$DATA_ROOT"

for cfg in "${CONFIGS[@]}"; do
  for spec in "${MAPS[@]}"; do
    read -r map scenprefix agents <<< "$spec"
    full_map="$DATA_ROOT/$map"
    cargs="$(args_for_cfg_and_map "$cfg" "$map")"
    echo "[$(date '+%F %T')] cfg=$cfg map=$map N=$agents"
    run_map_chunk "$cfg" "$full_map" "$scenprefix" "$agents" "$cargs" "$CHECKPOINT_DIR"
  done
done

{
  echo "config,map,scen,agents,solved,soc,makespan,sum_of_loss,comp_time"
  find "$CHECKPOINT_DIR" -type f -name '*.csvchunk' -print0 | sort -z | xargs -0 -r cat
} > "$RAW_CSV"

python3 "$ROOT_DIR/scripts/summarize_focus5_map_aware_suite.py" "$RAW_CSV"

echo "Done"
echo "RawCSV: $RAW_CSV"
echo "CheckpointDir: $CHECKPOINT_DIR"
