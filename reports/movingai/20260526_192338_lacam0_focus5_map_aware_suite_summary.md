# lacam0 Focus5 Map-Aware Suite

- Raw CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_192338_lacam0_focus5_map_aware_suite.csv`
- Mapwise CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_192338_lacam0_focus5_map_aware_suite_mapwise_vs_baseline.csv`

## Aggregate Ranking (solved guard, worst-map SOC, mean SOC, runtime)

| Rank | Config | Solved Guard | Worst Map SOC Gain % | Mean SOC Gain % | Mean Median-CT Gain % | Mean P95-CT Gain % |
|---:|---|---|---:|---:|---:|---:|
| 1 | mapaware_x35 | yes | 0.00 | 0.45 | 6.58 | -4.19 |
| 2 | mapaware_x36 | yes | 0.00 | 0.40 | -2.09 | -1.04 |
| 3 | mapaware_random_x35 | yes | 0.00 | 0.35 | 0.56 | -5.38 |
| 4 | mapaware_x35_linucb_a08 | yes | -0.01 | 0.64 | 6.89 | -4.45 |
| 5 | mapaware_x35_linucb_a10 | yes | -0.05 | 0.29 | 5.00 | -2.42 |
| 6 | mapaware_x35_linucb | yes | -0.14 | 0.26 | 4.13 | -3.40 |
| 7 | mapaware_x35_linucb_a06 | yes | -0.20 | 0.44 | 6.96 | -5.73 |

## Mapwise SOC Gain %

| Config | random | den520d | Paris | wh-2-1 | wh-2-2 |
|---|---:|---:|---:|---:|---:|
| mapaware_random_x35 | 1.75 | 0.00 | 0.00 | 0.00 | 0.00 |
| mapaware_x35 | 2.08 | 0.00 | 0.00 | 0.08 | 0.09 |
| mapaware_x35_linucb | 1.50 | 0.00 | 0.00 | -0.14 | -0.06 |
| mapaware_x35_linucb_a06 | 2.56 | 0.00 | 0.00 | -0.18 | -0.20 |
| mapaware_x35_linucb_a08 | 3.14 | 0.00 | 0.00 | 0.07 | -0.01 |
| mapaware_x35_linucb_a10 | 1.48 | 0.00 | 0.00 | 0.01 | -0.05 |
| mapaware_x36 | 1.75 | 0.00 | 0.00 | 0.10 | 0.14 |
