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

- Keep mapaware_x35 as default robust profile.
- Continue tuning stall sensitivity and reward weights to recover part of random-map SOC while preserving all-map non-negative behavior.