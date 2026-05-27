# lacam0 Focus5 Map-Aware Suite

- Raw CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_184756_lacam0_focus5_reward_weight_tuning.csv`
- Mapwise CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_184756_lacam0_focus5_reward_weight_tuning_mapwise_vs_baseline.csv`

## Aggregate Ranking (solved guard, worst-map SOC, mean SOC, runtime)

| Rank | Config | Solved Guard | Worst Map SOC Gain % | Mean SOC Gain % | Mean Median-CT Gain % | Mean P95-CT Gain % |
|---:|---|---|---:|---:|---:|---:|
| 1 | w_balA | yes | 0.00 | 0.45 | 7.88 | 6.94 |
| 2 | w_base | yes | 0.00 | 0.40 | 6.22 | 7.21 |
| 3 | w_socA | yes | -0.04 | 0.30 | 5.94 | 6.05 |
| 4 | w_balB | yes | -0.07 | 0.28 | 6.81 | 6.85 |
| 5 | w_socB | yes | -0.13 | 0.37 | 6.40 | 3.00 |
| 6 | w_smooth | yes | -0.14 | 0.20 | 5.15 | 2.82 |

## Mapwise SOC Gain %

| Config | random | den520d | Paris | wh-2-1 | wh-2-2 |
|---|---:|---:|---:|---:|---:|
| w_balA | 2.08 | 0.00 | 0.00 | 0.08 | 0.09 |
| w_balB | 1.44 | 0.00 | 0.00 | 0.05 | -0.07 |
| w_base | 1.75 | 0.00 | 0.00 | 0.10 | 0.14 |
| w_smooth | 0.97 | 0.00 | 0.00 | -0.14 | 0.16 |
| w_socA | 1.57 | 0.00 | 0.00 | -0.04 | -0.02 |
| w_socB | 2.08 | 0.00 | 0.00 | -0.12 | -0.13 |
