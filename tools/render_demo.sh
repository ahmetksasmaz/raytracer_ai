#!/bin/bash
# Renders the spectral-library demo scenes into outputs/spectral_demo/.
#
#   ./tools/render_demo.sh              # everything
#   ./tools/render_demo.sh chart        # only scenes matching "chart"
#
# Run from the repository root: scene asset paths and the spectra/ lookup both
# resolve from the current directory.
set -u

cd "$(dirname "$0")/.." || exit 1

FILTER="${1:-}"
SCENES=$(ls scenes/spectral_demo/*.json 2>/dev/null)
mkdir -p outputs/spectral_demo

if [ ! -x ./raytracer ]; then
  echo "raytracer not built; run 'make release' first" >&2
  exit 1
fi

failed=0
for scene in $SCENES; do
  name=$(basename "$scene" .json)
  if [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]]; then continue; fi

  printf '%-24s ' "$name"
  start=$(date +%s)
  if ./raytracer "$scene" > "outputs/spectral_demo/$name.log" 2>&1; then
    echo "ok  ($(( $(date +%s) - start ))s)"
  else
    echo "FAILED -- see outputs/spectral_demo/$name.log"
    tail -3 "outputs/spectral_demo/$name.log" | sed 's/^/    /'
    failed=$((failed + 1))
  fi
done

echo
echo "outputs in outputs/spectral_demo/"
exit $((failed > 0))
