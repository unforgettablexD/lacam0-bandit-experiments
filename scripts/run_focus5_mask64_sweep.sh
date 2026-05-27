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
POLICY="epsilon_greedy"
EPS=0.05
PIBT_REGRET_TRIALS=5
START_MASK=0
END_MASK=63

LOCAL_DATA_ROOT="$ROOT_DIR/benchmarks/movingai/data_all"
LEGACY_DATA_ROOT="$ROOT_DIR/../lacam/benchmarks/movingai/data_all"
if [[ -z "${DATA_ROOT:-}" ]]; then
  if [[ -d "$LOCAL_DATA_ROOT" ]]; then
    DATA_ROOT="$LOCAL_DATA_ROOT"
  else
    DATA_ROOT="$LEGACY_DATA_ROOT"
  fi
fi

RUN_ID="$(date +%Y%m%d_%H%M%S)_lacam0_focus5_mask64_sweep"
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
  --policy NAME                (default: $POLICY)
  --eps FLOAT                  (default: $EPS)
  --pibt-regret-trials N       (default: $PIBT_REGRET_TRIALS)
  --start-mask N               (default: $START_MASK)
  --end-mask N                 (default: $END_MASK)
  --run-id ID                  Resume/rebuild an existing run id

Policies: epsilon_greedy | ucb1 | thompson | random_uniform
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --parallel) PARALLEL="$2"; shift 2 ;;
    --time-limit) TIME_LIMIT="$2"; shift 2 ;;
    --scenarios) SCEN_COUNT="$2"; shift 2 ;;
    --data-root) DATA_ROOT="$2"; shift 2 ;;
    --policy) POLICY="$2"; shift 2 ;;
    --eps) EPS="$2"; shift 2 ;;
    --pibt-regret-trials) PIBT_REGRET_TRIALS="$2"; shift 2 ;;
    --start-mask) START_MASK="$2"; shift 2 ;;
    --end-mask) END_MASK="$2"; shift 2 ;;
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
if [[ "$PARALLEL" -lt 1 ]]; then
  echo "--parallel must be >= 1" >&2
  exit 1
fi
if [[ "$SCEN_COUNT" -lt 1 ]]; then
  echo "--scenarios must be >= 1" >&2
  exit 1
fi
if [[ "$START_MASK" -lt 0 || "$END_MASK" -gt 63 || "$START_MASK" -gt "$END_MASK" ]]; then
  echo "mask range must satisfy 0 <= start <= end <= 63" >&2
  exit 1
fi

mkdir -p "$CHECKPOINT_DIR"

run_case_row() {
  local track="$1" profile="$2" policy="$3" regret="$4" autoscale="$5" learn="$6"
  local time_limit="$7" map_path="$8" scen_path="$9" agents="${10}" cargs="${11}" row_path="${12}"

  local mapb scenb out solved soc mk sol ct
  mapb="$(basename "$map_path")"
  scenb="$(basename "$scen_path")"
  out="/tmp/lacam0_mask64_${RANDOM}_${RANDOM}.txt"

  # shellcheck disable=SC2086
  "$ROOT_DIR/build/main" -m "$map_path" -i "$scen_path" -N "$agents" -t "$time_limit" -v 0 -o "$out" $cargs >/dev/null || true

  solved="$(awk -F= '/^solved=/{print $2}' "$out" 2>/dev/null || echo 0)"
  soc="$(awk -F= '/^soc=/{print $2}' "$out" 2>/dev/null || echo 0)"
  mk="$(awk -F= '/^makespan=/{print $2}' "$out" 2>/dev/null || echo 0)"
  sol="$(awk -F= '/^sum_of_loss=/{print $2}' "$out" 2>/dev/null || echo 0)"
  ct="$(awk -F= '/^comp_time=/{print $2}' "$out" 2>/dev/null || echo 0)"

  echo "$track,$profile,$policy,$regret,$autoscale,$learn,$time_limit,$mapb,$scenb,$agents,$solved,$soc,$mk,$sol,$ct" > "$row_path"
  rm -f "$out"
}

run_map_chunk() {
  local track="$1" profile="$2" policy="$3" regret="$4" autoscale="$5" learn="$6"
  local time_limit="$7" map_path="$8" scenprefix="$9" agents="${10}" cargs="${11}" ckpt_dir="${12}"

  local mapb chunk tmp_dir chunk_tmp
  mapb="$(basename "$map_path")"
  chunk="${ckpt_dir}/${track}__${profile}__${policy}__r${regret}__a${autoscale}__l${learn}__t${time_limit}__${mapb}__N${agents}.csvchunk"
  if [[ -f "$chunk" ]]; then
    return 0
  fi

  tmp_dir="$(mktemp -d /tmp/lacam0_mask64_chunk_XXXXXX)"
  chunk_tmp="${chunk}.tmp"
  rm -f "$chunk_tmp"

  seq 1 "$SCEN_COUNT" | xargs -I{} -P "$PARALLEL" bash -lc \
    "run_case_row '$track' '$profile' '$policy' '$regret' '$autoscale' '$learn' '$time_limit' '$map_path' '$DATA_ROOT/scen-random/${scenprefix}-{}.scen' '$agents' '$cargs' '$tmp_dir/row_{}.csv'"

  find "$tmp_dir" -type f -name 'row_*.csv' -print0 | sort -z | xargs -0 -r cat > "$chunk_tmp"
  mv "$chunk_tmp" "$chunk"
  rm -rf "$tmp_dir"
}

