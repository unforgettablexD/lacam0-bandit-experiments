# 2026-05-26 Checkpoint: Bandit + Map-Aware + Stall Logic

## Scope Completed

- Implemented full 64-mask experimentation workflow with resumable chunk checkpoints.
- Added 5-map benchmark set support (including warehouse-20-40-10-2-1 and warehouse-20-40-10-2-2) across runners/summarizers.
- Added map-aware suite runners:
  - baseline_x00
  - mapaware_x35
  - mapaware_x36
  - mapaware_random_x35
- Added map-agnostic solver improvements (no map fingerprinting):
  - PIBT reward shaping with local congestion and no-progress penalties.
  - LaCAM stall-aware fallback behavior (safe-arm bias, reduced randomization under stalls, periodic safe restart injection).

## Key Implementation Files

- Core solver:
  - lacam/src/pibt.cpp
  - lacam/src/lacam.cpp
- Existing runtime/plumbing touched earlier in this phase:
  - main.cpp
  - lacam/include/pibt.hpp
  - lacam/src/dist_table.cpp
- Experiment scripts:
  - scripts/run_focus5_mask64_sweep.sh
  - scripts/run_focus5_map_aware_vs_baseline.sh
  - scripts/run_focus5_map_aware_suite.sh
- Summarizers:
  - scripts/summarize_focus5_map_aware_vs_baseline.py
  - scripts/summarize_focus5_map_aware_suite.py

## Main Experimental Results (Current State)

### Reward-weight tuning (SOC-first, map-aware)

- Run ID: 20260526_184756_lacam0_focus5_reward_weight_tuning
- Best profile: w_balA (used on non-Paris/non-den520d maps with X35 mask)
- Aggregate vs baseline_x00:
  - Mean SOC gain: +0.45%
  - Worst-map SOC gain: 0.00%
  - Mean median-CT gain: +7.88%
  - Mean p95-CT gain: +6.94%

### Promotion confirmation run

- Run ID: 20260526_185647_lacam0_focus5_map_aware_vs_baseline
- Promoted default behavior:
  - Paris_1_256 / den520d: baseline_x00
  - random + warehouses: X35 + w_balA reward weights
- Confirmation metrics:
  - Mean SOC gain: +0.45%
  - Worst-map SOC gain: 0.00%
  - Solved guard preserved on all maps (25/25 each)

### 4/5-map optimization update (professor-approved criterion)

- New acceptance criterion applied: at least 4/5 maps non-negative SOC gain.
- Added contextual PIBT bandit option (`linucb`) and tuned exploration strength on map-aware X35 profile.
- Best 4/5 candidate from suite:
  - Run ID: 20260526_192338_lacam0_focus5_map_aware_suite
  - Config: mapaware_x35_linucb_a08
  - Mean SOC gain: +0.64%
  - Worst-map SOC gain: -0.01%
  - Non-negative maps: 4/5
  - Solved guard: preserved
- Confirmation run after promotion:
  - Run ID: 20260526_193126_lacam0_focus5_map_aware_vs_baseline
  - Mean SOC gain: +0.64% (vs prior +0.45%)
  - Non-negative maps: 4/5
  - Runtime tradeoff observed (median/p95 less favorable than strict 5/5 profile)

### Feature-expanded contextual update (dual profile retention)

- Expanded LinUCB context features in PIBT arm selection (distance shape, blockage ratio, near-goal flag).
- Suite run ID: 20260526_203512_lacam0_focus5_map_aware_suite
  - Best profile: mapaware_x35_linucb_a08
  - Mean SOC gain: +0.54%
  - Worst-map SOC gain: 0.00%
  - Non-negative maps: 5/5
- Confirmation run ID: 20260526_204631_lacam0_focus5_map_aware_vs_baseline
  - Mean SOC gain: +0.536%
  - Worst-map SOC gain: 0.00%
  - Non-negative maps: 5/5

- Operational naming kept in runners:
  - strict_5of5: epsilon-greedy X35 + w_balA
  - aggressive_4of5: LinUCB X35 + w_balA (a08)

### 64-mask findings (5 maps, 25 scenarios/map)

- Best global mask tier by mean SOC gain: X34/X35/X36/X37 (~+0.4112%).
- No single global mask achieved strict positive SOC gain on all 5 maps.

### Map-aware strategy outcome

- Map-aware rule (baseline on Paris/den520d, X35 on random/warehouse maps) produced non-negative SOC gain on all 5 maps while keeping solved guard.
- This removed negative-map behavior seen in single global masks.

### Post stall-logic run snapshot (latest suite)

- Run ID: 20260526_181152_lacam0_focus5_map_aware_suite
- Aggregate ranking from summary:
  1. mapaware_x35
  2. mapaware_x36
  3. mapaware_random_x35
- mapaware_x35 remains best balanced deployment profile at this checkpoint.

## Command References Used

- Full map-aware suite:
  cd "/home/nicolas/Desktop/codes/phd projects/lacam0" && ./scripts/run_focus5_map_aware_suite.sh --parallel 10 --time-limit 60 --scenarios 25 --data-root "../lacam/benchmarks/movingai/data_all"

- 64-mask focused ranges:
  - X32..X43
  - X44..X63

## Result Artifacts to Reuse

- reports/movingai/20260526_164149_lacam0_focus5_mask64_sweep_summary.md
- reports/movingai/20260526_171105_lacam0_focus5_mask64_sweep_summary.md
- reports/movingai/20260526_175745_lacam0_focus5_map_aware_suite_summary.md
- reports/movingai/20260526_181152_lacam0_focus5_map_aware_suite_summary.md

## Recommended Next Step

- Keep two deployment modes available:
  - strict_5of5: mapaware_x35 (epsilon-greedy + w_balA)
  - soc_4of5: mapaware_x35_linucb_a08 (higher SOC, one slight negative map allowed)
- Next tuning target: recover p95 runtime for soc_4of5 profile without losing its SOC gain.

Updated practical recommendation:
- For robust deployment, prefer strict_5of5.
- For SOC-first experiments, use aggressive_4of5 and monitor runtime tails.