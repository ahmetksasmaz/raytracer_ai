#!/bin/bash
# Correctness suite for the ray/path tracer.
#
# Every check here is self-validating: it asserts against analytic ground truth
# (a white furnace must read exactly the emitted radiance) or against an
# invariance (rotating a sphere about its own centre must not change the image).
# Nothing is compared against a blessed reference image, because the renderer's
# current output is known-wrong and there is nothing trustworthy to bless.
#
#   ./tests/run_tests.sh            run everything
#   ./tests/run_tests.sh furnace    run only checks whose name matches "furnace"
#
# Renders land in tests/out/ and are left in place for inspection.

set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/build/bin"
RAYTRACER="$BIN/raytracer"
IMGDIFF="$BIN/imgdiff"
SCENES="$ROOT/tests/scenes"
CONFIGS="$ROOT/tests/configs"
OUT="$ROOT/tests/out"
FILTER="${1:-}"

PASS=0
FAIL=0
SKIP=0
FAILED_NAMES=()

for binary in "$RAYTRACER" "$IMGDIFF"; do
  if [ ! -x "$binary" ]; then
    echo "error: $binary not built. Run:"
    echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"
    exit 2
  fi
done

mkdir -p "$OUT"

# render <scene-basename>  — renders into $OUT so relative ImageName paths land there
render() {
  ( cd "$OUT" && "$RAYTRACER" "$SCENES/$1.json" ) > "$OUT/$1.log" 2>&1
  if [ $? -ne 0 ]; then
    echo "    render failed, see $OUT/$1.log"
    tail -3 "$OUT/$1.log" | sed 's/^/    /'
    return 1
  fi
  return 0
}

# check <name> <description> -- <imgdiff args...>
check() {
  local name="$1"; shift
  local desc="$1"; shift
  shift # the literal --

  if [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]]; then
    SKIP=$((SKIP + 1))
    return
  fi

  printf "%-22s %s\n" "$name" "$desc"
  local output
  output="$("$IMGDIFF" "$@" 2>&1)"
  local status=$?
  echo "$output" | sed "s|$OUT/||g" | sed 's/^/    /'
  if [ $status -eq 0 ]; then
    echo "    -> PASS"
    PASS=$((PASS + 1))
  else
    echo "    -> FAIL"
    FAIL=$((FAIL + 1))
    FAILED_NAMES+=("$name")
  fi
  echo
}

# expect_render_failure <name> <scene> <description> <expected-message-substring>
#
# Asserts that a scene is REJECTED. Passing means a non-zero exit whose message
# contains the expected text -- a crash for some unrelated reason would satisfy
# the exit code alone, so the message is part of the assertion.
expect_render_failure() {
  local name="$1" scene="$2" desc="$3" expected="$4"

  if [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]]; then
    SKIP=$((SKIP + 1))
    return
  fi

  printf "%-22s %s\n" "$name" "$desc"
  ( cd "$OUT" && "$RAYTRACER" "$SCENES/$scene.json" ) > "$OUT/$scene.log" 2>&1
  local status=$?

  if [ $status -eq 0 ]; then
    echo "    render SUCCEEDED but should have been rejected"
    echo "    -> FAIL"
    FAIL=$((FAIL + 1))
    FAILED_NAMES+=("$name")
  elif grep -q "$expected" "$OUT/$scene.log"; then
    echo "    rejected (exit $status): $(grep -o "$expected.*" "$OUT/$scene.log" | head -1 | cut -c1-96)"
    echo "    -> PASS"
    PASS=$((PASS + 1))
  else
    echo "    failed (exit $status) but without the expected message '$expected'"
    tail -3 "$OUT/$scene.log" | sed 's/^/    /'
    echo "    -> FAIL"
    FAIL=$((FAIL + 1))
    FAILED_NAMES+=("$name")
  fi
  echo
}

# needs <name> <scene>...  — render the scenes a check depends on, honouring the filter
needs() {
  local name="$1"; shift
  if [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]]; then
    return
  fi
  for scene in "$@"; do
    render "$scene" || true
  done
}

