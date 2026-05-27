# lacam0 Focus5 Baseline vs Map-Aware X35

- Raw CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_215441_lacam0_focus5_map_aware_vs_baseline.csv`
- Mapwise CSV: `/home/nicolas/Desktop/codes/phd projects/lacam0/reports/movingai/20260526_215441_lacam0_focus5_map_aware_vs_baseline_mapwise_vs_baseline.csv`

Map-aware policy:
- `den520d` and `Paris_1_256`: baseline (all bandits off)
- `random-32-32-20` and both `warehouse` maps: X35

| Map | Agents | Baseline Solved | Map-Aware Solved | Baseline SOC (Solved-Only) | Map-Aware SOC (Solved-Only) | SOC Gain % | Median CT Gain % | P95 CT Gain % |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| random-32-32-20 | 300 | 25/25 | 25/25 | 15506.24 | 15116.24 | 2.52% | -7.35% | -27.63% |
| den520d | 700 | 25/25 | 25/25 | 152719.08 | 152719.08 | 0.00% | 3.51% | -8.60% |
| Paris_1_256 | 1000 | 25/25 | 25/25 | 238097.92 | 238097.92 | 0.00% | 2.81% | -2.88% |
| warehouse-20-40-10-2-1 | 1000 | 25/25 | 25/25 | 269163.44 | 268817.36 | 0.13% | 10.06% | -11.27% |
| warehouse-20-40-10-2-2 | 1000 | 25/25 | 25/25 | 247740.60 | 247669.88 | 0.03% | -1.43% | 0.35% |
