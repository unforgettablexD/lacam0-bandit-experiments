# lacam0 Focus4 Improvement Pack

- Raw CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_171105_lacam0_focus5_mask64_sweep.csv`
- Mapwise CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_171105_lacam0_focus5_mask64_sweep_mapwise.csv`

## Ranking (solved guard first, then SOC gain, then runtime gains)

| Rank | Track | Time | Profile | Policy | Regret | Autoscale | Learning | Solved Guard | Mean SOC Gain % | Mean Median-CT Gain % | Mean P95-CT Gain % | Maps |
|---:|---|---:|---|---|---:|---|---|---|---:|---:|---:|---:|
| 1 | mask64_epsilon_greedy | 60 | mask_X59 | epsilon_greedy | 5 | off | off | yes | 0.40 | 53.80 | 53.59 | 5 |
| 2 | mask64_epsilon_greedy | 60 | mask_X61 | epsilon_greedy | 5 | off | off | yes | 0.40 | 51.91 | 55.00 | 5 |
| 3 | mask64_epsilon_greedy | 60 | mask_X60 | epsilon_greedy | 5 | off | off | yes | 0.40 | 38.82 | 39.11 | 5 |
| 4 | mask64_epsilon_greedy | 60 | mask_X58 | epsilon_greedy | 5 | off | off | yes | 0.40 | 13.64 | 9.27 | 5 |
| 5 | mask64_epsilon_greedy | 60 | mask_X45 | epsilon_greedy | 5 | off | off | yes | 0.40 | 11.22 | 4.55 | 5 |
| 6 | mask64_epsilon_greedy | 60 | mask_X47 | epsilon_greedy | 5 | off | off | yes | 0.40 | 6.11 | -14.37 | 5 |
| 7 | mask64_epsilon_greedy | 60 | mask_X57 | epsilon_greedy | 5 | off | off | yes | 0.40 | -0.45 | -5.71 | 5 |
| 8 | mask64_epsilon_greedy | 60 | mask_X56 | epsilon_greedy | 5 | off | off | yes | 0.40 | -2.01 | -11.02 | 5 |
| 9 | mask64_epsilon_greedy | 60 | mask_X46 | epsilon_greedy | 5 | off | off | yes | 0.40 | -8.45 | -13.36 | 5 |
| 10 | mask64_epsilon_greedy | 60 | mask_X44 | epsilon_greedy | 5 | off | off | yes | 0.40 | -26.64 | -28.21 | 5 |
| 11 | mask64_epsilon_greedy | 60 | mask_X63 | epsilon_greedy | 5 | off | off | yes | 0.40 | 53.11 | 53.23 | 5 |
| 12 | mask64_epsilon_greedy | 60 | mask_X62 | epsilon_greedy | 5 | off | off | yes | 0.40 | 39.18 | 38.26 | 5 |
| 13 | mask64_epsilon_greedy | 60 | mask_X55 | epsilon_greedy | 5 | off | off | yes | 0.40 | 3.37 | -259.08 | 5 |
| 14 | mask64_epsilon_greedy | 60 | mask_X54 | epsilon_greedy | 5 | off | off | yes | 0.40 | 1.14 | -24.37 | 5 |
| 15 | mask64_epsilon_greedy | 60 | mask_X51 | epsilon_greedy | 5 | off | off | yes | 0.40 | -8.49 | -12.28 | 5 |
| 16 | mask64_epsilon_greedy | 60 | mask_X50 | epsilon_greedy | 5 | off | off | yes | 0.40 | -26.18 | -107.73 | 5 |
| 17 | mask64_epsilon_greedy | 60 | mask_X52 | epsilon_greedy | 5 | off | off | yes | 0.39 | 8.45 | -14.36 | 5 |
| 18 | mask64_epsilon_greedy | 60 | mask_X48 | epsilon_greedy | 5 | off | off | yes | 0.39 | -7.65 | -18.50 | 5 |
| 19 | mask64_epsilon_greedy | 60 | mask_X53 | epsilon_greedy | 5 | off | off | yes | 0.39 | -27.23 | -183.73 | 5 |
| 20 | mask64_epsilon_greedy | 60 | mask_X49 | epsilon_greedy | 5 | off | off | yes | 0.39 | -30.42 | -59.72 | 5 |

## Notes

- Positive gain means candidate is better than baseline (lower SOC/comp_time).
- Solved guard requires candidate solved count to be >= baseline on every map included in that config.
