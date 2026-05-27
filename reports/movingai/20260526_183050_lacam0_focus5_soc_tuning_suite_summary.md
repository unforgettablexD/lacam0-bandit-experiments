# lacam0 Focus5 Map-Aware Suite

- Raw CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_183050_lacam0_focus5_soc_tuning_suite.csv`
- Mapwise CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_183050_lacam0_focus5_soc_tuning_suite_mapwise_vs_baseline.csv`

## Aggregate Ranking (solved guard, worst-map SOC, mean SOC, runtime)

| Rank | Config | Solved Guard | Worst Map SOC Gain % | Mean SOC Gain % | Mean Median-CT Gain % | Mean P95-CT Gain % |
|---:|---|---|---:|---:|---:|---:|
| 1 | x35_eps005_r5 | yes | 0.00 | 0.40 | 4.98 | -5.56 |
| 2 | x35_softmax_r5 | yes | -0.02 | 0.15 | -1.54 | 2.59 |
| 3 | x35_eps002_r5 | yes | -0.12 | 0.34 | 2.61 | -8.86 |
| 4 | x35_softmax_r9 | yes | -0.15 | 0.34 | 1.38 | -3.17 |
| 5 | x35_thompson_r5 | yes | -0.16 | 0.23 | 3.54 | -4.84 |

## Mapwise SOC Gain %

| Config | random | den520d | Paris | wh-2-1 | wh-2-2 |
|---|---:|---:|---:|---:|---:|
| x35_eps002_r5 | 1.87 | 0.00 | 0.00 | -0.03 | -0.12 |
| x35_eps005_r5 | 1.75 | 0.00 | 0.00 | 0.10 | 0.14 |
| x35_softmax_r5 | 0.67 | 0.00 | 0.00 | 0.08 | -0.02 |
| x35_softmax_r9 | 1.79 | 0.00 | 0.00 | 0.07 | -0.15 |
| x35_thompson_r5 | 1.39 | 0.00 | 0.00 | -0.10 | -0.16 |