# sensor_chain <prefix> <sensor-config> [seed]
#
# Runs the sensor stages over <prefix>_radiance.exr, leaving every intermediate
# in $OUT so a failing check can be traced to the stage that broke it. Named
# <prefix>_<stage>.<ext> throughout, which is the pipeline's naming convention.
sensor_chain() {
  local p="$1" cfg="$2" seed="${3:-1}"
  ( cd "$OUT" &&
    "$BIN/sensor_irradiance" --in "${p}_radiance.exr"    --out "${p}_photons.exr"     --config "$cfg" &&
    "$BIN/sensor_cfa"        --in "${p}_photons.exr"     --out "${p}_electrons.exr"   --config "$cfg" &&
    "$BIN/sensor_mosaic"     --in "${p}_electrons.exr"   --out "${p}_mosaic.exr"      --config "$cfg" &&
    "$BIN/sensor_noise"      --in "${p}_mosaic.exr"      --out "${p}_noisy.exr"       --config "$cfg" --seed "$seed" &&
    "$BIN/sensor_saturate"   --in "${p}_noisy.exr"       --out "${p}_wellclamped.exr" --config "$cfg" &&
    "$BIN/sensor_adc"        --in "${p}_wellclamped.exr" --out "${p}_raw.pgm"         --config "$cfg"
  ) >> "$OUT/${p}.log" 2>&1
}

# isp_chain <prefix> <sensor-config>
#
# The ISP stages, from the RAW to the display image. Calibration comes from the
# sensor's own curves, so the white balance gains and the colour matrix are
# guaranteed to be the matching pair.
isp_chain() {
  local p="$1" cfg="$2"
  ( cd "$OUT" &&
    "$BIN/sensor_ccm"       --config "$cfg"              --out "${p}_ccm.json" &&
    "$BIN/isp_blacklevel"   --in "${p}_raw.pgm"          --out "${p}_linearized.exr" --config "$cfg" &&
    "$BIN/isp_demosaic"     --in "${p}_linearized.exr"   --out "${p}_demosaiced.exr" --config "$cfg" &&
    "$BIN/isp_whitebalance" --in "${p}_demosaiced.exr"   --out "${p}_wb.exr"         --calibration "${p}_ccm.json" &&
    "$BIN/isp_colormatrix"  --in "${p}_wb.exr"           --out "${p}_xyz.exr"        --calibration "${p}_ccm.json" --matrix after_wb &&
    "$BIN/isp_srgb"         --in "${p}_xyz.exr"          --out "${p}_srgb.png"
  ) >> "$OUT/${p}.log" 2>&1
}

# pipeline <name> <scene> <sensor-config>  — render, then the whole chain
pipeline() {
  local name="$1" scene="$2" cfg="$3"
  if [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]]; then
    return
  fi
  render "$scene" || return 1
  sensor_chain "$scene" "$CONFIGS/$cfg.json" || {
    echo "    sensor chain failed, see $OUT/${scene}.log"; return 1; }
  isp_chain "$scene" "$CONFIGS/$cfg.json" || {
    echo "    isp chain failed, see $OUT/${scene}.log"; return 1; }
  return 0
}

# run_check <name> <description> <command...>  — a check whose assertion is a
# command's exit status rather than an imgdiff invocation.
run_check() {
  local name="$1"; shift
  local desc="$1"; shift

  if [ -n "$FILTER" ] && [[ "$name" != *"$FILTER"* ]]; then
    SKIP=$((SKIP + 1))
    return
  fi

  printf "%-22s %s\n" "$name" "$desc"
  local output
  output="$("$@" 2>&1)"
  local status=$?
  echo "$output" | sed "s|$OUT/||g" | sed 's/^/    /'
  if [ $status -eq 0 ]; then
    echo "    -> PASS"; PASS=$((PASS + 1))
  else
    echo "    -> FAIL"; FAIL=$((FAIL + 1)); FAILED_NAMES+=("$name")
  fi
  echo
}

echo "=============================================="
echo " raytracer correctness suite"
echo "=============================================="
echo

# --- Energy conservation -----------------------------------------------------
# A closed emissive cavity with an albedo-1 sphere inside. With cosine sampling
# and a normalized BRDF each sample evaluates to exactly rho*L, so a correct
# renderer produces a completely flat image with zero variance.

needs furnace-plain furnace
# Cosine sampling in a uniform field evaluates to exactly rho*L per sample, so
# this configuration is analytically noise-free and every pixel must be 1.0 --
# hence the very tight --max-dev rather than just a mean check.
check furnace-plain "white furnace, brute force -- every pixel must equal 1.0" -- \
  --expect-constant "$OUT/furnace.exr" 1.0 --tol 0.005 --max-dev 0.002

