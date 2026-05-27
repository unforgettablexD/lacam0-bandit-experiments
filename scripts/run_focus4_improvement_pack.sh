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

SCEN_COUNT=25
TIME_LIMITS_CSV="20,30,60"
RANDOM_AGENTS_CSV="300,350,400"
REGRET_TRIALS_CSV="1,3,5,7"
EPS=0.05

LOCAL_DATA_ROOT="$ROOT_DIR/benchmarks/movingai/data_all"
LEGACY_DATA_ROOT="$ROOT_DIR/../lacam/benchmarks/movingai/data_all"
if [[ -z "${DATA_ROOT:-}" ]]; then
  if [[ -d "$LOCAL_DATA_ROOT" ]]; then
    DATA_ROOT="$LOCAL_DATA_ROOT"
  else
    DATA_ROOT="$LEGACY_DATA_ROOT"
  fi
fi

RUN_ID="$(date +%Y%m%d_%H%M%S)_lacam0_focus4_improvement_pack"
RAW_CSV="$OUT_DIR/${RUN_ID}.csv"
CHECKPOINT_DIR="$OUT_DIR/${RUN_ID}_checkpoints"
RESUME_RUN_ID=""

usage() {
  cat <<EOF
Usage: $0 [options]
  --parallel N
  --scenarios N
  --time-limits CSV         (default: $TIME_LIMITS_CSV)
  --random-agents CSV       (default: $RANDOM_AGENTS_CSV)
  --regret-trials CSV       (default: $REGRET_TRIALS_CSV)
  --eps FLOAT               (default: $EPS)
  --data-root PATH
  --run-id ID               Resume/rebuild an existing run id

Tracks executed:
  1) headroom_harder
  2) policy_regret
  3) warehouse_fallback
  4) reward_stability
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --parallel) PARALLEL="$2"; shift 2 ;;
    --scenarios) SCEN_COUNT="$2"; shift 2 ;;
    --time-limits) TIME_LIMITS_CSV="$2"; shift 2 ;;
    --random-agents) RANDOM_AGENTS_CSV="$2"; shift 2 ;;
    --regret-trials) REGRET_TRIALS_CSV="$2"; shift 2 ;;
    --eps) EPS="$2"; shift 2 ;;
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
if [[ "$PARALLEL" -lt 1 ]]; then
  echo "--parallel must be >= 1" >&2
  exit 1
fi
if [[ "$SCEN_COUNT" -lt 1 ]]; then
  echo "--scenarios must be >= 1" >&2
  exit 1
fi

mkdir -p "$CHECKPOINT_DIR"

csv_to_array() {
  local csv="$1"
  local -n arr_ref="$2"
  IFS=',' read -r -a arr_ref <<< "$csv"
}

csv_to_array "$TIME_LIMITS_CSV" TIME_LIMITS
csv_to_array "$RANDOM_AGENTS_CSV" RANDOM_AGENT_SET
csv_to_array "$REGRET_TRIALS_CSV" REGRET_SET

run_case() {
  local track="$1" profile="$2" policy="$3" regret="$4" autoscale="$5" learn="$6"
  local time_limit="$7" map_path="$8" scen_path="$9" agents="${10}" cargs="${11}" ckpt_dir="${12}"

  local mapb scenb key ckpt
  mapb="$(basename "$map_path")"
  scenb="$(basename "$scen_path")"
  key="${track}__${profile}__${policy}__r${regret}__a${autoscale}__l${learn}__t${time_limit}__${mapb}__${scenb}__N${agents}"
  ckpt="${ckpt_dir}/${key}.csvline"
  if [[ -f "$ckpt" ]]; then
    return 0
  fi

  local out="/tmp/lacam0_improve_${RANDOM}_${RANDOM}.txt"
  # shellcheck disable=SC2086
  "$ROOT_DIR/build/main" -m "$map_path" -i "$scen_path" -N "$agents" -t "$time_limit" -v 0 -o "$out" $cargs >/dev/null || true

  local solved soc mk sol ct
  solved="$(awk -F= '/^solved=/{print $2}' "$out" 2>/dev/null || echo 0)"
  soc="$(awk -F= '/^soc=/{print $2}' "$out" 2>/dev/null || echo 0)"
  mk="$(awk -F= '/^makespan=/{print $2}' "$out" 2>/dev/null || echo 0)"
  sol="$(awk -F= '/^sum_of_loss=/{print $2}' "$out" 2>/dev/null || echo 0)"
  ct="$(awk -F= '/^comp_time=/{print $2}' "$out" 2>/dev/null || echo 0)"

  echo "$track,$profile,$policy,$regret,$autoscale,$learn,$time_limit,$mapb,$scenb,$agents,$solved,$soc,$mk,$sol,$ct" > "${ckpt}.tmp"
  mv "${ckpt}.tmp" "$ckpt"
  rm -f "$out"
}

export ROOT_DIR
export -f run_case

baseline_args() {
  echo "--no_pibt_bandit --no_order_bandit --no_branch_bandit --no_scheduler_bandit --no_random_bandit --no_dist_bandit"
}

x32_args_for() {
  local policy="$1" regret="$2" autoscale="$3" learn="$4"
  local args="--no_order_bandit --no_branch_bandit --no_scheduler_bandit --no_random_bandit --no_dist_bandit"
  args+=" --pibt_regret_trials ${regret}"
  args+=" --reward_autoscale ${autoscale} --reward_weight_learning ${learn}"

  if [[ "$policy" == "thompson" ]]; then
    args+=" --bandit_policy thompson"
  elif [[ "$policy" == "ucb1" ]]; then
    args+=" --bandit_policy ucb1"
  elif [[ "$policy" == "epsilon_greedy" ]]; then
    args+=" --bandit_policy epsilon_greedy --bandit_epsilon ${EPS} --bandit_epsilon_final ${EPS} --bandit_epsilon_decay_steps 0"
  else
    args+=" --bandit_policy ${policy}"
  fi

  echo "$args"
}

