#!/bin/bash
# White balance and the colour matrix must be used as the pair they were fitted
# as.
#
# Both correct for the same thing: that the sensor's three channels are not the
# eye's. sensor_ccm therefore fits two matrices from one set of training data --
# one for data that has been through the white balance gains, one for data that
# has not -- and the ISP must pick the one matching what it actually did.
#
# The failure this guards against is silent. Applying the gains and then the
# un-balanced matrix double-corrects; skipping the gains and using the balanced
# matrix under-corrects. Both produce a perfectly plausible picture with a
# colour cast nobody can see without a reference, which is exactly the kind of
# error a rendering test suite normally misses.
#
# The assertion is an invariance rather than a threshold: two routes through the
# ISP that describe the same physics must agree.
#
#   route A:  demosaiced -> white balance -> matrix_after_wb -> XYZ
#   route B:  demosaiced ->                  matrix_no_wb    -> XYZ
#
# Both are the same linear map composed differently, so they must produce the
# same XYZ. If they do not, the two fits have drifted apart.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/bin"
OUT="$ROOT/tests/out"
CFG="$ROOT/tests/configs/clean.json"
WORK="$OUT/wbccm"

DEMOSAICED="$OUT/sensor_clean_demosaiced.exr"
if [ ! -f "$DEMOSAICED" ]; then
  echo "no $DEMOSAICED -- the sensor_clean pipeline must run first"
  exit 1
fi

mkdir -p "$WORK"

"$BIN/sensor_ccm" --config "$CFG" --out "$WORK/ccm.json" > /dev/null || exit 1

# Route A: gains, then the matrix fitted for balanced data.
"$BIN/isp_whitebalance" --in "$DEMOSAICED" --out "$WORK/wb.exr" \
    --calibration "$WORK/ccm.json" > /dev/null || exit 1
"$BIN/isp_colormatrix"  --in "$WORK/wb.exr" --out "$WORK/a_xyz.exr" \
    --calibration "$WORK/ccm.json" --matrix after_wb > /dev/null || exit 1

# Route B: no gains, and the matrix fitted for unbalanced data.
"$BIN/isp_colormatrix"  --in "$DEMOSAICED" --out "$WORK/b_xyz.exr" \
    --calibration "$WORK/ccm.json" --matrix no_wb > /dev/null || exit 1

echo "wb + matrix_after_wb must equal matrix_no_wb alone"
"$BIN/imgdiff" --compare "$WORK/a_xyz.exr" "$WORK/b_xyz.exr" --tol 0.02 || exit 1

# And the pairing must actually matter: using the wrong matrix for the data has
# to change the result, or the two fits are the same matrix and the whole
# distinction is decorative.
"$BIN/isp_colormatrix" --in "$WORK/wb.exr" --out "$WORK/wrong_xyz.exr" \
    --calibration "$WORK/ccm.json" --matrix no_wb > /dev/null || exit 1

echo "mismatching them must NOT agree -- otherwise the pairing is meaningless"
"$BIN/imgdiff" --expect-differ "$WORK/a_xyz.exr" "$WORK/wrong_xyz.exr" --tol 0.01 || exit 1

exit 0
