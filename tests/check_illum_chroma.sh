#!/bin/bash
# The per-pixel illuminant map, checked against what the calibration already
# knows, and then checked for actually being used.
#
# Three parts, mirroring check_wb_ccm.sh: a closed form, an exact degenerate
# case, and an anti-vacuity guard.
#
# The closed form is the good one. sensor_ccm derives the white balance gains
# analytically from the D65 spectrum and the sensor's curves, and those gains
# are by definition the reciprocals of that illuminant's sensor chromaticity:
#
#     wb_gains[0] = 1 / (r/g)      wb_gains[2] = 1 / (b/g)
#
# The chromaticity map arrives at the same numbers by a completely different
# route -- the renderer transporting light through a scene, splatting it into a
# film, and integrating the result against the same curves. Agreement between
# the two is a statement about the whole path tracer, not just about this stage.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/bin"
OUT="$ROOT/tests/out"
CFG="$ROOT/tests/configs/clean.json"
WORK="$OUT/chroma"

MAP="$OUT/sensor_clean_illumchroma.exr"
DEMOSAICED="$OUT/sensor_clean_demosaiced.exr"
ILLUMINATION="$OUT/sensor_clean_illumination.exr"
for f in "$MAP" "$DEMOSAICED" "$ILLUMINATION"; do
  if [ ! -f "$f" ]; then
    echo "no $f -- the sensor_clean pipeline must run first"
    exit 1
  fi
done

mkdir -p "$WORK"
"$BIN/sensor_ccm" --config "$CFG" --illuminant D65 --out "$WORK/ccm.json" > /dev/null || exit 1

# The reciprocals of the calibrated gains: what the map must contain.
WANT="$(python3 -c "
import json
g = json.load(open('$WORK/ccm.json'))['wb_gains']
print(1.0/g[0], 1.0/g[2])
")" || exit 1
WANT_RG="${WANT% *}"
WANT_BG="${WANT#* }"

# sensor_clean is a closed box lit by a single D65 emitter, so the ground-truth
# illuminant is the same everywhere and the map must be constant at that value.
echo "the map must be constant at D65's chromaticity through this sensor"
echo "  expected r/g=$WANT_RG  b/g=$WANT_BG (reciprocals of the calibrated gains)"
"$BIN/imgdiff" --expect-pair "$MAP" "$WANT_RG" "$WANT_BG" --tol 1e-5 || exit 1

# --- Degenerate case: per-pixel with a constant map == global gains ----------
# Exact, not statistical. Both routes apply the same three numbers; if they
# disagree at all, the per-pixel path is not the von Kries it claims to be.
"$BIN/isp_whitebalance" --in "$DEMOSAICED" --out "$WORK/pixel_wb.exr" \
    --chroma "$MAP" > /dev/null || exit 1
"$BIN/isp_whitebalance" --in "$DEMOSAICED" --out "$WORK/global_wb.exr" \
    --calibration "$WORK/ccm.json" > /dev/null || exit 1

echo "a constant map must reproduce the global calibrated gains exactly"
"$BIN/imgdiff" --compare "$WORK/pixel_wb.exr" "$WORK/global_wb.exr" --tol 1e-9 || exit 1

# --- Anti-vacuity: the map's CONTENT must drive the correction ---------------
# Without this, the equality above would pass just as well if the map were being
# read and then thrown away.
#
# The second map comes from the same illumination cube through a different
# camera. Chromaticity is a property of the light AND the sensor looking at it,
# so a measured Nikon CFA reports different ratios for the very same light --
# which is also the reason this stage is sensor-side rather than in the renderer.
NIKON="$ROOT/configs/sensors/nikon_d700.json"
if [ -f "$NIKON" ]; then
  "$BIN/sensor_illum_chroma" --in "$ILLUMINATION" --out "$WORK/nikon_map.exr" \
      --config "$NIKON" > /dev/null || exit 1

  echo "the same light through a different camera must give a different map"
  if "$BIN/imgdiff" --compare "$MAP" "$WORK/nikon_map.exr" --tol 1e-6 > /dev/null 2>&1; then
    echo "FAIL: two different sensors produced the same chromaticity map"
    exit 1
  fi

  "$BIN/isp_whitebalance" --in "$DEMOSAICED" --out "$WORK/nikon_wb.exr" \
      --chroma "$WORK/nikon_map.exr" > /dev/null || exit 1

  # Per-pixel, not mean-vs-mean: --expect-differ compares the two images' means,
  # which a correction that redistributes colour without changing overall
  # brightness can slip through. Same trap check_noise_seed.sh documents.
  echo "and therefore a different corrected image"
  if "$BIN/imgdiff" --compare "$WORK/pixel_wb.exr" "$WORK/nikon_wb.exr" --tol 1e-9 > /dev/null 2>&1; then
    echo "FAIL: the chromaticity map is read but not applied"
    exit 1
  fi
  echo "PASS: the map's content drives the correction"
else
  echo "skipped the cross-camera half: $NIKON not present"
fi

exit 0
