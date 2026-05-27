# lacam0 Focus5 Map-Aware Suite

- Raw CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_181152_lacam0_focus5_map_aware_suite.csv`
- Mapwise CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_181152_lacam0_focus5_map_aware_suite_mapwise_vs_baseline.csv`

## Aggregate Ranking (solved guard, worst-map SOC, mean SOC, runtime)

| Rank | Config | Solved Guard | Worst Map SOC Gain % | Mean SOC Gain % | Mean Median-CT Gain % | Mean P95-CT Gain % |
|---:|---|---|---:|---:|---:|---:|
| 1 | mapaware_x35 | yes | 0.00 | 0.35 | 7.98 | 2.53 |
| 2 | mapaware_x36 | yes | 0.00 | 0.35 | -4.07 | -1.89 |
| 3 | mapaware_random_x35 | yes | 0.00 | 0.34 | -1.99 | -5.02 |

## Mapwise SOC Gain %

| Config | random | den520d | Paris | wh-2-1 | wh-2-2 |
|---|---:|---:|---:|---:|---:|
| mapaware_random_x35 | 1.71 | 0.00 | 0.00 | 0.00 | 0.00 |
| mapaware_x35 | 1.71 | 0.00 | 0.00 | 0.02 | 0.04 |
| mapaware_x36 | 1.71 | 0.00 | 0.00 | 0.02 | 0.04 |