needs furnace-nee furnace_nee
check furnace-nee "white furnace + NEE -- double-counted emission reads high" -- \
  --expect-constant "$OUT/furnace_nee.exr" 1.0 --tol 0.005

needs furnace-mis furnace_nee_mis
check furnace-mis "white furnace + NEE + MIS -- weights must sum to one" -- \
  --expect-constant "$OUT/furnace_nee_mis.exr" 1.0 --tol 0.005

needs furnace-rr furnace_rr
check furnace-rr "white furnace + Russian roulette -- uncompensated RR reads low" -- \
  --expect-constant "$OUT/furnace_rr.exr" 1.0 --tol 0.01

needs furnace-brdf furnace_default_brdf
check furnace-brdf "white furnace, default BRDF -- non-normalized reads pi times high" -- \
  --expect-constant "$OUT/furnace_default_brdf.exr" 1.0 --tol 0.01

needs furnace-split furnace_split4
check furnace-split "white furnace + SplittingFactor 4 -- emitter-only control" -- \
  --expect-constant "$OUT/furnace_split4.exr" 1.0 --tol 0.01

# --- Multi-bounce transport --------------------------------------------------
# A furnace cannot detect MIS weighting errors: both strategies share one
# expectation and the weights sum to one, so bias cancels however wrong the
# weights are. These need real multi-bounce indirect light to be meaningful.

needs nee-equivalence cornell_brute cornell_nee
check nee-equivalence "Cornell box: NEE must match brute force" -- \
  --compare "$OUT/cornell_nee.exr" "$OUT/cornell_brute.exr" --tol 0.03

needs mis-equivalence cornell_brute cornell_mis
check mis-equivalence "Cornell box: NEE+MIS must match brute force" -- \
  --compare "$OUT/cornell_mis.exr" "$OUT/cornell_brute.exr" --tol 0.03

# --- Estimator invariances ---------------------------------------------------

needs split-invariance split_analytic_1 split_analytic_4
check split-invariance "SplittingFactor 1 vs 4 with an ANALYTIC light -- must match" -- \
  --compare "$OUT/split_analytic_4.exr" "$OUT/split_analytic_1.exr" --tol 0.02

# --- Volume absorption -------------------------------------------------------
# 0.2-thick slab, sigma_a = 2, so transmission must be exp(-0.4) = 0.670320.

needs beer-lambert beer_zero beer_absorb
check beer-lambert "Beer-Lambert through a 0.2 slab -- ratio must be exp(-0.4)=0.6703" -- \
  --expect-ratio "$OUT/beer_absorb.exr" "$OUT/beer_zero.exr" 0.670320 --tol 0.02

# --- Geometry ----------------------------------------------------------------

needs sphere-rotation sphere_norot sphere_rot
check sphere-rotation "sphere rotated about its own centre -- must be identical" -- \
  --compare "$OUT/sphere_rot.exr" "$OUT/sphere_norot.exr" --tol 0.001

needs plane-occlusion plane_shadow quad_shadow
check plane-occlusion "infinite Plane vs equivalent quad -- both must cast shadow" -- \
  --compare "$OUT/plane_shadow.exr" "$OUT/quad_shadow.exr" --tol 0.02

needs area-light-back area_light_back
check area-light-back "surface behind an area light -- no negative radiance" -- \
  --expect-nonnegative "$OUT/area_light_back.exr"

# --- Tone mapping ------------------------------------------------------------
# An all-black scene must tone map to all black in every operator.

needs tonemap tonemap_black
check tonemap-photographic "all-black scene, Photographic -- control, has the guard" -- \
  --expect-below "$OUT/tonemap_black_photographic.png" 4
check tonemap-filmic "all-black scene, Filmic -- NaN clamps to white without a guard" -- \
  --expect-below "$OUT/tonemap_black_filmic.png" 4
check tonemap-aces "all-black scene, ACES -- NaN clamps to white without a guard" -- \
  --expect-below "$OUT/tonemap_black_aces.png" 4

# A flat radiance of 1.0 through the Photographic operator lands on an exactly
# predictable byte: 0.18/(1+0.18) = 0.152542, gamma 2.2, times 255 = 108.
needs tonemap-ldr furnace_ldr
check tonemap-ldr "LDR output honours Tonemap -- flat 1.0 must map to 108" -- \
  --expect-constant "$OUT/furnace_ldr.png" 108 --tol 0.01 --max-dev 1

