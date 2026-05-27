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

RUN_ID="$(date +%Y%m%d_%H%M%S)_lacam0_focus5_map_aware_vs_baseline"
RAW_CSV="$OUT_DIR/${RUN_ID}.csv"
CHECKPOINT_DIR="$OUT_DIR/${RUN_ID}_checkpoints"
RESUME_RUN_ID=""
PROFILE="strict_5of5"
MC_ROLLOUTS=4
MC_ROLLOUTS_AFTER_GOAL=2
MC_ROLLOUTS_EARLY_MARGIN=3
MC_SCORE_W_STAY=0.0
MC_SCORE_W_PROGRESS=0.0
MC_SCORE_W_REGRESS=0.0
MC_SCORE_STALL_ONLY=0

usage() {
  cat <<EOF
Usage: $0 [options]
  --parallel N
  --time-limit N
  --scenarios N
  --data-root PATH
  --run-id ID                  Resume/rebuild an existing run id
  --profile NAME               strict_5of5 | aggressive_4of5 | aggressive_4of5_mc | aggressive_4of5_mc_stallscore
  --mc-rollouts N              Override --pibt_rollouts for aggressive_4of5_mc
  --mc-after-goal N            Override --pibt_rollouts_after_goal for aggressive_4of5_mc
  --mc-early-margin N          Override --pibt_rollouts_early_margin for aggressive_4of5_mc
  --mc-score-stay X            Override --mccg_score_w_stay for aggressive_4of5_mc
  --mc-score-progress X        Override --mccg_score_w_progress for aggressive_4of5_mc
  --mc-score-regress X         Override --mccg_score_w_regress for aggressive_4of5_mc
  --mc-score-stall-only        Pass --mccg_score_stall_only for aggressive_4of5_mc
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --parallel) PARALLEL="$2"; shift 2 ;;
    --time-limit) TIME_LIMIT="$2"; shift 2 ;;
    --scenarios) SCEN_COUNT="$2"; shift 2 ;;
    --data-root) DATA_ROOT="$2"; shift 2 ;;
    --run-id) RESUME_RUN_ID="$2"; shift 2 ;;
    --profile) PROFILE="$2"; shift 2 ;;
    --mc-rollouts) MC_ROLLOUTS="$2"; shift 2 ;;
    --mc-after-goal) MC_ROLLOUTS_AFTER_GOAL="$2"; shift 2 ;;
    --mc-early-margin) MC_ROLLOUTS_EARLY_MARGIN="$2"; shift 2 ;;
    --mc-score-stay) MC_SCORE_W_STAY="$2"; shift 2 ;;
    --mc-score-progress) MC_SCORE_W_PROGRESS="$2"; shift 2 ;;
    --mc-score-regress) MC_SCORE_W_REGRESS="$2"; shift 2 ;;
    --mc-score-stall-only) MC_SCORE_STALL_ONLY=1; shift 1 ;;
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
if [[ "$MC_ROLLOUTS" -lt 1 ]]; then
  echo "--mc-rollouts must be >= 1" >&2
  exit 1
fi
if [[ "$MC_ROLLOUTS_AFTER_GOAL" -lt 0 ]]; then
  echo "--mc-after-goal must be >= 0" >&2
  exit 1
fi
if [[ "$MC_ROLLOUTS_EARLY_MARGIN" -lt 0 ]]; then
  echo "--mc-early-margin must be >= 0" >&2
  exit 1
fi

mkdir -p "$CHECKPOINT_DIR"

run_case_row() {
  local cfg="$1" map_path="$2" scen_path="$3" agents="$4" cargs="$5" row_path="$6"

  local mapb scenb out solved soc mk sol ct
  mapb="$(basename "$map_path")"
  scenb="$(basename "$scen_path")"
  out="/tmp/lacam0_mapaware_${RANDOM}_${RANDOM}.txt"

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

  tmp_dir="$(mktemp -d /tmp/lacam0_mapaware_chunk_XXXXXX)"
  chunk_tmp="${chunk}.tmp"

  seq 1 "$SCEN_COUNT" | xargs -I{} -P "$PARALLEL" bash -lc \
    "run_case_row '$cfg' '$map_path' '$DATA_ROOT/scen-random/${scenprefix}-{}.scen' '$agents' '$cargs' '$tmp_dir/row_{}.csv'"

  find "$tmp_dir" -type f -name 'row_*.csv' -print0 | sort -z | xargs -0 -r cat > "$chunk_tmp"
  mv "$chunk_tmp" "$chunk"
  rm -rf "$tmp_dir"
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
  args+=" --bandit_policy epsilon_greedy --bandit_epsilon 0.05 --bandit_epsilon_final 0.05 --bandit_epsilon_decay_steps 0 --pibt_regret_trials 5 --no_events_log"
  echo "${args# }"
}

baseline_args() {
  echo "--no_pibt_bandit --no_order_bandit --no_branch_bandit --no_scheduler_bandit --no_random_bandit --no_dist_bandit --no_events_log"
}

mask35_w_balA_args() {
  echo "--no_order_bandit --no_branch_bandit --no_scheduler_bandit --bandit_policy epsilon_greedy --bandit_epsilon 0.05 --bandit_epsilon_final 0.05 --bandit_epsilon_decay_steps 0 --pibt_regret_trials 5 --reward_w_goal 1.2 --reward_w_delay 0.9 --reward_w_stay 0.8 --reward_w_leave 1.0 --reward_w_occ 0.9 --reward_w_cong 0.8 --reward_w_noprog 0.9 --no_events_log"
}

