# lacam0 Focus4 Improvement Pack

- Raw CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_164149_lacam0_focus5_mask64_sweep.csv`
- Mapwise CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_164149_lacam0_focus5_mask64_sweep_mapwise.csv`

## Ranking (solved guard first, then SOC gain, then runtime gains)

| Rank | Track | Time | Profile | Policy | Regret | Autoscale | Learning | Solved Guard | Mean SOC Gain % | Mean Median-CT Gain % | Mean P95-CT Gain % | Maps |
|---:|---|---:|---|---|---:|---|---|---|---:|---:|---:|---:|
| 1 | mask64_epsilon_greedy | 60 | mask_X36 | epsilon_greedy | 5 | off | off | yes | 0.41 | 11.95 | -13.71 | 5 |
| 2 | mask64_epsilon_greedy | 60 | mask_X37 | epsilon_greedy | 5 | off | off | yes | 0.41 | 6.41 | -7.34 | 5 |
| 3 | mask64_epsilon_greedy | 60 | mask_X35 | epsilon_greedy | 5 | off | off | yes | 0.41 | 5.91 | 1.62 | 5 |
| 4 | mask64_epsilon_greedy | 60 | mask_X34 | epsilon_greedy | 5 | off | off | yes | 0.41 | -6.07 | -12.32 | 5 |
| 5 | mask64_epsilon_greedy | 60 | mask_X33 | epsilon_greedy | 5 | off | off | yes | 0.41 | 8.47 | 5.10 | 5 |
| 6 | mask64_epsilon_greedy | 60 | mask_X32 | epsilon_greedy | 5 | off | off | yes | 0.41 | -14.72 | -21.62 | 5 |
| 7 | mask64_epsilon_greedy | 60 | mask_X43 | epsilon_greedy | 5 | off | off | yes | 0.40 | 21.39 | 12.38 | 5 |
| 8 | mask64_epsilon_greedy | 60 | mask_X41 | epsilon_greedy | 5 | off | off | yes | 0.40 | 9.63 | 0.96 | 5 |
| 9 | mask64_epsilon_greedy | 60 | mask_X40 | epsilon_greedy | 5 | off | off | yes | 0.40 | -8.59 | -18.06 | 5 |
| 10 | mask64_epsilon_greedy | 60 | mask_X42 | epsilon_greedy | 5 | off | off | yes | 0.40 | -76.18 | -129.64 | 5 |
| 11 | mask64_epsilon_greedy | 60 | mask_X39 | epsilon_greedy | 5 | off | off | yes | 0.40 | 8.35 | 0.10 | 5 |
| 12 | mask64_epsilon_greedy | 60 | mask_X38 | epsilon_greedy | 5 | off | off | yes | 0.40 | -12.37 | -20.64 | 5 |

## Notes

- Positive gain means candidate is better than baseline (lower SOC/comp_time).
- Solved guard requires candidate solved count to be >= baseline on every map included in that config.
