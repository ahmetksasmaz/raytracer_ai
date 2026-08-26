#!/bin/bash
# Renders the six two-illuminant ColorChecker charts and develops each through
# every measured camera.
#
#   ./demo/dual_illuminant/run_demo.sh                 everything
#   ./demo/dual_illuminant/run_demo.sh --sensors 3     the first 3 cameras
#   ./demo/dual_illuminant/run_demo.sh --keep-exr      keep the intermediates
#
# Each scene has two spectrally different lights, one per side, so the
# illuminant varies across the frame. That is the case a single white balance
# gain triple cannot serve, and both corrections are produced side by side:
#
#   6_srgb.png                global gains, calibrated for the LEFT light only
#   9_srgb_ground_truth.png   per-pixel von Kries from the renderer's own
#                             illumination map
#
# On the single-illuminant demo those two agree to within a rounding error. Here
# they should not, and the right half of 6_srgb.png is where to look.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
DEMO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$ROOT/build/bin"

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
  echo "no scenes or sensors. Run: python3 demo/dual_illuminant/generate_demo.py" >&2
  exit 1
fi

# The sensor and ISP chain, shared with the single-illuminant demo.
. "$ROOT/demo/develop.sh"

# Everything this script prints also goes to a log file, so a run that takes
# minutes can be followed with `tail -f` and inspected afterwards. Without it
# the only record of a background run was whatever scrollback happened to
# survive, which is no record at all.
LOG="$DEMO/run.log"
mkdir -p "$DEMO"
exec > >(tee "$LOG") 2>&1
echo "log: $LOG"
echo "started: $(date '+%Y-%m-%d %H:%M:%S')"

mapfile -t PAIRS < <(cd "$DEMO/scenes" && ls chart_*.json | sed 's/^chart_//; s/\.json$//')
mapfile -t SENSORS < <(cd "$DEMO/sensors" && ls *.json | sed 's/\.json$//')
if [ "$SENSOR_LIMIT" -gt 0 ]; then
  SENSORS=("${SENSORS[@]:0:$SENSOR_LIMIT}")
fi

echo "=============================================="
echo " ${#PAIRS[@]} illuminant pairs x ${#SENSORS[@]} sensors = $(( ${#PAIRS[@]} * ${#SENSORS[@]} )) developed images"
echo "=============================================="

cd "$DEMO" || exit 1
failed=0

for pair in "${PAIRS[@]}"; do
  out="rendered_$pair"
  if [ -f "$out/chart_radiance.exr" ]; then
    echo "render $pair ... cached"
    continue
  fi
  mkdir -p "$out"
  printf '[%s] render %-22s ' "$(date '+%H:%M:%S')" "$pair"
  start=$(date +%s)
  if "$BIN/raytracer" "scenes/chart_$pair.json" > "$out/render.log" 2>&1; then
    echo "ok ($(( $(date +%s) - start ))s)"
  else
    echo "FAILED -- see $out/render.log"
    tail -3 "$out/render.log" | sed 's/^/    /'
    failed=$((failed + 1))
  fi
done

[ "$failed" -gt 0 ] && { echo "renders failed; stopping"; exit 1; }

total=$(( ${#PAIRS[@]} * ${#SENSORS[@]} ))
for sensor in "${SENSORS[@]}"; do
  printf '[%s] develop %-30s ' "$(date '+%H:%M:%S')" "$sensor"
  for pair in "${PAIRS[@]}"; do
    # The global white balance has to pick ONE light. It picks the left one --
    # a camera that metered off the left of the frame -- so the right half comes
    # out wrong, which is exactly the failure the per-pixel route avoids.
    if develop "$sensor" "$pair" "${pair%%_*}"; then
      printf '.'
    else
      printf 'X'
      failed=$((failed + 1))
    fi
  done
  clip=$(grep -h 'isp_blacklevel' "$DEMO/${sensor}_${PAIRS[0]}/develop.log" 2>/dev/null |
         grep -o '[0-9.]*% clipped' | head -1)
  printf '  %s\n' "${clip:-}"
done

echo
echo "=============================================="
echo " finished: $(date '+%Y-%m-%d %H:%M:%S')"
if [ "$failed" -gt 0 ]; then
  echo " $failed of $total failed -- see demo/dual_illuminant/*/develop.log"
  exit 1
fi
echo " $total developed, 10 images each"
echo " total:    $(du -sh "$DEMO" | cut -f1)"
echo "=============================================="
