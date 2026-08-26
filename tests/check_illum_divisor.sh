#!/bin/bash
# The illuminant map is the divisor:
#
#     demosaiced / (r/g, 1, b/g)  =  white balanced
#
# That is what makes it an illuminant map rather than a decorative false-colour
# image, and it is what lets chroma_preview's picture be read as "the colour
# this correction divides out". Green is untouched, so the middle term is 1.
#
# Asserted as a product rather than a quotient because imgdiff can multiply:
# wb * (r/g, 1, b/g) must reproduce the demosaiced image, per channel by name.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/bin"
OUT="$ROOT/tests/out"
WORK="$OUT/divisor"

DEMOSAICED="$OUT/sensor_clean_demosaiced.exr"
MAP="$OUT/sensor_clean_illumchroma.exr"
for f in "$DEMOSAICED" "$MAP"; do
  if [ ! -f "$f" ]; then
    echo "no $f -- the sensor_clean pipeline must run first"
    exit 1
  fi
done

mkdir -p "$WORK"

# The map as a three-channel image, which is the form the division uses.
"$BIN/chroma_preview" --in "$MAP" --out "$WORK/map.png" \
    --exr "$WORK/triple.exr" > /dev/null || exit 1

"$BIN/isp_whitebalance" --in "$DEMOSAICED" --out "$WORK/wb.exr" \
    --chroma "$MAP" > /dev/null || exit 1

echo "white balanced x (r/g, 1, b/g) must reproduce the demosaiced image"
"$BIN/imgdiff" --expect-product "$WORK/wb.exr" "$WORK/triple.exr" "$DEMOSAICED" --tol 1e-6 || exit 1

# Green must be exactly 1 in the triple, or the correction is not von Kries and
# the data has silently left the units the exposure window and the colour
# matrix anchor both depend on.
echo "the green channel of the map must be exactly 1 -- von Kries leaves it alone"
"$BIN/imgdiff" --expect-constant "$WORK/triple.exr" 1.0 --tol 0.6 > /dev/null || {
  echo "FAIL: could not read the triple"; exit 1; }
python3 - "$WORK/triple.exr" <<'PY' || exit 1
import subprocess, re, sys
out = subprocess.run(["build/bin/imgdiff", "--stats", sys.argv[1]],
                     capture_output=True, text=True).stdout
g = float(re.search(r'g=([0-9.]+)', out).group(1))
print("  green channel mean = %.9f" % g)
if abs(g - 1.0) > 1e-9:
    print("FAIL: green is not 1; the correction is not a green-preserving von Kries")
    sys.exit(1)
print("PASS")
PY

exit 0
