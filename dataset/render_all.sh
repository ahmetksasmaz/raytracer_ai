#!/bin/bash
# Renders the dataset. This is the long one -- about 7 hours for 100 scenes at
# 512x512 and 512 spp -- so it is built to be left running and come back to.
#
#   ./dataset/render_all.sh                 all 100
#   ./dataset/render_all.sh --scenes 5      the first 5, to check before committing hours
#   ./dataset/render_all.sh --samples 256   regenerate at a different budget, then render
#   ./dataset/render_all.sh --force         re-render scenes that are already done
#
# RESUMABLE: a scene whose _radiance.exr already exists is skipped. A run this
# long will be interrupted, and restarting from zero would be unusable.
#
# Everything is also written to dataset/render.log with timestamps, so a run
# started in the background leaves a record rather than relying on scrollback.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATASET="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$ROOT/build/bin"

# Pin the spectral library rather than relying on the relative search order,
# which depends on where this is invoked from. Every scene references library
# illuminants by _ref, and an unresolvable ref is a hard error.
export RAYTRACER_SPECTRA_DIR="$ROOT/spectra"

LIMIT=0
FORCE=0
SAMPLES=""
while [ $# -gt 0 ]; do
  case "$1" in
    --scenes) LIMIT="$2"; shift 2 ;;
    --samples) SAMPLES="$2"; shift 2 ;;
    --force) FORCE=1; shift ;;
    *) echo "unknown option: $1" >&2; exit 1 ;;
  esac
done

if [ ! -x "$BIN/raytracer" ]; then
  echo "not built. Run:" >&2
  echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j" >&2
  exit 1
fi

if [ -n "$SAMPLES" ]; then
  echo "regenerating scenes at $SAMPLES spp"
  python3 "$DATASET/generate_dataset.py" --samples "$SAMPLES" || exit 1
fi

if [ ! -d "$DATASET/scenes" ]; then
  echo "no scenes. Run: python3 dataset/generate_dataset.py" >&2
  exit 1
fi

mkdir -p "$DATASET/renders"
LOG="$DATASET/render.log"
exec > >(tee -a "$LOG") 2>&1

mapfile -t SCENES < <(cd "$DATASET/scenes" && ls scene_*.json | sed 's/\.json$//')
[ "$LIMIT" -gt 0 ] && SCENES=("${SCENES[@]:0:$LIMIT}")

# What the scenes were generated at, read from the first one rather than
# assumed -- --samples may have changed it since.
read -r RES SPP < <(python3 -c "
import json,sys
c=json.load(open('$DATASET/scenes/${SCENES[0]}.json'))['Scene']['Cameras']['Camera'][0]
print(c['ImageResolution'].replace(' ','x'), c['NumSamples'])")

echo "=============================================================="
echo " dataset render -- ${#SCENES[@]} scenes at $RES, $SPP spp"
echo " started: $(date '+%Y-%m-%d %H:%M:%S')"
echo " log:     $LOG"
echo "=============================================================="

start_all=$(date +%s)
done_count=0
skipped=0
failed=0

for name in "${SCENES[@]}"; do
  target="$DATASET/renders/${name}_radiance.exr"
  if [ -f "$target" ] && [ "$FORCE" -eq 0 ]; then
    skipped=$((skipped + 1))
    continue
  fi

  start=$(date +%s)
  printf '[%s] %s ... ' "$(date '+%H:%M:%S')" "$name"

  if "$BIN/raytracer" "$DATASET/scenes/$name.json" > "$DATASET/renders/$name.log" 2>&1; then
    elapsed=$(( $(date +%s) - start ))
    done_count=$((done_count + 1))

    # An ETA from the mean of what has actually been rendered this run, not
    # from a guess. Over seven hours the estimate is the useful part.
    total_elapsed=$(( $(date +%s) - start_all ))
    mean=$(( total_elapsed / done_count ))
    remaining=$(( ${#SCENES[@]} - skipped - done_count ))
    eta=$(( mean * remaining ))
    printf 'ok %4ds   [%d/%d]  mean %ds  eta %dh%02dm\n' \
      "$elapsed" "$((done_count + skipped))" "${#SCENES[@]}" "$mean" \
      "$((eta / 3600))" "$(( (eta % 3600) / 60 ))"
  else
    failed=$((failed + 1))
    printf 'FAILED -- see renders/%s.log\n' "$name"
    tail -3 "$DATASET/renders/$name.log" | sed 's/^/    /'
  fi
done

total=$(( $(date +%s) - start_all ))
echo "=============================================================="
echo " finished: $(date '+%Y-%m-%d %H:%M:%S')"
printf ' %d rendered, %d already done, %d failed, in %dh%02dm\n' \
  "$done_count" "$skipped" "$failed" "$((total / 3600))" "$(( (total % 3600) / 60 ))"
echo " output:   $(du -sh "$DATASET/renders" 2>/dev/null | cut -f1) in dataset/renders/"
echo "=============================================================="
[ "$failed" -eq 0 ]