export ROOT_DIR DATA_ROOT PARALLEL SCEN_COUNT
export -f run_case_row
export -f run_map_chunk

baseline_args() {
  echo "--no_pibt_bandit --no_order_bandit --no_branch_bandit --no_scheduler_bandit --no_random_bandit --no_dist_bandit --no_events_log"
}

mask_args() {
  local mask="$1"
  local p o b s r d args bits
  bits=$(awk -v n="$mask" 'BEGIN{r=""; for(i=0;i<6;i++){r=(n%2) r; n=int(n/2)} print r}')
  p=${bits:0:1}; o=${bits:1:1}; b=${bits:2:1}; s=${bits:3:1}; r=${bits:4:1}; d=${bits:5:1}
  args=""
  [[ "$p" == "0" ]] && args+=" --no_pibt_bandit"
  [[ "$o" == "0" ]] && args+=" --no_order_bandit"
  [[ "$b" == "0" ]] && args+=" --no_branch_bandit"
  [[ "$s" == "0" ]] && args+=" --no_scheduler_bandit"
  [[ "$r" == "0" ]] && args+=" --no_random_bandit"
  [[ "$d" == "0" ]] && args+=" --no_dist_bandit"
  args+=" --pibt_regret_trials ${PIBT_REGRET_TRIALS} --no_events_log"

  case "$POLICY" in
    epsilon_greedy)
      args+=" --bandit_policy epsilon_greedy --bandit_epsilon ${EPS} --bandit_epsilon_final ${EPS} --bandit_epsilon_decay_steps 0"
      ;;
    ucb1|thompson|random_uniform)
      args+=" --bandit_policy ${POLICY}"
      ;;
    *)
      echo "Unsupported policy: $POLICY" >&2
      exit 1
      ;;
  esac

  echo "${args# }"
}

declare -a MAPS=(
  "random-32-32-20.map random-32-32-20-random 300"
  "den520d.map den520d-random 700"
  "Paris_1_256.map Paris_1_256-random 1000"
  "warehouse-20-40-10-2-1.map warehouse-20-40-10-2-1-random 1000"
  "warehouse-20-40-10-2-2.map warehouse-20-40-10-2-2-random 1000"
)

TRACK_NAME="mask64_${POLICY}"
echo "[$(date '+%F %T')] run_id=$RUN_ID parallel=$PARALLEL scenarios=$SCEN_COUNT time_limit=$TIME_LIMIT policy=$POLICY regret=$PIBT_REGRET_TRIALS masks=${START_MASK}..${END_MASK} data_root=$DATA_ROOT"

for spec in "${MAPS[@]}"; do
  read -r map scenprefix agents <<< "$spec"
  full_map="$DATA_ROOT/$map"
  echo "[$(date '+%F %T')] baseline map=$map N=$agents"
  run_map_chunk "$TRACK_NAME" "baseline_x00" "baseline" "1" "off" "off" "$TIME_LIMIT" "$full_map" "$scenprefix" "$agents" "$(baseline_args)" "$CHECKPOINT_DIR"
done

for mask in $(seq "$START_MASK" "$END_MASK"); do
  profile=$(printf 'mask_X%02d' "$mask")
  cargs="$(mask_args "$mask")"
  for spec in "${MAPS[@]}"; do
    read -r map scenprefix agents <<< "$spec"
    full_map="$DATA_ROOT/$map"
    echo "[$(date '+%F %T')] profile=$profile map=$map N=$agents"
    run_map_chunk "$TRACK_NAME" "$profile" "$POLICY" "$PIBT_REGRET_TRIALS" "off" "off" "$TIME_LIMIT" "$full_map" "$scenprefix" "$agents" "$cargs" "$CHECKPOINT_DIR"
  done
done

{
  echo "track,profile,policy,regret_trials,reward_autoscale,reward_weight_learning,time_limit,map,scen,agents,solved,soc,makespan,sum_of_loss,comp_time"
  find "$CHECKPOINT_DIR" -type f -name '*.csvchunk' -print0 | sort -z | xargs -0 -r cat
} > "$RAW_CSV"

python3 "$ROOT_DIR/scripts/summarize_focus4_improvement_pack.py" "$RAW_CSV"

echo "Done"
echo "RawCSV: $RAW_CSV"
echo "CheckpointDir: $CHECKPOINT_DIR"