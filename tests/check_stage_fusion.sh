#!/bin/bash
# The file handover between the sensor stages must not change the numbers.
#
# The arithmetic identity -- that the split stages reproduce the fused
# ElectronsAt exactly -- is asserted in sensortest, where it can be checked in
# double precision over several spectra. What sensortest cannot see is the part
# that only exists once the stages are separate programs: values going out
# through an EXR and back in again, and stages that are supposed to be
# identities actually being identities.
#
# So this checks the seams:
#   1. a noise-free config means sensor_noise must be a no-op
#   2. an unsaturated image means sensor_saturate must be a no-op
#   3. the mosaic must keep its Bayer structure across the write/read boundary
#
# (1) and (2) matter because both stages read the config and decide whether to
# act. A stage that acts when it should not is the easiest way to break a
# pipeline whose output still looks entirely reasonable.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/bin"
OUT="$ROOT/tests/out"
CFG="$ROOT/tests/configs/clean.json"
WORK="$OUT/fusion"

RADIANCE="$OUT/sensor_clean_radiance.exr"
if [ ! -f "$RADIANCE" ]; then
  echo "no $RADIANCE -- the sensor_clean render must run first"
  exit 1
fi

mkdir -p "$WORK"
run() { "$@" > /dev/null || { echo "stage failed: $*"; exit 1; }; }

run "$BIN/sensor_irradiance" --in "$RADIANCE"           --out "$WORK/photons.exr"   --config "$CFG"
run "$BIN/sensor_cfa"        --in "$WORK/photons.exr"   --out "$WORK/electrons.exr" --config "$CFG"
run "$BIN/sensor_mosaic"     --in "$WORK/electrons.exr" --out "$WORK/mosaic.exr"    --config "$CFG"
run "$BIN/sensor_noise"      --in "$WORK/mosaic.exr"    --out "$WORK/noisy.exr"     --config "$CFG" --seed 1
run "$BIN/sensor_saturate"   --in "$WORK/noisy.exr"     --out "$WORK/clamped.exr"   --config "$CFG"

echo "NoiseSources None: sensor_noise must pass the image through unchanged"
"$BIN/imgdiff" --compare "$WORK/mosaic.exr" "$WORK/noisy.exr" --tol 1e-9 || exit 1

echo "signal below full well: sensor_saturate must pass the image through unchanged"
"$BIN/imgdiff" --compare "$WORK/noisy.exr" "$WORK/clamped.exr" --tol 1e-9 || exit 1

echo "mosaic survives the EXR handover with its Bayer structure intact"
"$BIN/imgdiff" --expect-finite "$WORK/mosaic.exr" || exit 1
"$BIN/imgdiff" --expect-nonnegative "$WORK/mosaic.exr" || exit 1
"$BIN/imgdiff" --expect-bayer "$WORK/mosaic.exr" RGGB || exit 1

exit 0
