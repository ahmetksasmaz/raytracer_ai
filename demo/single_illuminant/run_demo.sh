#!/bin/bash
# Renders the five ColorChecker charts and develops each through every measured
# camera in the spectral library.
#
#   ./demo/single_illuminant/run_demo.sh                 everything
#   ./demo/single_illuminant/run_demo.sh --sensors 3     the first 3 cameras only, for a quick look
#   ./demo/single_illuminant/run_demo.sh --keep-exr      keep the intermediate EXRs as well
#
# Outputs, all gitignored and regenerable:
#
#   demo/single_illuminant/rendered_<illuminant>/          the spectral cubes, one render each
#   demo/single_illuminant/<sensor>_<illuminant>/          nine PNGs + the RAW and the calibration
#
# The nine are the same scene seen at nine points along the pipeline, from what
# the sensor physically recorded to what a display should show.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEMO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$ROOT/build/bin"

# Pin the library explicitly rather than relying on the relative search order,
# which depends on where this is invoked from.
export RAYTRACER_SPECTRA_DIR="$ROOT/spectra"

SENSOR_LIMIT=0
KEEP_EXR=0
while [ $# -gt 0 ]; do
  case "$1" in
    --sensors) SENSOR_LIMIT="$2"; shift 2 ;;
    --keep-exr) KEEP_EXR=1; shift ;;
    *) echo "unknown option: $1" >&2; exit 1 ;;
  esac
done

if [ ! -x "$BIN/raytracer" ]; then
  echo "not built. Run:" >&2
  echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j" >&2
  exit 1
fi
if [ ! -d "$DEMO/scenes" ] || [ ! -d "$DEMO/sensors" ]; then
  echo "no scenes or sensors. Run: python3 demo/single_illuminant/generate_demo.py" >&2
  exit 1
fi

ILLUMINANTS=(d65 incandescent fl11 led_b3 hps)
mapfile -t SENSORS < <(cd "$DEMO/sensors" && ls *.json | sed 's/\.json$//')
if [ "$SENSOR_LIMIT" -gt 0 ]; then
  SENSORS=("${SENSORS[@]:0:$SENSOR_LIMIT}")
fi

echo "=============================================="
echo " ${#ILLUMINANTS[@]} illuminants x ${#SENSORS[@]} sensors = $(( ${#ILLUMINANTS[@]} * ${#SENSORS[@]} )) developed images"
echo "=============================================="

cd "$DEMO" || exit 1
failed=0

# --- 1. Render ---------------------------------------------------------------
# The expensive half, and the reason the pipeline is split: each scene is
# rendered ONCE and then replayed through every sensor.
for illum in "${ILLUMINANTS[@]}"; do
  out="rendered_$illum"
  if [ -f "$out/chart_radiance.exr" ]; then
    echo "render $illum ... cached"
    continue
  fi
  mkdir -p "$out"
  printf 'render %-14s ' "$illum"
  start=$(date +%s)
  if "$BIN/raytracer" "scenes/chart_$illum.json" > "$out/render.log" 2>&1; then
    echo "ok ($(( $(date +%s) - start ))s)"
  else
    echo "FAILED -- see $out/render.log"
    tail -3 "$out/render.log" | sed 's/^/    /'
    failed=$((failed + 1))
  fi
done

[ "$failed" -gt 0 ] && { echo "renders failed; stopping"; exit 1; }

# --- 2. Develop --------------------------------------------------------------
# The sensor and ISP chain lives in demo/develop.sh, shared with the
# two-illuminant demo so the metering and calibration cannot drift apart.
. "$ROOT/demo/develop.sh"

total=$(( ${#ILLUMINANTS[@]} * ${#SENSORS[@]} ))
done_count=0
for sensor in "${SENSORS[@]}"; do
  printf 'develop %-30s ' "$sensor"
  for illum in "${ILLUMINANTS[@]}"; do
    if develop "$sensor" "$illum" "$illum"; then
      printf '.'
    else
      printf 'X'
      failed=$((failed + 1))
    fi
    done_count=$((done_count + 1))
  done
  # The clipping percentage is the number that says whether the exposure is
  # right; the picture will look fine either way.
  clip=$(grep -h 'isp_blacklevel' "$DEMO/${sensor}_d65/develop.log" 2>/dev/null |
         grep -o '[0-9.]*% clipped' | head -1)
  printf '  %s\n' "${clip:-}"
done

echo
echo "=============================================="
if [ "$failed" -gt 0 ]; then
  echo " $failed of $total failed -- see demo/single_illuminant/*/develop.log"
  exit 1
fi
echo " $total developed, 9 images each"
echo " renders:  $(du -sh "$DEMO"/rendered_* 2>/dev/null | awk '{s+=$1} END {print NR" dirs"}')"
echo " total:    $(du -sh "$DEMO" | cut -f1)"
echo "=============================================="
