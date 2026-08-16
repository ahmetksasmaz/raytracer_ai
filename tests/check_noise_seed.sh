#!/bin/bash
# Noise must be reproducible from its seed, and must actually depend on it.
#
# Before the split, noise came from a thread-local generator seeded from the
# thread id, which is right for a renderer -- every tile gets an independent
# stream with no coordination -- but wrong for a stage that is now its own
# program: two runs over the same input would give different RAWs and a frame
# could never be reproduced once you had looked at it.
#
# Two assertions, and both are needed. "Same seed reproduces" alone would pass
# if the seed were ignored and the noise were disabled entirely; "different
# seeds differ" alone would pass on a generator that ignored the seed and simply
# drew fresh randomness every run.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/bin"
OUT="$ROOT/tests/out"
CFG="$ROOT/tests/configs/noisy.json"
WORK="$OUT/seed"

MOSAIC="$OUT/sensor_noisy_mosaic.exr"
if [ ! -f "$MOSAIC" ]; then
  echo "no $MOSAIC -- the sensor_noisy pipeline must run first"
  exit 1
fi

mkdir -p "$WORK"

"$BIN/sensor_noise" --in "$MOSAIC" --out "$WORK/a.exr" --config "$CFG" --seed 7 > /dev/null || exit 1
"$BIN/sensor_noise" --in "$MOSAIC" --out "$WORK/b.exr" --config "$CFG" --seed 7 > /dev/null || exit 1
"$BIN/sensor_noise" --in "$MOSAIC" --out "$WORK/c.exr" --config "$CFG" --seed 8 > /dev/null || exit 1

echo "seed 7 twice must be bit-identical"
"$BIN/imgdiff" --compare "$WORK/a.exr" "$WORK/b.exr" --tol 1e-12 || exit 1

# Per-pixel, not mean-vs-mean: --expect-differ compares the two images' means,
# and two draws of zero-mean noise over the same signal have almost the same
# mean however different they are pixel by pixel. So the assertion is that a
# strict --compare FAILS.
echo "seed 8 must differ from seed 7 (per-pixel)"
if "$BIN/imgdiff" --compare "$WORK/a.exr" "$WORK/c.exr" --tol 1e-9 > /dev/null 2>&1; then
  echo "FAIL: seeds 7 and 8 produced identical noise -- the seed is being ignored"
  exit 1
fi
echo "PASS: the two seeds give different noise realisations"

exit 0
