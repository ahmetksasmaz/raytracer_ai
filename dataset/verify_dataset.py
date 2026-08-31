#!/usr/bin/env python3
"""Checks the generated dataset against what the metadata claims.

Structural checks run without rendering anything; the render checks are skipped
for scenes not yet rendered, so this is useful both before and after the long
run.

    python3 dataset/verify_dataset.py
    python3 dataset/verify_dataset.py --dir /tmp/dstest

The failure this is really guarding against: a scene whose lights are wound the
wrong way renders a plausible dark image rather than an error, and a metadata
row written before an object was rejected lies quietly. Neither is visible by
looking at a thumbnail.
"""

import argparse
import json
import math
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
IMGDIFF = os.path.join(ROOT, "build", "bin", "imgdiff")

sys.path.insert(0, HERE)
from generate_dataset import Camera, BOX  # noqa: E402  the same projection


def fail(problems, scene, message):
    problems.append("scene_%s: %s" % (scene, message))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dir", default=HERE)
    args = parser.parse_args()

    root = os.path.abspath(args.dir)
    scenes_dir = os.path.join(root, "scenes")
    meta_dir = os.path.join(root, "metadata")
    renders = os.path.join(root, "renders")

    names = sorted(f[:-5] for f in os.listdir(scenes_dir) if f.endswith(".json"))
    table = open(os.path.join(root, "metadata.txt")).read().split("\n")
    rows = {}
    for line in table:
        m = re.match(r"^(\d{3})\s+(\w+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)", line)
        if m:
            rows[m.group(1)] = {
                "kind": m.group(2), "spheres": int(m.group(3)),
                "cubes": int(m.group(4)), "lights": int(m.group(5)),
                "materials": int(m.group(6)), "glass": int(m.group(7)),
                "mirror": int(m.group(8)),
            }

    problems = []
    rendered = 0
    if len(rows) != len(names):
        problems.append("metadata.txt has %d rows for %d scenes"
                        % (len(rows), len(names)))

    for name in names:
        index = name.split("_")[1]
        scene = json.load(open(os.path.join(scenes_dir, name + ".json")))["Scene"]
        objects = scene["Objects"]
        camera = scene["Cameras"]["Camera"][0]
        row = rows.get(index)
        if row is None:
            fail(problems, index, "missing from metadata.txt")
            continue

        # --- counts in the table must match the scene actually written -------
        spheres = len(objects.get("Sphere", []))
        cubes = len(objects.get("Mesh", [])) - 5          # 5 walls
        lights = len(objects.get("LightMesh", [])) + len(objects.get("LightSphere", []))
        materials = len(scene["Materials"]["Material"])
        if spheres != row["spheres"]:
            fail(problems, index, "%d spheres in JSON, %d in metadata"
                 % (spheres, row["spheres"]))
        if cubes != row["cubes"]:
            fail(problems, index, "%d cubes in JSON, %d in metadata"
                 % (cubes, row["cubes"]))
        if lights != row["lights"]:
            fail(problems, index, "%d lights in JSON, %d in metadata"
                 % (lights, row["lights"]))
        if materials != row["materials"]:
            fail(problems, index, "%d materials in JSON, %d in metadata"
                 % (materials, row["materials"]))
        if lights < 1:
            fail(problems, index, "no illuminant at all")

        # --- specular scenes must really be specular -------------------------
        text = json.dumps(scene)
        glass = text.count('"dielectric"')
        mirror = text.count('"mirror"')
        if glass != row["glass"] or mirror != row["mirror"]:
            fail(problems, index, "glass/mirror %d/%d in JSON, %d/%d in metadata"
                 % (glass, mirror, row["glass"], row["mirror"]))
        if row["kind"] == "specular":
            if glass < 1 or mirror < 1:
                fail(problems, index, "specular scene with %d glass, %d mirror"
                     % (glass, mirror))
            # At the parser's default of 1 these render pure black while the
            # rest of the image looks entirely normal.
            if int(camera["MaxRecursionDepth"]) < 12:
                fail(problems, index, "specular scene at MaxRecursionDepth %s"
                     % camera["MaxRecursionDepth"])

        # --- every object inside the box and inside the frame ----------------
        width, height = (int(v) for v in camera["ImageResolution"].split())
        view = Camera(width / float(height))
        detail = open(os.path.join(meta_dir, name + ".txt")).read()
        for line in detail.split("\n"):
            m = re.match(r"\s+\d+\s+(sphere|cube)\s+\S+\s+\S+\s+at \(\s*(-?[\d.]+)"
                         r"\s+(-?[\d.]+)\s+(-?[\d.]+)\)\s+r\s+([\d.]+)", line)
            if not m:
                continue
            centre = tuple(float(m.group(i)) for i in (2, 3, 4))
            radius = float(m.group(5))
            if max(abs(c) for c in centre) > BOX:
                fail(problems, index, "object at %s is outside the box" % (centre,))
            if not view.sees(centre, radius):
                fail(problems, index, "object at %s is not in frame" % (centre,))

        # --- rendered output, when it exists ---------------------------------
        radiance = os.path.join(renders, name + "_radiance.exr")
        if not os.path.exists(radiance):
            continue
        rendered += 1
        out = subprocess.run([IMGDIFF, "--stats", radiance],
                             capture_output=True, text=True).stdout
        mean = float(re.search(r"mean=([\d.]+)", out).group(1))
        if "NON-FINITE" in out:
            fail(problems, index, "radiance contains non-finite samples")
        if "NEGATIVE" in out:
            fail(problems, index, "radiance contains negative samples")
        # A scene whose lights are wound backwards renders dark, not broken.
        if mean < 1e-3:
            fail(problems, index, "radiance mean %.2e -- effectively black, "
                                  "check the emitter winding" % mean)

    print("scenes:   %d" % len(names))
    print("rendered: %d" % rendered)
    print("checked:  metadata counts, object containment and framing,")
    print("          specular content and recursion depth, render sanity")
    if problems:
        print("\n%d PROBLEM(S):" % len(problems))
        for p in problems[:40]:
            print("  " + p)
        if len(problems) > 40:
            print("  ... and %d more" % (len(problems) - 40))
        return 1
    print("\nall checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
