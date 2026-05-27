#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SRC_DEFAULT="$ROOT_DIR/../lacam/benchmarks/movingai/data_all"
SRC="${1:-$SRC_DEFAULT}"
DST="$ROOT_DIR/benchmarks/movingai/data_all"
SCEN_DST="$DST/scen-random"

if [[ ! -d "$SRC" ]]; then
  echo "Source data root not found: $SRC" >&2
  echo "Pass source path explicitly: $0 /path/to/data_all" >&2
  exit 1
fi

mkdir -p "$SCEN_DST"

maps=(
  random-32-32-20.map
  den520d.map
  Paris_1_256.map
  warehouse-20-40-10-2-2.map
)

for m in "${maps[@]}"; do
  cp -f "$SRC/$m" "$DST/$m"
done

for prefix in random-32-32-20-random den520d-random Paris_1_256-random warehouse-20-40-10-2-2-random; do
  for i in $(seq 1 25); do
    cp -f "$SRC/scen-random/${prefix}-${i}.scen" "$SCEN_DST/${prefix}-${i}.scen"
  done
done

echo "Bootstrapped focus4 MovingAI data to: $DST"
