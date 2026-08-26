#!/usr/bin/env python3
"""Generates the two-illuminant demo: scenes and sensor configs.

The single-illuminant demo lights the whole chart with one source, which is the
case a global white balance handles perfectly. This one puts TWO spectrally
different lights on the same chart, one on each side, so the illuminant varies
across the frame.

That is the case a single gain triple cannot serve. Balance for the left light
and the right half is wrong; balance for the right and the left half is wrong;
balance for the average and both halves are wrong. It is precisely the situation
the renderer's per-pixel illumination map exists to solve, and the demo produces
both corrections side by side so the difference is visible rather than asserted.

Three core illuminants -- daylight, tungsten, fluorescent -- in all six ORDERED
pairs. Ordered, not combinations: (a, b) puts `a` on the left and `b` on the
right, and (b, a) is its mirror. Rendering both is a free correctness check,
since the two should come out as mirror images of each other.
"""

import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(ROOT, "scenes", "spectral_demo"))
sys.path.insert(0, os.path.join(ROOT, "demo", "single_illuminant"))

import generate as chart          # noqa: E402  the chart geometry
import generate_demo as single    # noqa: E402  the sensor settings

# The three classics. Daylight, a blackbody-ish tungsten lamp and a narrowband
# triphosphor fluorescent -- far enough apart spectrally that mixing any two
# gives a genuinely bi-chromatic scene rather than two shades of the same white.
CORE = [
    ("d65", "light:d65", "CIE D65 daylight"),
    ("incandescent", "light:incandescent", "measured tungsten lamp"),
    ("fl11", "light:fl11", "CIE FL11 narrowband fluorescent"),
]

SCALE = 42.0
RESOLUTION = "400 400"
SAMPLES = "96"


def pairs():
    """All ordered pairs of distinct core illuminants: 3 x 2 = 6."""
    return [(a, b) for a in CORE for b in CORE if a[0] != b[0]]


def dual_scene(left, right):
    """The chart with two emitter panels, one per side.

    Built on chart_scene so the geometry, the 24 measured reflectances and the
    backdrop stay identical to the single-illuminant demo -- only the lighting
    differs, which is what keeps the two demos comparable.
    """
    left_tag, left_ref, left_desc = left
    right_tag, right_ref, right_desc = right

    scene = chart.chart_scene(left_tag, left_ref, SCALE, left_desc)

    # chart_scene appended one emitter material and one LightMesh. Replace both
    # with a pair: the existing material becomes the left light, and a second is
    # added for the right.
    materials = scene["Materials"]["Material"]
    left_material = int(materials[-1]["_id"])
    right_material = left_material + 1
    materials.append(chart.emitter(right_material,
                                   right_ref.split(":", 1)[1], SCALE))

    # Two panels above the chart, each covering one half in x. The single-light
    # panel spanned x = -8..8; splitting it at x = 0 keeps the total emitting
    # area and the geometry identical, so the two demos stay comparable.
    #
    # The panels overlap slightly at the seam so neither half is unlit at the
    # centre line. That overlap is the interesting part: it is a smooth mix of
    # two illuminants, where no single chromaticity is correct.
    vertices = scene["VertexData"]["_data"].split("\n")
    left_faces = chart.quad("-8 8 -1", "0.5 8 -1", "0.5 8 6", "-8 8 6", vertices)
    right_faces = chart.quad("-0.5 8 -1", "8 8 -1", "8 8 6", "-0.5 8 6", vertices)
    scene["VertexData"]["_data"] = "\n".join(vertices)

    scene["Objects"]["LightMesh"] = [
        {
            "_id": "1",
            "Material": str(left_material),
            "RadianceSpectrum": {"_ref": left_ref, "_scale": str(SCALE)},
            "Faces": {"_data": "\n".join(left_faces)},
        },
        {
            "_id": "2",
            "Material": str(right_material),
            "RadianceSpectrum": {"_ref": right_ref, "_scale": str(SCALE)},
            "Faces": {"_data": "\n".join(right_faces)},
        },
    ]

    tag = "%s_%s" % (left_tag, right_tag)
    camera = scene["Cameras"]["Camera"][0]
    camera["ImageName"] = os.path.join("rendered_%s" % tag, "chart.exr")
    camera["ImageResolution"] = RESOLUTION
    camera["NumSamples"] = SAMPLES
    scene["_comment"] = (
        "ColorChecker under TWO illuminants: %s on the left, %s on the right. "
        "The illuminant varies across the frame, so no single white balance "
        "gain triple is correct everywhere -- which is what the renderer's "
        "per-pixel illumination map is for." % (left_desc, right_desc))

    path = os.path.join(HERE, "scenes", "chart_%s.json" % tag)
    with open(path, "w") as handle:
        json.dump({"Scene": scene}, handle, indent=2)
        handle.write("\n")
    return tag, path


def main():
    os.makedirs(os.path.join(HERE, "scenes"), exist_ok=True)
    os.makedirs(os.path.join(HERE, "sensors"), exist_ok=True)

    print("sensor: identical to the single-illuminant demo (imported), so the")
    print("        two sets of results are directly comparable\n")

    print("scenes: %d ordered pairs from %d core illuminants" %
          (len(pairs()), len(CORE)))
    for left, right in pairs():
        tag, path = dual_scene(left, right)
        print("  %-28s %s left, %s right" % (tag, left[0], right[0]))

    # Same 30 cameras, same settings. Written here rather than shared so each
    # demo directory stands on its own, but generated from the same source so
    # the sensor cannot silently drift between the two.
    names = single.sensor_names()
    for name in names:
        config = dict(single.SENSOR_BASE)
        config["_comment"] = (
            "Measured spectral sensitivity of %s. Identical to the "
            "single-illuminant demo's config so the two are comparable." % name)
        config["_ref"] = "sensor:%s" % name
        with open(os.path.join(HERE, "sensors", "%s.json" % name), "w") as h:
            json.dump(config, h, indent=2)
            h.write("\n")
    print("\nsensors: %d configs" % len(names))
    print("\nrender with:  ./demo/dual_illuminant/run_demo.sh")


if __name__ == "__main__":
    main()