# --- Spectral core -----------------------------------------------------------
# Reference-data and colour-maths checks that no rendered image can catch: a
# mistyped digit in the CIE tables or an illuminant produces a perfectly
# plausible picture and a quietly biased white-balance result.

if [ -z "$FILTER" ] || [[ "spectral-selfcheck" == *"$FILTER"* ]]; then
  printf "%-22s %s\n" "spectral-selfcheck" "CIE tables, illuminant chromaticities, uplift identities"
  if [ -x "$BIN/spectraltest" ]; then
    if "$BIN/spectraltest" | sed 's/^/    /'; then
      echo "    -> PASS"; PASS=$((PASS + 1))
    else
      echo "    -> FAIL"; FAIL=$((FAIL + 1)); FAILED_NAMES+=("spectral-selfcheck")
    fi
  else
    echo "    $BIN/spectraltest not built; run:"
    echo "      cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"
    echo "    -> FAIL"; FAIL=$((FAIL + 1)); FAILED_NAMES+=("spectral-selfcheck")
  fi
  echo
else
  SKIP=$((SKIP + 1))
fi

# Sensor physics: noise moments, radiometric linearity, Bayer layout. Noise is
# checked statistically -- a wrong noise model still looks grainy, so only the
# moments distinguish a correct one.
if [ -z "$FILTER" ] || [[ "sensor-selfcheck" == *"$FILTER"* ]]; then
  printf "%-22s %s\n" "sensor-selfcheck" "Poisson/Gaussian moments, radiometry, Bayer, quantisation"
  if [ -x "$BIN/sensortest" ]; then
    if "$BIN/sensortest" | sed 's/^/    /'; then
      echo "    -> PASS"; PASS=$((PASS + 1))
    else
      echo "    -> FAIL"; FAIL=$((FAIL + 1)); FAILED_NAMES+=("sensor-selfcheck")
    fi
  else
    echo "    $BIN/sensortest not built; run:"
    echo "      cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j"
    echo "    -> FAIL"; FAIL=$((FAIL + 1)); FAILED_NAMES+=("sensor-selfcheck")
  fi
  echo
else
  SKIP=$((SKIP + 1))
fi

# Same geometry and reflectance, different illuminant SPD. These MUST differ --
# if they match, the illuminant data is not reaching the render.
needs illuminant-discrimination illuminant_d65 illuminant_a
check illuminant-discrimination "D65 vs illuminant A -- renders must differ" -- \
  --expect-differ "$OUT/illuminant_a.exr" "$OUT/illuminant_d65.exr" --tol 0.02

# A reflectance with no RGB equivalent: narrow band around 550 nm under D65.
needs spectral-reflectance spectral_narrowband
check spectral-reflectance "narrow-band 550nm reflectance must render green" -- \
  --expect-channel-max "$OUT/spectral_narrowband.exr" g

# --- Measured spectral library -----------------------------------------------
# The library (spectra/*.spd) is a second route to the same physics as the
# compiled-in tables, so it can be checked against them rather than against a
# blessed image: D65 read from file must reproduce D65 read from SpectralData.hpp.
# That one comparison exercises the file parser, the resample onto the band grid
# and the 560nm normalisation in a single assertion.

needs library-equivalence illuminant_d65 library_ref_d65
check library-equivalence "library D65 must match the compiled-in D65" -- \
  --compare "$OUT/illuminant_d65.exr" "$OUT/library_ref_d65.exr" --tol 0.002

# A reference that does not resolve must stop the render. Silently substituting
# a default would let a colour study complete under the wrong spectrum and
# report a plausible, wrong answer -- the failure mode no image inspection
# catches. Same reasoning as the unknown-illuminant error these mirror.
expect_render_failure library-unknown-ref library_bad_ref \
  "unknown _ref must be fatal, not silently defaulted" \
  "Unknown spectrum reference"

expect_render_failure library-wrong-kind library_wrong_kind \
  "a 3-channel sensor curve used as a single spectrum must be fatal" \
  "cannot be used as a single spectrum"

# --- Sensor pipeline ---------------------------------------------------------
# End-to-end through the split pipeline: render -> six sensor stages -> five ISP
# stages, each its own program passing a file to the next.
#
# Noise is checked WITHIN a Bayer parity class: whole-image variance on a mosaic
# just measures the mosaic pattern, so a perfectly noise-free render would still
# look "noisy" by that measure.

pipeline sensor sensor_clean clean
pipeline sensor sensor_noisy noisy

