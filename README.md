# lacam0

[![MIT License](http://img.shields.io/badge/license-MIT-blue.svg?style=flat)](LICENSE)
[![CI](https://github.com/Kei18/lacam0/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Kei18/lacam0/actions/workflows/ci.yml)

This repository provides a basic implementation of [LaCAM](https://kei18.github.io/lacam-project/) for multi-agent pathfinding (MAPF).
The code has been cleaned up and refactored from the original repositories with the aim of making LaCAM easier to extend.
See ["Implemented Techniques"](#implemented-techniques) to find out what has been implemented.
For advanced one, please check the project page.

![](./assets/mapf.gif)

## Building

All you need is [CMake](https://cmake.org/) (≥v3.16).
The code is written in C++(17).

First, clone this repo with submodules.

```sh
git clone --recursive {this repo}
```

Then, build the project.

```sh
cmake -B build && make -C build -j4
```

## Usage

```sh
build/main -i assets/random-32-32-10-random-1.scen -m assets/random-32-32-10.map -N 400 -v 3
```

The result will be saved in `build/result.txt`.

You can find details of all parameters with:

```sh
build/main --help
```

## Adaptive PIBT Bandit Extension (Local Port)

This local branch includes a first port of adaptive PIBT candidate ranking with
bandit policies.

Additional CLI flags:

- `--no_pibt_bandit`
- `--bandit_policy ucb1|thompson|epsilon_greedy|random_uniform`
- `--bandit_epsilon <float>`
- `--bandit_epsilon_final <float>`
- `--bandit_epsilon_decay_steps <int>`

Compatibility flags accepted (currently no-op in this first lacam0 port):

- `--no_order_bandit`
- `--no_branch_bandit`
- `--no_scheduler_bandit`
- `--no_random_bandit`
- `--no_dist_bandit`
- `--no_events_log`
- `--pibt_regret_trials <int>`

### Focus-4 baseline vs Thompson+X32 runner

```sh
./scripts/run_focus4_baseline_vs_x32.sh --parallel "$(( $(nproc) - 2 ))" --time-limit 60
```

Notes:
- By default this script reads MovingAI files from
  `../lacam/benchmarks/movingai/data_all` (your existing data location).
- Outputs are written to `reports/movingai/`.

## Visualizer

This repository is compatible with [kei18@mapf-visualizer](https://github.com/kei18/mapf-visualizer).
For example,

```sh
mapf-visualizer assets/random-32-32-10.map build/result.txt
```

## Implemented Techniques

- tree rewiring (i.e., LaCAM*) [[IJCAI-23]](https://kei18.github.io/lacam2)
- node reinsert [[AAAI-23]](https://kei18.github.io/lacam)
- random restart [[IJCAI-23]](https://kei18.github.io/lacam2)
- non-deterministic node extraction [[AAMAS-24]](https://kei18.github.io/lacam3)
- PIBT swap [[IJCAI-23]](https://kei18.github.io/lacam2)
- hindrance [[SoCS-25]](https://arxiv.org/abs/2505.12623)

### Roadmaps

I may integrate the following techniques:
- regret learning [[SoCS-25]](https://arxiv.org/abs/2505.12623)
- iterative refinement (aka. LNS) [[IROS-21]](https://kei18.github.io/mapf-IR/)

## Experiment Utilities

You can use [kei18@mapf-lib-exp](https://github.com/Kei18/mapf-lib-exp/), written in Julia.

### Setup

```sh
git submodule add git@github.com:Kei18/mapf-lib-exp.git scripts
sh scripts/setup.sh
```

### Usage

```sh
julia --project=scripts/ --threads=auto
> include("scripts/eval.jl"); main("scripts/config/mapf-bench.yaml")
```

The results will be stored in `../data`


## License

This software is released under the MIT License, see [LICENSE.txt](LICENSE.txt).


## Notes

### install pre-commit for formatting

```sh
pre-commit install
```

### simple test

```sh
ctest --test-dir ./build
```
