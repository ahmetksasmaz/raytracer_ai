#!/usr/bin/env python3
"""Generates the demo's scenes and sensor configs.

Five ColorChecker charts -- the same 24 measured reflectances under five
spectrally different lights -- and one sensor config per measured camera in the
spectral library. `run_demo.sh` then renders the five and develops each through
all thirty.

The chart geometry is reused from scenes/spectral_demo/generate.py rather than
rewritten; only the illuminant differs between the five scenes, which is what
makes them comparable.

Run this after `fetch_spectra.py` adds or removes a camera; the sensor list is
read from spectra/ rather than hard-coded.
"""

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "scenes", "spectral_demo"))

import generate as chart  # noqa: E402  the existing chart builder

RESOLUTION = "400 400"
SAMPLES = "96"

# --- The sensor -------------------------------------------------------------
#
# Longer exposure and lower ISO than the stock demo, without inventing a
# photosite that could not exist.
#
# The full well is the binding constraint. A 3.45 um pixel holding 60,000 e- is
# already at the generous end of what is physical, so it is NOT raised -- and
# the chart's white patch already nearly fills it. That caps how much more light
# can be collected: only the headroom between the old ADC ceiling and the well.
#
#   old: Gain 12 e-/DN at 12 bits -> ADC tops out at 4095 x 12 = 49,140 e-,
#        so 18% of the well was unreachable and exposure could not use it.
#   new: 14 bits (what every modern DSLR does) with Gain set so the ADC range
#        lands exactly on the well: 16383 x 3.662 = 60,000 e-.
#
# That buys two things. Exposure can rise 22% before the white patch clips, for
# +10% shot SNR. And the quantisation step falls from 12 e- to 3.66 e- -- RMS
# 1.06 e-, now below the 2.0 e- read noise, where a well-designed camera puts
# it. Together the read+quantisation floor drops from 4.00 e- to 2.26 e-, a 43%
# reduction.
#
# Note gain here is ELECTRONS PER DN, not ISO amplification, and it is applied
# after every noise source -- so it cannot change SNR by itself. Raising the
# number is the low-ISO direction: it takes more light to saturate.
FULL_WELL = 60000.0
BIT_DEPTH = 14
MAX_DN = float((1 << BIT_DEPTH) - 1)
GAIN = FULL_WELL / MAX_DN              # ADC range mapped exactly onto the well
BASE_EXPOSURE = 8e-5
EXPOSURE = BASE_EXPOSURE * (FULL_WELL / (4095.0 * 12.0))   # the 22% headroom

SENSOR_BASE = {
    "_pattern": "RGGB",
    "ExposureTime": "%.4g" % EXPOSURE,
    "PixelPitch": "3.45e-6",
    "FNumber": "2.8",
    "FullWell": "%.0f" % FULL_WELL,
    "Gain": "%.4f" % GAIN,
    "BitDepth": str(BIT_DEPTH),
    "DynamicRange": "12",
    "ReadNoise": "2.0",
    "DarkCurrent": "5.0",
    "NoiseSources": "Shot Read Dark",
}


def sensor_names():
    """Every multichannel record under spectra/sensors, read from the files."""
    names = []
    directory = os.path.join(ROOT, "spectra", "sensors")
    for filename in sorted(os.listdir(directory)):
        if not filename.endswith(".spd"):
            continue
        with open(os.path.join(directory, filename)) as handle:
            for line in handle:
                if line.startswith("name:"):
                    names.append(line.split(":", 1)[1].strip())
    return sorted(names)


def write_scene(tag, light_ref, scale, description):
    """One chart scene, with a RELATIVE ImageName.

    The stock generator bakes an absolute path into ImageName, which sends the
    render into outputs/ and leaves the cube nowhere near the scene. Here the
    render must land in demo/rendered_<tag>/ so run_demo.sh can find it.
    """
    scene = chart.chart_scene(tag, light_ref, scale, description)
    camera = scene["Cameras"]["Camera"][0]
    camera["ImageName"] = os.path.join("rendered_%s" % tag, "chart.exr")
    camera["ImageResolution"] = RESOLUTION
    camera["NumSamples"] = SAMPLES
    scene["_comment"] = (
        "ColorChecker under %s. The 24 patches carry measured reflectance "
        "spectra; only the illuminant differs between the five demo scenes, "
        "which is what makes them comparable." % description)

    path = os.path.join(HERE, "scenes", "chart_%s.json" % tag)
    with open(path, "w") as handle:
        json.dump({"Scene": scene}, handle, indent=2)
        handle.write("\n")
    return path


def write_sensor(name):
    config = dict(SENSOR_BASE)
    config["_comment"] = (
        "Measured spectral sensitivity of %s. The curve is the whole optical "
        "response, so it fills the three CFA curves and quantum efficiency is "
        "forced to 1. Exposure and gain are set so the ADC range lands exactly "
        "on the full well; see demo/generate_demo.py." % name)
    config["_ref"] = "sensor:%s" % name
    path = os.path.join(HERE, "sensors", "%s.json" % name)
    with open(path, "w") as handle:
        json.dump(config, handle, indent=2)
        handle.write("\n")
    return path


def main():
    os.makedirs(os.path.join(HERE, "scenes"), exist_ok=True)
    os.makedirs(os.path.join(HERE, "sensors"), exist_ok=True)

    print("sensor: %d bit, gain %.4f e-/DN, exposure %.3g s" %
          (BIT_DEPTH, GAIN, EXPOSURE))
    print("        ADC ceiling %.0f e- vs full well %.0f e-" %
          (MAX_DN * GAIN, FULL_WELL))
    print("        quantisation RMS %.2f e- vs read noise 2.00 e-\n" %
          (GAIN / 12 ** 0.5))

    print("scenes:")
    for tag, ref, scale, description in chart.ILLUMINANTS:
        print("  %s" % os.path.relpath(write_scene(tag, ref, scale, description), ROOT))

    names = sensor_names()
    for name in names:
        write_sensor(name)
    print("\nsensors: %d configs in %s" %
          (len(names), os.path.relpath(os.path.join(HERE, "sensors"), ROOT)))
    print("\nrender with:  ./demo/run_demo.sh")


if __name__ == "__main__":
    main()