run_profile_across_focus4() {
  local track="$1" profile="$2" policy="$3" regret="$4" autoscale="$5" learn="$6" time_limit="$7" random_agents="$8"

  local random_spec="random-32-32-20.map random-32-32-20-random ${random_agents}"
  local den_spec="den520d.map den520d-random 700"
  local paris_spec="Paris_1_256.map Paris_1_256-random 1000"
  local wh1_spec="warehouse-20-40-10-2-1.map warehouse-20-40-10-2-1-random 1000"
  local wh_spec="warehouse-20-40-10-2-2.map warehouse-20-40-10-2-2-random 1000"

  local specs=("$random_spec" "$den_spec" "$paris_spec" "$wh1_spec" "$wh_spec")

  for spec in "${specs[@]}"; do
    read -r map scenprefix agents <<< "$spec"
    local full_map="$DATA_ROOT/$map"
    if [[ ! -f "$full_map" ]]; then
      echo "Skipping missing map: $full_map" >&2
      continue
    fi

    local cargs
    if [[ "$profile" == "baseline_x00" ]]; then
      cargs="$(baseline_args)"
    elif [[ "$profile" == "x32_whsafe" && ( "$map" == "warehouse-20-40-10-2-1.map" || "$map" == "warehouse-20-40-10-2-2.map" ) ]]; then
      cargs="$(baseline_args)"
    elif [[ "$profile" == "x32_whsafe_regret7" && ( "$map" == "warehouse-20-40-10-2-1.map" || "$map" == "warehouse-20-40-10-2-2.map" ) ]]; then
      cargs="$(baseline_args)"
    elif [[ "$profile" == "x32_whsafe_regret7" ]]; then
      cargs="$(x32_args_for "$policy" "7" "$autoscale" "$learn")"
    else
      cargs="$(x32_args_for "$policy" "$regret" "$autoscale" "$learn")"
    fi

    echo "[$(date '+%F %T')] track=$track profile=$profile policy=$policy regret=$regret autoscale=$autoscale learn=$learn t=$time_limit map=$map N=$agents"
    seq 1 "$SCEN_COUNT" | xargs -I{} -P "$PARALLEL" bash -lc \
      "run_case '$track' '$profile' '$policy' '$regret' '$autoscale' '$learn' '$time_limit' '$full_map' '$DATA_ROOT/scen-random/${scenprefix}-{}.scen' '$agents' '$cargs' '$CHECKPOINT_DIR'"
  done
}

echo "[$(date '+%F %T')] run_id=$RUN_ID parallel=$PARALLEL scenarios=$SCEN_COUNT data_root=$DATA_ROOT"
echo "time_limits=${TIME_LIMITS[*]} random_agents=${RANDOM_AGENT_SET[*]} regrets=${REGRET_SET[*]} eps=$EPS"

# Track 1: harder benchmark headroom (time limits + random map load increase)
for t in "${TIME_LIMITS[@]}"; do
  for ra in "${RANDOM_AGENT_SET[@]}"; do
    run_profile_across_focus4 "headroom_harder" "baseline_x00" "baseline" "1" "off" "off" "$t" "$ra"
    run_profile_across_focus4 "headroom_harder" "x32_std" "thompson" "3" "off" "off" "$t" "$ra"
  done
done

# Track 2: policy + regret sweep on standard focus4 setting
for r in "${REGRET_SET[@]}"; do
  run_profile_across_focus4 "policy_regret" "baseline_x00" "baseline" "1" "off" "off" "60" "300"
  run_profile_across_focus4 "policy_regret" "x32_std" "thompson" "$r" "off" "off" "60" "300"
  run_profile_across_focus4 "policy_regret" "x32_std" "ucb1" "$r" "off" "off" "60" "300"
  run_profile_across_focus4 "policy_regret" "x32_std" "epsilon_greedy" "$r" "off" "off" "60" "300"
done

# Track 3: warehouse-safe fallback variants
run_profile_across_focus4 "warehouse_fallback" "baseline_x00" "baseline" "1" "off" "off" "60" "300"
run_profile_across_focus4 "warehouse_fallback" "x32_std" "thompson" "3" "off" "off" "60" "300"
run_profile_across_focus4 "warehouse_fallback" "x32_whsafe" "thompson" "3" "off" "off" "60" "300"
run_profile_across_focus4 "warehouse_fallback" "x32_whsafe_regret7" "thompson" "7" "off" "off" "60" "300"

# Track 4: reward stabilization options
run_profile_across_focus4 "reward_stability" "baseline_x00" "baseline" "1" "off" "off" "60" "300"
for autoscale in off zscore; do
  for learn in off online_linear; do
    run_profile_across_focus4 "reward_stability" "x32_std" "thompson" "3" "$autoscale" "$learn" "60" "300"
  done
done

# Rebuild raw csv from checkpoints for resumability
{
  echo "track,profile,policy,regret_trials,reward_autoscale,reward_weight_learning,time_limit,map,scen,agents,solved,soc,makespan,sum_of_loss,comp_time"
  find "$CHECKPOINT_DIR" -type f -name '*.csvline' -print0 | sort -z | xargs -0 -r cat
} > "$RAW_CSV"

python3 "$ROOT_DIR/scripts/summarize_focus4_improvement_pack.py" "$RAW_CSV"

echo "Done"
echo "RawCSV: $RAW_CSV"
echo "CheckpointDir: $CHECKPOINT_DIR"
