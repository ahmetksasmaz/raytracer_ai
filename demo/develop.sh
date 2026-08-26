# Shared by both demos: everything between a rendered spectral cube and the
# nine viewable images.
#
# Deliberately one file rather than a copy in each demo directory. Two subtle
# things live in here -- the per-pair exposure metering and the split between
# the scene-fitted gains and the reference-fitted colour matrix -- and both are
# the kind of mistake that produces a plausible-looking wrong image. Keeping one
# copy is what stops the two demos drifting apart on either.
#
# Expects BIN, DEMO and KEEP_EXR to be set by the caller.
#
#   develop <sensor> <scene-tag> <white-balance-illuminant>

develop() {
  local sensor="$1" illum="$2" wb_illum="$3"
  local base="$DEMO/sensors/$sensor.json"
  local src="$DEMO/rendered_$illum"
  local out="$DEMO/${sensor}_${illum}"
  mkdir -p "$out"

  local log="$out/develop.log"
  : > "$log"
  run() { "$@" >> "$log" 2>&1 || return 1; }

  # --- Meter -----------------------------------------------------------------
  # Set the exposure from the scene, the way a photographer does, instead of
  # using one shutter speed for every light.
  #
  # It has to be per (sensor, illuminant). Library lights are normalised to 1.0
  # at 560 nm, which fixes their spectral SHAPE but not their total power: a
  # narrowband source whose peaks tower over 560 nm -- sodium, triphosphor
  # fluorescent -- delivers several times the energy of daylight at the same
  # nominal scale. Metered against one fixed exposure, FL11 came out 45%
  # clipped and HPS 18%, against 1.5% for D65. And the three channels differ
  # per camera, so the right shutter speed differs per camera too.
  #
  # This is NOT the auto-exposure the pipeline refuses to do. That one is a
  # scene-adaptive DISPLAY mapping, which hides a badly exposed sensor. This
  # chooses ExposureTime before the exposure happens and then leaves the fixed
  # display window alone -- exactly what "set the exposure until the histogram
  # fits" means.
  local cfg="$out/sensor.json"
  run "$BIN/sensor_irradiance" --in "$src/chart_radiance.exr" --out "$out/probe_photons.exr"   --config "$base" &&
  run "$BIN/sensor_cfa"        --in "$out/probe_photons.exr"  --out "$out/probe_electrons.exr" --config "$base" || return 1

  local peak
  peak=$("$BIN/imgdiff" --stats "$out/probe_electrons.exr" 2>/dev/null |
         grep -o 'max=[0-9.]*' | cut -d= -f2)
  rm -f "$out/probe_photons.exr" "$out/probe_electrons.exr"
  [ -n "$peak" ] || return 1

  # Electrons are exactly linear in exposure time, so one probe pass is enough
  # to solve for the shutter speed that puts the brightest patch at 92% of the
  # well -- short of saturation, with a little room for shot noise to fluctuate
  # into. The config that produced each result is written beside it.
  python3 - "$base" "$cfg" "$peak" <<'METER'
import json, sys
base, out, peak = sys.argv[1], sys.argv[2], float(sys.argv[3])
config = json.load(open(base))
full_well = float(config["FullWell"])
exposure = float(config["ExposureTime"])
if peak > 0:
    exposure *= 0.92 * full_well / peak
config["ExposureTime"] = "%.6g" % exposure
config["_metered"] = ("ExposureTime chosen so the brightest patch reaches 92%% "
                      "of the full well; probe peaked at %.0f e- at the base "
                      "exposure." % peak)
json.dump(config, open(out, "w"), indent=2)
METER
  [ -f "$cfg" ] || return 1

  # Sensor: spectral radiance -> RAW Bayer mosaic.
  run "$BIN/sensor_irradiance" --in "$src/chart_radiance.exr" --out "$out/photons.exr"     --config "$cfg" &&
  run "$BIN/sensor_cfa"        --in "$out/photons.exr"        --out "$out/electrons.exr"   --config "$cfg" &&
  run "$BIN/sensor_mosaic"     --in "$out/electrons.exr"      --out "$out/mosaic.exr"      --config "$cfg" &&
  run "$BIN/sensor_noise"      --in "$out/mosaic.exr"         --out "$out/noisy.exr"       --config "$cfg" --seed 1 &&
  run "$BIN/sensor_saturate"   --in "$out/noisy.exr"          --out "$out/wellclamped.exr" --config "$cfg" &&
  run "$BIN/sensor_adc"        --in "$out/wellclamped.exr"    --out "$out/raw.pgm"         --config "$cfg" || return 1

  # --- Calibrate -------------------------------------------------------------
  # TWO fits, and they are not interchangeable.
  #
  # The white balance GAINS must come from the scene's own illuminant: they are
  # the per-scene adaptation, and that is what makes a grey card read grey.
  #
  # The colour MATRIX must come from a fixed reference illuminant. A real
  # camera's CCM is a property of the camera, measured once; it is not refitted
  # per scene. Refitting it here is actively wrong, because the fit targets the
  # absolute XYZ of each patch UNDER THAT LIGHT -- so it maps a white-balanced
  # neutral back onto the illuminant's own chromaticity and cancels the white
  # balance it was just given. Measured on this chart under HPS:
  #
  #     matrix fitted under hps : neutral -> x=0.506 y=0.429   (orange)
  #     matrix fitted under D65 : neutral -> x=0.313 y=0.330   (D65 white)
  #
  # Both produce a perfectly plausible image, which is what makes it worth
  # spelling out rather than leaving to whoever reads the output.
  run "$BIN/sensor_ccm" --config "$cfg" --illuminant "light:$wb_illum" --out "$out/ccm_scene.json" &&
  run "$BIN/sensor_ccm" --config "$cfg" --illuminant D65            --out "$out/ccm_reference.json" || return 1

  # The renderer's ground-truth illuminant map, through this sensor's filters.
  run "$BIN/sensor_illum_chroma" --in "$src/chart_illumination.exr" \
      --out "$out/illumchroma.exr" --config "$cfg" || return 1

  # ISP up to the demosaic, shared by both white-balance routes.
  run "$BIN/isp_blacklevel" --in "$out/raw.pgm"          --out "$out/linearized.exr" --config "$cfg" &&
  run "$BIN/isp_demosaic"   --in "$out/linearized.exr"   --out "$out/demosaiced.exr" --config "$cfg" || return 1

  # Route A: the calibrated global gains, what a camera does with a known light.
  run "$BIN/isp_whitebalance" --in "$out/demosaiced.exr" --out "$out/wb.exr"  --calibration "$out/ccm_scene.json" &&
  run "$BIN/isp_colormatrix"  --in "$out/wb.exr"         --out "$out/xyz.exr" --calibration "$out/ccm_reference.json" --matrix after_wb &&
  run "$BIN/isp_srgb"         --in "$out/xyz.exr"        --out "$out/6_srgb.png" || return 1

  # Route B: the renderer's per-pixel ground truth.
  run "$BIN/isp_whitebalance" --in "$out/demosaiced.exr" --out "$out/wb_gt.exr"  --chroma "$out/illumchroma.exr" &&
  run "$BIN/isp_colormatrix"  --in "$out/wb_gt.exr"      --out "$out/xyz_gt.exr" --calibration "$out/ccm_reference.json" --matrix after_wb &&
  run "$BIN/isp_srgb"         --in "$out/xyz_gt.exr"     --out "$out/9_srgb_ground_truth.png" || return 1

  # The viewable set. Numbered so they sort in pipeline order in a file browser.
  # The ground-truth illuminant map, made viewable, in SENSOR SPACE. That is
  # the view that matters, because the triple it draws is literally the divisor:
  #
  #     demosaiced / (r/g, 1, b/g)  =  wb
  #
  # It comes out green-cyan, the way published illuminant maps do -- green is
  # the reference channel and a CFA's green response dominates, so both ratios
  # sit below 1. Passing --calibration would convert it to what the light looks
  # like to the eye, which is a different and no longer divisible quantity.
  run "$BIN/chroma_preview" --in "$out/illumchroma.exr" --out "$out/0_illuminant_map.png" &&
  run "$BIN/raw_preview"  --in "$out/raw.pgm"        --out "$out/1_bayer_raw.png" --config "$cfg" --mosaic &&
  run "$BIN/isp_preview"  --in "$out/demosaiced.exr" --out "$out/2_debayered_linear.png" --linear &&
  run "$BIN/isp_preview"  --in "$out/demosaiced.exr" --out "$out/3_debayered_gamma.png" &&
  run "$BIN/isp_preview"  --in "$out/wb.exr"         --out "$out/4_wb_debayered_linear.png" --linear &&
  run "$BIN/isp_preview"  --in "$out/wb.exr"         --out "$out/5_wb_debayered_gamma.png" &&
  run "$BIN/isp_preview"  --in "$out/wb_gt.exr"      --out "$out/7_wb_gt_debayered_linear.png" --linear &&
  run "$BIN/isp_preview"  --in "$out/wb_gt.exr"      --out "$out/8_wb_gt_debayered_gamma.png" || return 1

  if [ "$KEEP_EXR" -eq 0 ]; then
    rm -f "$out"/{photons,electrons,mosaic,noisy,wellclamped,linearized}.exr
    rm -f "$out"/{demosaiced,wb,xyz,wb_gt,xyz_gt,illumchroma}.exr "$out/raw.exr"
  fi
  return 0
}
