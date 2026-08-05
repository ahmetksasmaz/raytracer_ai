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
RAYTRACER="$ROOT/raytracer"
IMGDIFF="$ROOT/imgdiff"
SCENES="$ROOT/tests/scenes"
OUT="$ROOT/tests/out"
FILTER="${1:-}"

PASS=0
FAIL=0
SKIP=0
FAILED_NAMES=()

for binary in "$RAYTRACER" "$IMGDIFF"; do
  if [ ! -x "$binary" ]; then
    echo "error: $binary not built. Run: make release imgdiff"
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
