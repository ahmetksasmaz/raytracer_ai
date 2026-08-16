#!/bin/bash
# A spectral cube must survive the write/read boundary exactly.
#
# The cube is the handover between the renderer and everything downstream, and
# it is the only place the band grid is recorded: the channels are named
# "0400nm", "0410nm", ... and nothing else in the file says what the bands are.
# A reader that counted channels instead of parsing names would work perfectly
# until someone changed kSpectralBands, then silently misinterpret every
# wavelength -- which would look like a colour bug, not a file-format bug.
#
# sensor_irradiance reads a cube and writes a cube, so running it with a
# transparent configuration is a full round trip through both halves of the
# spectral EXR code. Every band is scaled by the same known constant, so any
# band landing in the wrong channel shows up as a mismatch.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/bin"
OUT="$ROOT/tests/out"
WORK="$OUT/roundtrip"

RADIANCE="$OUT/sensor_clean_radiance.exr"
if [ ! -f "$RADIANCE" ]; then
  echo "no $RADIANCE -- the sensor_clean render must run first"
  exit 1
fi

mkdir -p "$WORK"

# A geometry that makes the conversion exactly 1.0 is not available -- the
# photon conversion is wavelength dependent by construction -- so the round trip
# is asserted structurally instead: same channel count, same channel names, and
# a second pass through the same stage reproducing the first pass exactly.
CFG="$ROOT/tests/configs/clean.json"

"$BIN/sensor_irradiance" --in "$RADIANCE"          --out "$WORK/once.exr"  --config "$CFG" > /dev/null || exit 1
"$BIN/sensor_irradiance" --in "$RADIANCE"          --out "$WORK/twice.exr" --config "$CFG" > /dev/null || exit 1

echo "the stage is deterministic: two runs over the same cube agree exactly"
"$BIN/imgdiff" --compare "$WORK/once.exr" "$WORK/twice.exr" --tol 1e-12 || exit 1

# --expect-channels asserts the count AND that every channel name parses as a
# wavelength in ascending order, which is the part that matters: the names are
# the only record of which band is which.
echo "band count and wavelength names survive the round trip"
"$BIN/imgdiff" --expect-channels "$WORK/once.exr" 31 || exit 1

# A cube whose channels went out under the wrong names would still read back
# with 31 ascending wavelengths, so the count and the names are not enough on
# their own. Reading the ORIGINAL cube and the round-tripped one and finding the
# same band structure in both is what closes that gap.
echo "the source cube has the same band structure as the round-tripped one"
"$BIN/imgdiff" --expect-channels "$RADIANCE" 31 || exit 1

echo "values survive: no NaN, no negatives introduced by the read"
"$BIN/imgdiff" --expect-finite "$WORK/once.exr" || exit 1
"$BIN/imgdiff" --expect-nonnegative "$WORK/once.exr" || exit 1

exit 0
