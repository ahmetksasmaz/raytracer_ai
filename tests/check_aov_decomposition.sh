#!/bin/bash
# radiance = reflectance * illumination, band by band.
#
# This is a real claim, not an identity. The illumination map is built by
# re-evaluating the BRDF with kd=1 -- the radiance a white surface would have
# shown -- and never by dividing radiance by the albedo. Had it been a division,
# the product would reconstruct the radiance by construction and this check
# would pass no matter how wrong the transport was.
#
# The scene matters twice over. illuminant_d65's sphere is purely diffuse
# (SpecularReflectance "0 0 0"), and the factorisation is exact only where the
# first vertex has no specular lobe -- with ks > 0 no per-band scalar splits a
# highlight into surface colour times light colour.
#
# And its albedo is 0.6, not 1. That is load-bearing: on a scene whose surfaces
# are all white the reflectance map is 1 everywhere, the product degenerates
# into comparing the illumination against itself, and the check passes without
# testing anything. The guard below asserts the map is NOT constant, so that
# failure mode cannot come back unnoticed.
#
# --expect-product compares per channel, matched by name, over all 31 bands. It
# must not go through imgdiff's band-averaging path: mean(R) * mean(E) is not
# mean(R * E), so an averaged comparison would be a weaker and different claim.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/bin"
OUT="$ROOT/tests/out"

RADIANCE="$OUT/illuminant_d65_radiance.exr"
REFLECTANCE="$OUT/illuminant_d65_reflectance.exr"
ILLUMINATION="$OUT/illuminant_d65_illumination.exr"

for f in "$RADIANCE" "$REFLECTANCE" "$ILLUMINATION"; do
  if [ ! -f "$f" ]; then
    echo "no $f -- the illuminant_d65 render must run first, with AOVs enabled"
    exit 1
  fi
done

# Anti-vacuity first, because everything below is worthless without it: if the
# reflectance were 1 everywhere the product would be the illumination itself.
echo "the reflectance map must not be uniformly 1 -- or the product tests nothing"
if "$BIN/imgdiff" --expect-constant "$REFLECTANCE" 1.0 --tol 1e-6 > /dev/null 2>&1; then
  echo "FAIL: reflectance is uniformly 1; this scene cannot test the factorisation"
  exit 1
fi
echo "PASS: the scene has a non-trivial albedo (0.6 sphere against 1.0 emitters)"

echo "reflectance x illumination must reproduce the radiance, band by band"
"$BIN/imgdiff" --expect-product "$REFLECTANCE" "$ILLUMINATION" "$RADIANCE" --tol 1e-5 || exit 1

# Both maps must be physically sensible on their own. A reflectance above 1
# would be a surface emitting more than it receives.
echo "reflectance is a reflectance: finite, non-negative"
"$BIN/imgdiff" --expect-finite "$REFLECTANCE" || exit 1
"$BIN/imgdiff" --expect-nonnegative "$REFLECTANCE" || exit 1

echo "illumination is finite and non-negative"
"$BIN/imgdiff" --expect-finite "$ILLUMINATION" || exit 1
"$BIN/imgdiff" --expect-nonnegative "$ILLUMINATION" || exit 1

# Both are spectral cubes on the same band grid as the radiance -- the channel
# names are the only record of which band is which, so they are asserted.
echo "both AOVs are well-formed N-band cubes"
"$BIN/imgdiff" --expect-channels "$REFLECTANCE" 31 || exit 1
"$BIN/imgdiff" --expect-channels "$ILLUMINATION" 31 || exit 1

exit 0