mask35_w_balA_linucb_a08_args() {
  echo "--no_order_bandit --no_branch_bandit --no_scheduler_bandit --bandit_policy linucb --bandit_epsilon 0.08 --bandit_epsilon_final 0.08 --bandit_epsilon_decay_steps 0 --pibt_regret_trials 5 --reward_w_goal 1.2 --reward_w_delay 0.9 --reward_w_stay 0.8 --reward_w_leave 1.0 --reward_w_occ 0.9 --reward_w_cong 0.8 --reward_w_noprog 0.9 --no_events_log"
}

mask35_w_balA_linucb_a08_mccg_args() {
  local score_stall_only_arg=""
  if [[ "$MC_SCORE_STALL_ONLY" -eq 1 ]]; then
    score_stall_only_arg=" --mccg_score_stall_only"
  fi
  echo "--no_order_bandit --no_branch_bandit --no_scheduler_bandit --bandit_policy linucb --bandit_epsilon 0.08 --bandit_epsilon_final 0.08 --bandit_epsilon_decay_steps 0 --pibt_regret_trials 5 --pibt_rollouts $MC_ROLLOUTS --pibt_rollouts_after_goal $MC_ROLLOUTS_AFTER_GOAL --pibt_rollouts_early_margin $MC_ROLLOUTS_EARLY_MARGIN --mccg_score_w_stay $MC_SCORE_W_STAY --mccg_score_w_progress $MC_SCORE_W_PROGRESS --mccg_score_w_regress $MC_SCORE_W_REGRESS$score_stall_only_arg --reward_w_goal 1.2 --reward_w_delay 0.9 --reward_w_stay 0.8 --reward_w_leave 1.0 --reward_w_occ 0.9 --reward_w_cong 0.8 --reward_w_noprog 0.9 --no_events_log"
}

mask35_w_balA_linucb_a08_mccg_stallscore_args() {
  echo "--no_order_bandit --no_branch_bandit --no_scheduler_bandit --bandit_policy linucb --bandit_epsilon 0.08 --bandit_epsilon_final 0.08 --bandit_epsilon_decay_steps 0 --pibt_regret_trials 5 --pibt_rollouts 4 --pibt_rollouts_after_goal 2 --pibt_rollouts_early_margin 3 --mccg_score_w_stay 0.2 --mccg_score_w_progress 0.8 --mccg_score_w_regress 1.2 --mccg_score_stall_only --reward_w_goal 1.2 --reward_w_delay 0.9 --reward_w_stay 0.8 --reward_w_leave 1.0 --reward_w_occ 0.9 --reward_w_cong 0.8 --reward_w_noprog 0.9 --no_events_log"
}

# Map-aware policy: baseline on Paris/den520d, X35 elsewhere
map_aware_args() {
  local map_name="$1"
  case "$map_name" in
    Paris_1_256.map|den520d.map)
      baseline_args
      ;;
    *)
      case "$PROFILE" in
        strict_5of5) mask35_w_balA_args ;;
        aggressive_4of5) mask35_w_balA_linucb_a08_args ;;
        aggressive_4of5_mc) mask35_w_balA_linucb_a08_mccg_args ;;
        aggressive_4of5_mc_stallscore) mask35_w_balA_linucb_a08_mccg_stallscore_args ;;
        *) echo "Unknown --profile: $PROFILE" >&2; exit 1 ;;
      esac
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

echo "[$(date '+%F %T')] run_id=$RUN_ID profile=$PROFILE parallel=$PARALLEL scenarios=$SCEN_COUNT time_limit=$TIME_LIMIT data_root=$DATA_ROOT mc_rollouts=$MC_ROLLOUTS mc_after_goal=$MC_ROLLOUTS_AFTER_GOAL mc_early_margin=$MC_ROLLOUTS_EARLY_MARGIN mc_score_stay=$MC_SCORE_W_STAY mc_score_progress=$MC_SCORE_W_PROGRESS mc_score_regress=$MC_SCORE_W_REGRESS mc_score_stall_only=$MC_SCORE_STALL_ONLY"

for spec in "${MAPS[@]}"; do
  read -r map scenprefix agents <<< "$spec"
  full_map="$DATA_ROOT/$map"

  echo "[$(date '+%F %T')] cfg=baseline_x00 map=$map N=$agents"
  run_map_chunk "baseline_x00" "$full_map" "$scenprefix" "$agents" "$(baseline_args)" "$CHECKPOINT_DIR"

  echo "[$(date '+%F %T')] cfg=mapaware_x35 map=$map N=$agents"
  run_map_chunk "mapaware_x35" "$full_map" "$scenprefix" "$agents" "$(map_aware_args "$map")" "$CHECKPOINT_DIR"
done

{
  echo "config,map,scen,agents,solved,soc,makespan,sum_of_loss,comp_time"
  find "$CHECKPOINT_DIR" -type f -name '*.csvchunk' -print0 | sort -z | xargs -0 -r cat
} > "$RAW_CSV"

python3 "$ROOT_DIR/scripts/summarize_focus5_map_aware_vs_baseline.py" "$RAW_CSV"

echo "Done"
echo "RawCSV: $RAW_CSV"
echo "CheckpointDir: $CHECKPOINT_DIR"
