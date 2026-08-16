#!/bin/bash
# Runs the whole pipeline: a scene in, a viewable sRGB image out.
#
# The point of the split is that you rarely want this. Each stage is its own
# program precisely so you can stop at one, look at what came out, change one
# parameter and re-run the rest without re-rendering. This script exists for the
# times you do want the whole thing, and as a readable statement of what the
# stage order actually is.
#
#   ./tools/pipeline.sh <scene.json> [sensor-config.json] [output-dir]
#
# Intermediates are named <scene>_<stage>.<ext> and all of them are left in
# place, because they are the interesting part.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/bin"

SCENE="${1:-}"
SENSOR="${2:-$ROOT/configs/sensors/cmos_generic.json}"
OUT="${3:-$ROOT/outputs/pipeline}"

if [ -z "$SCENE" ]; then
  echo "usage: $0 <scene.json> [sensor-config.json] [output-dir]" >&2
  exit 1
fi
if [ ! -x "$BIN/raytracer" ]; then
  echo "Not built. Run:" >&2
  echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j" >&2
  exit 1
fi
for required in "$SCENE" "$SENSOR"; do
  [ -f "$required" ] || { echo "no such file: $required" >&2; exit 1; }
done

SCENE="$(cd "$(dirname "$SCENE")" && pwd)/$(basename "$SCENE")"
SENSOR="$(cd "$(dirname "$SENSOR")" && pwd)/$(basename "$SENSOR")"
NAME="$(basename "$SCENE" .json)"

mkdir -p "$OUT"
cd "$OUT"

# Relative asset paths in a scene resolve from the working directory, so the
# render runs from the scene's own directory and the result is moved here.
SCENE_DIR="$(dirname "$SCENE")"

step() { echo; echo "--- $* ---"; }

step "render: rays -> spectral radiance"
( cd "$SCENE_DIR" && "$BIN/raytracer" "$SCENE" )
# The renderer names its output from the scene's ImageName, so find what it
# produced rather than assuming it matches the scene's filename.
RADIANCE="$(cd "$SCENE_DIR" && ls -t *_radiance.exr 2>/dev/null | head -1)"
if [ -z "$RADIANCE" ]; then
  echo "the render produced no *_radiance.exr" >&2
  exit 1
fi
[ "$SCENE_DIR" = "$OUT" ] || mv "$SCENE_DIR/$RADIANCE" "$OUT/${NAME}_radiance.exr"

SENSOR_ARGS=(--config "$SENSOR")

step "sensor: radiance -> RAW"
"$BIN/sensor_irradiance" --in "${NAME}_radiance.exr"    --out "${NAME}_photons.exr"     "${SENSOR_ARGS[@]}"
"$BIN/sensor_cfa"        --in "${NAME}_photons.exr"     --out "${NAME}_electrons.exr"   "${SENSOR_ARGS[@]}"
"$BIN/sensor_mosaic"     --in "${NAME}_electrons.exr"   --out "${NAME}_mosaic.exr"      "${SENSOR_ARGS[@]}"
"$BIN/sensor_noise"      --in "${NAME}_mosaic.exr"      --out "${NAME}_noisy.exr"       "${SENSOR_ARGS[@]}" --seed 1
"$BIN/sensor_saturate"   --in "${NAME}_noisy.exr"       --out "${NAME}_wellclamped.exr" "${SENSOR_ARGS[@]}"
"$BIN/sensor_adc"        --in "${NAME}_wellclamped.exr" --out "${NAME}_raw.pgm"         "${SENSOR_ARGS[@]}"

step "calibrate: sensor curves -> white balance gains + colour matrices"
"$BIN/sensor_ccm" "${SENSOR_ARGS[@]}" --out "${NAME}_ccm.json"

step "isp: RAW -> sRGB"
"$BIN/isp_blacklevel"    --in "${NAME}_raw.pgm"         --out "${NAME}_linearized.exr" "${SENSOR_ARGS[@]}"
"$BIN/isp_demosaic"      --in "${NAME}_linearized.exr"  --out "${NAME}_demosaiced.exr" "${SENSOR_ARGS[@]}"
"$BIN/isp_whitebalance"  --in "${NAME}_demosaiced.exr"  --out "${NAME}_wb.exr"         --calibration "${NAME}_ccm.json"
"$BIN/isp_colormatrix"   --in "${NAME}_wb.exr"          --out "${NAME}_xyz.exr"        --calibration "${NAME}_ccm.json" --matrix after_wb
"$BIN/isp_srgb"          --in "${NAME}_xyz.exr"         --out "${NAME}_srgb.png"

step "preview: the RAW made viewable, still uncorrected sensor space"
"$BIN/raw_preview" --in "${NAME}_raw.pgm" --out "${NAME}_raw_gamma.png"        "${SENSOR_ARGS[@]}"
"$BIN/raw_preview" --in "${NAME}_raw.pgm" --out "${NAME}_raw_mosaic_gamma.png" "${SENSOR_ARGS[@]}" --mosaic

echo
echo "done: $OUT/${NAME}_srgb.png"
echo "intermediates left in place; ${NAME}_radiance.exr can be replayed through another sensor without re-rendering."
