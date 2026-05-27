# lacam0 Focus5 Map-Aware Suite

- Raw CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_203512_lacam0_focus5_map_aware_suite.csv`
- Mapwise CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_203512_lacam0_focus5_map_aware_suite_mapwise_vs_baseline.csv`

## Aggregate Ranking (solved guard, worst-map SOC, mean SOC, runtime)

| Rank | Config | Solved Guard | Worst Map SOC Gain % | Mean SOC Gain % | Mean Median-CT Gain % | Mean P95-CT Gain % |
|---:|---|---|---:|---:|---:|---:|
| 1 | mapaware_x35_linucb_a08 | yes | 0.00 | 0.54 | 3.17 | -1.01 |
| 2 | mapaware_x35 | yes | 0.00 | 0.45 | 6.04 | 0.70 |
| 3 | mapaware_x36 | yes | 0.00 | 0.40 | -1.25 | -0.76 |
| 4 | mapaware_random_x35 | yes | 0.00 | 0.35 | -0.75 | -8.40 |
| 5 | mapaware_x35_linucb_a10 | yes | -0.06 | 0.21 | 3.77 | 0.67 |
| 6 | mapaware_x35_linucb | yes | -0.25 | 0.38 | 4.88 | -0.78 |
| 7 | mapaware_x35_linucb_a06 | yes | -0.36 | 0.13 | 1.93 | -2.27 |

## Mapwise SOC Gain %

| Config | random | den520d | Paris | wh-2-1 | wh-2-2 |
|---|---:|---:|---:|---:|---:|
| mapaware_random_x35 | 1.75 | 0.00 | 0.00 | 0.00 | 0.00 |
| mapaware_x35 | 2.08 | 0.00 | 0.00 | 0.08 | 0.09 |
| mapaware_x35_linucb | 2.08 | 0.00 | 0.00 | 0.06 | -0.25 |
| mapaware_x35_linucb_a06 | 1.28 | 0.00 | 0.00 | -0.29 | -0.36 |
| mapaware_x35_linucb_a08 | 2.52 | 0.00 | 0.00 | 0.13 | 0.03 |
| mapaware_x35_linucb_a10 | 1.05 | 0.00 | 0.00 | 0.05 | -0.06 |
| mapaware_x36 | 1.75 | 0.00 | 0.00 | 0.10 | 0.14 |
