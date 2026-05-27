# lacam0 Focus5 Map-Aware Suite

- Raw CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_175745_lacam0_focus5_map_aware_suite.csv`
- Mapwise CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_175745_lacam0_focus5_map_aware_suite_mapwise_vs_baseline.csv`

## Aggregate Ranking (solved guard, worst-map SOC, mean SOC, runtime)

| Rank | Config | Solved Guard | Worst Map SOC Gain % | Mean SOC Gain % | Mean Median-CT Gain % | Mean P95-CT Gain % |
|---:|---|---|---:|---:|---:|---:|
| 1 | mapaware_x35 | yes | 0.00 | 0.46 | 7.18 | -0.21 |
| 2 | mapaware_x36 | yes | 0.00 | 0.46 | -0.54 | -17.49 |
| 3 | mapaware_random_x35 | yes | 0.00 | 0.44 | 3.87 | -9.26 |

## Mapwise SOC Gain %

| Config | random | den520d | Paris | wh-2-1 | wh-2-2 |
|---|---:|---:|---:|---:|---:|
| mapaware_random_x35 | 2.22 | 0.00 | 0.00 | 0.00 | 0.00 |
| mapaware_x35 | 2.22 | 0.00 | 0.00 | 0.00 | 0.07 |
| mapaware_x36 | 2.22 | 0.00 | 0.00 | 0.00 | 0.07 |