check sensor-radiance-cube "radiance cube is a well-formed N-band EXR" -- \
  --expect-channels "$OUT/sensor_clean_radiance.exr" 31

check sensor-bayer "Bayer mosaic: green sites must agree, R/B must not" -- \
  --expect-bayer "$OUT/sensor_clean_raw.exr" RGGB

check sensor-noiseless "NoiseSources None -- variance must be exactly zero" -- \
  --variance "$OUT/sensor_clean_raw.exr" --expect-noiseless 1e-9

# Poisson electrons through gain g give DN with variance/mean = 1/g. Here g=16,
# so each parity should land near 0.0625; the printed ratios are the real check.
check sensor-noise "shot/read/dark noise present, var/mean ~ 1/gain" -- \
  --variance "$OUT/sensor_noisy_raw.exr" --expect-noisy 20

# --- The split itself --------------------------------------------------------
# Splitting one fused loop into six programs is exactly the kind of change that
# produces a plausible image with a units error buried in it. These checks are
# about the seams rather than the physics.

# The stages must compose back into the fused SensorModel::ElectronsAt that
# sensortest already validates. A factor dropped in the handover between two
# programs -- the band width, the geometry factor, the photon energy -- would
# show up here and nowhere else.
run_check stage-fusion "split stages must equal the fused electron count" \
  "$ROOT/tests/check_stage_fusion.sh"

# Noise is its own program now, so it needs its own generator. Same seed twice
# must be identical, or a frame cannot be reproduced once you have looked at it;
# different seeds must differ, or the seed is being ignored.
run_check noise-determinism "same seed reproduces, different seed does not" \
  "$ROOT/tests/check_noise_seed.sh"

# The band grid lives in the EXR channel names, not in the channel count. A cube
# must survive a write/read round trip exactly, including when its grid is not
# the one this binary was compiled for.
run_check spectral-roundtrip "spectral cube survives write/read exactly" \
  "$ROOT/tests/check_spectral_roundtrip.sh"

# --- White balance and the colour matrix -------------------------------------
# The two corrections overlap, so they are fitted together and must be used as a
# pair. Applying the gains and then the un-balanced matrix double-corrects;
# skipping the gains and using the balanced matrix under-corrects. Both produce
# a plausible image, which is what makes this worth asserting.
run_check wb-ccm-pairing "wb+after_wb and no_wb alone must agree" \
  "$ROOT/tests/check_wb_ccm.sh"

# --- Fixed sensor response ---------------------------------------------------
# The sensor display path is deliberately NOT scene-adaptive: digital numbers
# map to code values through the ADC full scale and DynamicRange, both constants
# of the sensor. These two pairs pin that down from opposite sides.

pipeline sensor-fixed-exposure sensor_exposure_1x exposure_1x
pipeline sensor-fixed-exposure sensor_exposure_2x exposure_2x
# An auto-exposure would renormalise both frames to the same brightness and make
# them agree, so "must differ" is the assertion that catches its return.
check sensor-fixed-exposure "doubling exposure must brighten the image, not be normalised away" -- \
  --expect-differ "$OUT/sensor_exposure_1x_srgb.png" "$OUT/sensor_exposure_2x_srgb.png" --tol 0.02

pipeline sensor-range sensor_range_wide range_wide
pipeline sensor-range sensor_range_narrow range_narrow
# DynamicRange is a property of how the measurement is shown, not of the
# measurement. It is read by isp_blacklevel and by nothing upstream of it, which
# this pair asserts from both sides. Noise is off in both so the comparison is
# exact.
check sensor-range-raw "DynamicRange must not alter the RAW measurement" -- \
  --compare "$OUT/sensor_range_wide_raw.exr" "$OUT/sensor_range_narrow_raw.exr" --tol 1e-9

check sensor-range-display "DynamicRange must change the rendered image" -- \
  --expect-differ "$OUT/sensor_range_wide_srgb.png" "$OUT/sensor_range_narrow_srgb.png" --tol 0.02

# --- Summary -----------------------------------------------------------------

echo "=============================================="
printf " passed %d   failed %d" "$PASS" "$FAIL"
[ "$SKIP" -gt 0 ] && printf "   skipped %d" "$SKIP"
echo
if [ "$FAIL" -gt 0 ]; then
  echo " failing: ${FAILED_NAMES[*]}"
fi
echo "=============================================="

[ "$FAIL" -eq 0 ]
