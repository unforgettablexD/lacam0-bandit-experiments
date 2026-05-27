# lacam0 Focus5 Map-Aware Suite

- Raw CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_191831_lacam0_focus5_map_aware_suite.csv`
- Mapwise CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_191831_lacam0_focus5_map_aware_suite_mapwise_vs_baseline.csv`

## Aggregate Ranking (solved guard, worst-map SOC, mean SOC, runtime)

| Rank | Config | Solved Guard | Worst Map SOC Gain % | Mean SOC Gain % | Mean Median-CT Gain % | Mean P95-CT Gain % |
|---:|---|---|---:|---:|---:|---:|
| 1 | mapaware_x35 | yes | 0.00 | 0.45 | 8.62 | -2.85 |
| 2 | mapaware_x36 | yes | 0.00 | 0.40 | -2.31 | -8.18 |
| 3 | mapaware_random_x35 | yes | 0.00 | 0.35 | -2.69 | -5.63 |
| 4 | mapaware_x35_linucb | yes | -0.14 | 0.26 | 6.16 | -6.26 |

## Mapwise SOC Gain %

| Config | random | den520d | Paris | wh-2-1 | wh-2-2 |
|---|---:|---:|---:|---:|---:|
| mapaware_random_x35 | 1.75 | 0.00 | 0.00 | 0.00 | 0.00 |
| mapaware_x35 | 2.08 | 0.00 | 0.00 | 0.08 | 0.09 |
| mapaware_x35_linucb | 1.50 | 0.00 | 0.00 | -0.14 | -0.06 |
| mapaware_x36 | 1.75 | 0.00 | 0.00 | 0.10 | 0.14 |
