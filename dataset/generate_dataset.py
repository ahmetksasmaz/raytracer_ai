#!/usr/bin/env python3
"""Generates a dataset of procedural Cornell-box scenes.

    90 scenes   N spheres [0,20], M cubes [0,20], K illuminants [1,6]
    10 scenes   N spheres [0,10], M cubes [1,20], K illuminants [1,6],
                plus several glass and mirror objects

Every scene ships with its full spectral decomposition -- radiance, illumination
and reflectance -- which is the reason to build a dataset with this renderer
rather than another. The per-pixel illumination map is ground truth for the
illuminant at every pixel, which is exactly what a colour-constancy method needs
and normally cannot get.

    python3 dataset/generate_dataset.py                      the real thing
    python3 dataset/generate_dataset.py --resolution "128 128" --samples 64 \
            --out /tmp/dstest                                a fast check

--seed makes the whole set reproducible; --samples and --resolution mean
re-generating at a different budget costs a second, so 512 spp is a default
rather than a commitment.
"""

import argparse
import json
import math
import os
import random

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# --- The room ---------------------------------------------------------------
# Same box and camera as tests/scenes/hyperspectral_box.json: open front, camera
# outside looking in, so nothing sits between the lens and the scene.
BOX = 3.0
CAM_POS = (0.0, 0.2, 10.5)
CAM_GAZE = (0.0, -0.9, 0.0)
CAM_UP = (0.0, 1.0, 0.0)
FOV_Y = 40.0

# --- Palettes ---------------------------------------------------------------
# Diffuse albedos for the objects. Kept under ~0.8: a closed box full of
# near-white surfaces converges very slowly, which is why the stock Cornell
# scenes use 0.6.
OBJECT_ALBEDOS = [
    ("bone",        (0.72, 0.70, 0.66)),
    ("slate",       (0.28, 0.31, 0.35)),
    ("brick",       (0.55, 0.18, 0.14)),
    ("moss",        (0.20, 0.42, 0.19)),
    ("ochre",       (0.66, 0.47, 0.13)),
    ("teal",        (0.13, 0.45, 0.46)),
    ("plum",        (0.36, 0.16, 0.40)),
    ("sand",        (0.68, 0.60, 0.42)),
    ("rose",        (0.70, 0.42, 0.45)),
    ("olive",       (0.42, 0.44, 0.18)),
    ("cobalt",      (0.15, 0.24, 0.58)),
    ("charcoal",    (0.13, 0.13, 0.14)),
]

WALL_WHITE = (0.73, 0.73, 0.73)
WALL_RED = (0.63, 0.06, 0.05)
WALL_GREEN = (0.14, 0.45, 0.09)

# Illuminants, by library reference. Spectrally varied on purpose -- a dataset
# whose lights are all daylight teaches nothing about illuminant estimation.
ILLUMINANTS = [
    "light:d65", "light:a", "light:incandescent", "light:fl11",
    "light:led_b3", "light:hps", "light:d50", "light:cool_white_fl",
    "light:metal_halide", "light:mercury",
]


# --- Vector helpers ---------------------------------------------------------
def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1],
            a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def normalize(a):
    n = math.sqrt(dot(a, a))
    return (a[0] / n, a[1] / n, a[2] / n)


class Camera:
    """Just enough of the camera to answer 'would this be in frame?'.

    Mirrors BaseCamera's lookAt construction. Used for rejection sampling: an
    object the camera cannot see is not worth rendering, and the brief asks for
    all of them to be visible.
    """

    def __init__(self, aspect):
        self.pos = CAM_POS
        self.forward = normalize(sub(CAM_GAZE, CAM_POS))
        self.right = normalize(cross(self.forward, CAM_UP))
        self.up = cross(self.right, self.forward)
        self.tan_y = math.tan(math.radians(FOV_Y) / 2.0)
        self.tan_x = self.tan_y * aspect

    def sees(self, centre, radius, margin=0.04):
        """True when a sphere of this radius projects wholly inside the frame."""
        v = sub(centre, self.pos)
        depth = dot(v, self.forward)
        if depth <= radius + 0.1:
            return False
        # Half-extent of the frame at this depth, shrunk by the object's own
        # angular radius so it is contained rather than merely centred.
        half_x = depth * self.tan_x * (1.0 - margin) - radius
        half_y = depth * self.tan_y * (1.0 - margin) - radius
        if half_x <= 0 or half_y <= 0:
            return False
        return abs(dot(v, self.right)) <= half_x and abs(dot(v, self.up)) <= half_y


# --- Scene pieces -----------------------------------------------------------
def diffuse_material(index, albedo, brdf="1"):
    return {
        "_id": str(index), "_BRDF": brdf,
        "AmbientReflectance": "0 0 0",
        "DiffuseReflectance": "%.4f %.4f %.4f" % albedo,
        "SpecularReflectance": "0 0 0",
    }


def mirror_material(index, tint):
    # MirrorReflectance is the only field the mirror branch reads, and it has no
    # default -- RawMaterial::mirror is uninitialised, so omitting it yields
    # indeterminate values rather than an error.
    return {
        "_id": str(index), "_type": "mirror",
        "AmbientReflectance": "0 0 0",
        "DiffuseReflectance": "0 0 0",
        "SpecularReflectance": "0 0 0",
        "MirrorReflectance": "%.4f %.4f %.4f" % tint,
    }


def glass_material(index, absorption, ior):
    # AbsorptionCoefficient must be written even when zero: absent, the parser
    # stores -1 -1 -1 and Beer-Lambert is silently skipped entirely.
    return {
        "_id": str(index), "_type": "dielectric",
        "AmbientReflectance": "0 0 0",
        "DiffuseReflectance": "0 0 0",
        "SpecularReflectance": "0 0 0",
        "AbsorptionCoefficient": "%.4f %.4f %.4f" % absorption,
        "RefractionIndex": "%.3f" % ior,
    }


def emitter_material(index):
    return {
        "_id": str(index),
        "AmbientReflectance": "0 0 0",
        "DiffuseReflectance": "0 0 0",
        "SpecularReflectance": "0 0 0",
    }


def add_vertex(vertices, point):
    """Appends a vertex and returns its 1-BASED index, which is what the parser wants."""
    vertices.append("%.5f %.5f %.5f" % point)
    return len(vertices)


def add_cube(vertices, centre, half):
    """Appends the 8 corners of an axis-aligned cube; returns its face string.

    12 triangles -- there is no box primitive. Winding is outward (copied from
    the beer_absorb slab), which matters only if the cube is ever an emitter,
    since the tracer flips normals toward the ray for everything else.
    """
    cx, cy, cz = centre
    hx, hy, hz = half
    base = len(vertices)
    for point in ((cx - hx, cy - hy, cz - hz), (cx + hx, cy - hy, cz - hz),
                  (cx + hx, cy + hy, cz - hz), (cx - hx, cy + hy, cz - hz),
                  (cx - hx, cy - hy, cz + hz), (cx + hx, cy - hy, cz + hz),
                  (cx + hx, cy + hy, cz + hz), (cx - hx, cy + hy, cz + hz)):
        add_vertex(vertices, point)
    faces = [5, 6, 7, 5, 7, 8,  1, 4, 3, 1, 3, 2,  2, 3, 7, 2, 7, 6,
             1, 5, 8, 1, 8, 4,  1, 2, 6, 1, 6, 5,  4, 8, 7, 4, 7, 3]
    return " ".join(str(base + i) for i in faces)


def add_ceiling_panel(vertices, cx, cz, half_x, half_z, height):
    """A downward-facing emitter panel.

    Corner order is load-bearing: emitters are ONE-SIDED, and a panel wound the
    other way contributes nothing at all -- not even through next-event
    estimation, whose pdf also returns zero for the back face. It does not
    render as an error, it renders as an unlit room.
    """
    base = len(vertices)
    for point in ((cx - half_x, height, cz - half_z),
                  (cx + half_x, height, cz - half_z),
                  (cx + half_x, height, cz + half_z),
                  (cx - half_x, height, cz + half_z)):
        add_vertex(vertices, point)
    return " ".join(str(base + i) for i in (1, 2, 3, 1, 3, 4))


# --- One scene --------------------------------------------------------------
def build_scene(index, kind, rng, resolution, samples, out_dir):
    """Returns (scene dict, metadata dict) for one Cornell box."""
    aspect = 1.0
    width, height = (int(v) for v in resolution.split())
    aspect = width / float(height)
    camera = Camera(aspect)

    specular = kind == "specular"
    if specular:
        n_spheres = rng.randint(0, 10)
        n_cubes = rng.randint(1, 20)
    else:
        n_spheres = rng.randint(0, 20)
        n_cubes = rng.randint(0, 20)
    n_lights = rng.randint(1, 6)

    vertices = []
    materials = []
    meta = {"index": index, "kind": kind, "seed": rng.getstate()[1][0],
            "objects": [], "lights_detail": [], "walls": {}}

    # Box corners occupy vertex slots 1..8, in the order the wall face table
    # below assumes.
    for point in ((-BOX, -BOX, -BOX), (BOX, -BOX, -BOX), (BOX, BOX, -BOX),
                  (-BOX, BOX, -BOX), (-BOX, -BOX, BOX), (BOX, -BOX, BOX),
                  (BOX, BOX, BOX), (-BOX, BOX, BOX)):
        add_vertex(vertices, point)

    # Material 1 is the emitter material by convention, as in every Cornell
    # scene in the repo.
    materials.append(emitter_material(1))

    # Walls: white is preferred, so coloured side walls are the minority.
    coloured_walls = rng.random() < 0.25
    materials.append(diffuse_material(2, WALL_WHITE))
    if coloured_walls:
        materials.append(diffuse_material(3, WALL_RED))
        materials.append(diffuse_material(4, WALL_GREEN))
        left_material, right_material = "3", "4"
        meta["walls"] = {"left": "red", "right": "green", "other": "white"}
    else:
        left_material = right_material = "2"
        meta["walls"] = {"left": "white", "right": "white", "other": "white"}

    meshes = [
        {"_id": "1", "Material": "2", "Faces": {"_data": "1 2 3 1 3 4"}},        # back
        {"_id": "2", "Material": left_material, "Faces": {"_data": "1 4 8 1 8 5"}},
        {"_id": "3", "Material": right_material, "Faces": {"_data": "2 6 7 2 7 3"}},
        {"_id": "4", "Material": "2", "Faces": {"_data": "1 5 6 1 6 2"}},        # floor
        {"_id": "5", "Material": "2", "Faces": {"_data": "4 3 7 4 7 8"}},        # ceiling
    ]

    # --- Objects ------------------------------------------------------------
    # Size shrinks as the population grows, so 40 objects still read as
    # separate things rather than one mass. Overlap is allowed, so this is not
    # a packing problem -- only containment and visibility are enforced.
    total = max(1, n_spheres + n_cubes)
    scale = min(1.0, math.sqrt(9.0 / total))
    radius_range = (0.22 * scale + 0.10, 0.55 * scale + 0.16)

    spheres, cubes = [], []
    next_material = len(materials) + 1
    next_object = 6

    def place(radius, on_floor=False):
        """A centre inside the box and inside the frame, or None.

        Containment uses the object's own radius as the margin so nothing pokes
        through a wall, and visibility is the camera's own projection so nothing
        is clipped at the frame edge. Overlap is deliberately NOT rejected --
        the brief allows it, and rejecting it would make dense scenes
        unsatisfiable.
        """
        for _ in range(400):
            y = (-BOX + radius) if on_floor else \
                rng.uniform(-BOX + radius, BOX * 0.55 - radius)
            centre = (rng.uniform(-BOX + radius, BOX - radius), y,
                      rng.uniform(-BOX + radius, BOX - radius))
            if camera.sees(centre, radius):
                return centre
        return None

    def specular_material(role):
        nonlocal next_material
        if role == "glass":
            tint = rng.choice([(0.0, 0.0, 0.0), (0.0, 0.0, 0.0),
                               (0.35, 0.08, 0.08), (0.06, 0.30, 0.22)])
            materials.append(glass_material(next_material, tint,
                                            rng.uniform(1.45, 1.62)))
        else:
            base = rng.uniform(0.88, 0.96)
            tint = rng.choice([(base, base, base), (base, base, base),
                               (base, base * 0.80, base * 0.48)])
            materials.append(mirror_material(next_material, tint))
        next_material += 1
        return str(next_material - 1)

    # For the specular scenes, decide up front which objects are glass/mirror so
    # they are spread through the population rather than clumped at the end.
    glass_wanted = rng.randint(2, 5) if specular else 0
    mirror_wanted = rng.randint(2, 5) if specular else 0
    roles = ["glass"] * glass_wanted + ["mirror"] * mirror_wanted
    roles += ["diffuse"] * max(0, n_spheres + n_cubes - len(roles))
    rng.shuffle(roles)
    roles = roles[:n_spheres + n_cubes]

    n_glass = n_mirror = 0
    for slot in range(n_spheres + n_cubes):
        is_sphere = slot < n_spheres
        radius = rng.uniform(*radius_range)
        centre = place(radius, on_floor=rng.random() < 0.45)
        if centre is None:
            continue  # no visible spot found; the metadata records what exists

        role = roles[slot] if slot < len(roles) else "diffuse"
        if role == "diffuse":
            name, albedo = rng.choice(OBJECT_ALBEDOS)
            materials.append(diffuse_material(next_material, albedo))
            material_id = str(next_material)
            next_material += 1
        else:
            name = role
            material_id = specular_material(role)
            if role == "glass":
                n_glass += 1
            else:
                n_mirror += 1

        if is_sphere:
            slot_id = add_vertex(vertices, centre)
            spheres.append({"_id": str(next_object), "Material": material_id,
                            "Center": str(slot_id), "Radius": "%.4f" % radius})
            shape = "sphere"
        else:
            half = (radius * rng.uniform(0.7, 1.0),
                    radius * rng.uniform(0.7, 1.0),
                    radius * rng.uniform(0.7, 1.0))
            faces = add_cube(vertices, centre, half)
            meshes.append({"_id": str(next_object), "Material": material_id,
                           "Faces": {"_data": faces}})
            shape = "cube"
        next_object += 1

        meta["objects"].append({
            "shape": shape, "material": name, "role": role,
            "centre": centre, "radius": radius,
        })

    # --- Illuminants --------------------------------------------------------
    # A mix of broad ceiling panels (global) and small emissive spheres placed
    # in the room (local), so the dataset contains both the case a global white
    # balance handles and the case it cannot.
    light_meshes, light_spheres = [], []
    for k in range(n_lights):
        ref = rng.choice(ILLUMINANTS)
        # Divide the budget across the lights so a 6-light scene is not six
        # times brighter than a 1-light one.
        budget = 1.0 / math.sqrt(n_lights)
        if rng.random() < 0.55:
            half_x = rng.uniform(0.5, 1.5)
            half_z = rng.uniform(0.5, 1.5)
            cx = rng.uniform(-BOX + half_x + 0.3, BOX - half_x - 0.3)
            cz = rng.uniform(-BOX + half_z + 0.3, BOX - half_z - 0.3)
            faces = add_ceiling_panel(vertices, cx, cz, half_x, half_z, BOX - 0.06)
            scale_value = rng.uniform(7.0, 13.0) * budget / (half_x * half_z)
            light_meshes.append({
                "_id": str(next_object), "Material": "1",
                "Radiance": "6 6 6",
                "RadianceSpectrum": {"_ref": ref, "_scale": "%.4f" % scale_value},
                "Faces": {"_data": faces},
            })
            meta["lights_detail"].append({"type": "global panel", "ref": ref,
                                   "scale": scale_value,
                                   "centre": (cx, BOX - 0.06, cz),
                                   "size": (2 * half_x, 2 * half_z)})
        else:
            radius = rng.uniform(0.16, 0.34)
            centre = place(radius) or (0.0, 1.0, 0.0)
            slot_id = add_vertex(vertices, centre)
            scale_value = rng.uniform(9.0, 20.0) * budget
            light_spheres.append({
                "_id": str(next_object), "Material": "1",
                "Center": str(slot_id), "Radius": "%.4f" % radius,
                "Radiance": "20 20 20",
                "RadianceSpectrum": {"_ref": ref, "_scale": "%.4f" % scale_value},
            })
            meta["lights_detail"].append({"type": "local sphere", "ref": ref,
                                   "scale": scale_value, "centre": centre,
                                   "radius": radius})
        next_object += 1

    # --- Assemble -----------------------------------------------------------
    # Glass needs the deeper recursion: each dielectric hit spawns two rays, and
    # at the parser's default of 1 every glass and mirror surface renders pure
    # black while the rest of the image looks entirely normal.
    max_depth = "12" if specular else "8"

    camera_block = {
        "_id": "1", "_type": "lookAt",
        "Position": "%g %g %g" % CAM_POS,
        "GazePoint": "%g %g %g" % CAM_GAZE,
        "Up": "0 1 0", "FovY": "%g" % FOV_Y, "NearDistance": "1",
        "ImageResolution": resolution, "NumSamples": str(samples),
        "MaxRecursionDepth": max_depth, "MinRecursionDepth": "3",
        "Renderer": "PathTracing",
        "RendererParams":
            "ImportanceSampling NextEventEstimation MIS_BALANCE RussianRoulette",
        "ImageName": os.path.join(out_dir, "scene_%03d.exr" % index),
        "Tonemap": {"TMO": "Photographic", "TMOOptions": "0.18 1.0",
                    "Saturation": "1.0", "Gamma": "2.2", "Extension": ".png"},
    }

    objects = {"Mesh": meshes}
    if spheres:
        objects["Sphere"] = spheres
    if light_meshes:
        objects["LightMesh"] = light_meshes
    if light_spheres:
        objects["LightSphere"] = light_spheres

    scene = {
        "_comment": "Procedural Cornell box %03d (%s): %d spheres, %d cubes, "
                    "%d illuminants. Generated by dataset/generate_dataset.py."
                    % (index, kind, len(spheres), len(meshes) - 5, n_lights),
        "BackgroundColor": "0 0 0",
        "ShadowRayEpsilon": "0.001",
        "Cameras": {"Camera": [camera_block]},
        "Lights": {"AmbientLight": "0 0 0"},
        "BRDFs": {"ModifiedBlinnPhong": {"_id": "1", "_normalized": "true",
                                         "Exponent": "1"}},
        "Materials": {"Material": materials},
        "VertexData": {"_data": "\n".join(vertices)},
        "Objects": objects,
    }

    meta.update({
        "spheres": len(spheres), "cubes": len(meshes) - 5,
        "lights": n_lights, "materials": len(materials),
        "glass": n_glass, "mirror": n_mirror,
        "samples": samples, "resolution": resolution,
        "requested_spheres": n_spheres, "requested_cubes": n_cubes,
        "max_depth": max_depth,
    })
    return scene, meta


# --- Metadata ---------------------------------------------------------------
def write_scene_metadata(path, meta):
    """The full record for one scene: what to read when a render looks wrong."""
    with open(path, "w") as f:
        f.write("scene_%03d\n" % meta["index"])
        f.write("=" * 60 + "\n\n")
        f.write("kind             %s\n" % meta["kind"])
        f.write("resolution       %s\n" % meta["resolution"])
        f.write("samples          %d\n" % meta["samples"])
        f.write("max recursion    %s\n" % meta["max_depth"])
        f.write("walls            left %s, right %s, other %s\n"
                % (meta["walls"]["left"], meta["walls"]["right"],
                   meta["walls"]["other"]))
        f.write("\ncounts\n")
        f.write("  spheres        %d\n" % meta["spheres"])
        f.write("  cubes          %d\n" % meta["cubes"])
        f.write("  illuminants    %d\n" % meta["lights"])
        f.write("  materials      %d\n" % meta["materials"])
        f.write("  glass objects  %d\n" % meta["glass"])
        f.write("  mirror objects %d\n" % meta["mirror"])
        if meta["requested_spheres"] != meta["spheres"] or \
           meta["requested_cubes"] != meta["cubes"]:
            # Honest about the gap: an object with no visible spot is dropped
            # rather than placed somewhere the camera cannot see it.
            f.write("\n  note: %d spheres and %d cubes were drawn; the counts\n"
                    "  above are what fitted in frame.\n"
                    % (meta["requested_spheres"], meta["requested_cubes"]))

        f.write("\nilluminants\n")
        for i, light in enumerate(meta["lights_detail"], 1):
            f.write("  %2d  %-14s %-22s scale %7.3f  at (%6.2f %6.2f %6.2f)"
                    % (i, light["type"], light["ref"], light["scale"],
                       light["centre"][0], light["centre"][1], light["centre"][2]))
            if "size" in light:
                f.write("  size %.2f x %.2f" % light["size"])
            else:
                f.write("  radius %.2f" % light["radius"])
            f.write("\n")

        f.write("\nobjects\n")
        for i, obj in enumerate(meta["objects"], 1):
            f.write("  %3d  %-7s %-9s %-10s at (%6.2f %6.2f %6.2f)  r %.3f\n"
                    % (i, obj["shape"], obj["role"], obj["material"],
                       obj["centre"][0], obj["centre"][1], obj["centre"][2],
                       obj["radius"]))

        f.write("\nAll objects above are inside the box and project wholly\n"
                "inside the frame. Overlap is permitted, so one may still be\n"
                "hidden behind another -- 'visible' here means in frame, not\n"
                "unoccluded.\n")


def write_table(path, metas, seed):
    with open(path, "w") as f:
        f.write("Procedural Cornell-box dataset -- %d scenes\n" % len(metas))
        f.write("=" * 78 + "\n\n")
        f.write("Generated by dataset/generate_dataset.py --seed %d\n" % seed)
        f.write("Per-scene detail is in dataset/metadata/scene_NNN.txt;\n")
        f.write("the scene files themselves are in dataset/scenes/.\n\n")
        f.write("Each render produces, in dataset/renders/:\n")
        f.write("  scene_NNN_radiance.exr      31-band spectral radiance\n")
        f.write("  scene_NNN_illumination.exr  31-band, the light alone\n")
        f.write("  scene_NNN_reflectance.exr   31-band, the surface alone\n")
        f.write("  scene_NNN.exr / .png        linear RGB and a tonemapped preview\n\n")
        f.write("radiance = reflectance * illumination, so the illumination cube\n")
        f.write("is per-pixel ground truth for the illuminant.\n\n")
        f.write("%-6s %-9s %8s %6s %7s %10s %6s %7s %6s %s\n"
                % ("scene", "kind", "spheres", "cubes", "lights", "materials",
                   "glass", "mirror", "spp", "walls"))
        f.write("-" * 78 + "\n")
        for m in metas:
            f.write("%-6s %-9s %8d %6d %7d %10d %6d %7d %6d %s\n"
                    % ("%03d" % m["index"], m["kind"], m["spheres"], m["cubes"],
                       m["lights"], m["materials"], m["glass"], m["mirror"],
                       m["samples"],
                       "red/green" if m["walls"]["left"] == "red" else "white"))

        f.write("\ntotals: %d spheres, %d cubes, %d illuminants, "
                "%d glass, %d mirror\n"
                % (sum(m["spheres"] for m in metas),
                   sum(m["cubes"] for m in metas),
                   sum(m["lights"] for m in metas),
                   sum(m["glass"] for m in metas),
                   sum(m["mirror"] for m in metas)))


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--seed", type=int, default=20260826)
    parser.add_argument("--resolution", default="512 512")
    parser.add_argument("--samples", type=int, default=512)
    parser.add_argument("--diffuse", type=int, default=90)
    parser.add_argument("--specular", type=int, default=10)
    parser.add_argument("--out", default=HERE,
                        help="where to write scenes/ and metadata/")
    args = parser.parse_args()

    out = os.path.abspath(args.out)
    scenes_dir = os.path.join(out, "scenes")
    meta_dir = os.path.join(out, "metadata")
    renders_dir = os.path.join(out, "renders")
    for d in (scenes_dir, meta_dir, renders_dir):
        os.makedirs(d, exist_ok=True)

    kinds = ["diffuse"] * args.diffuse + ["specular"] * args.specular
    metas = []
    for index, kind in enumerate(kinds):
        # One stream per scene, so regenerating scene 42 alone gives the same
        # scene 42 whatever else changed.
        rng = random.Random(args.seed + index * 7919)
        scene, meta = build_scene(index, kind, rng, args.resolution,
                                  args.samples, renders_dir)

        with open(os.path.join(scenes_dir, "scene_%03d.json" % index), "w") as f:
            json.dump({"Scene": scene}, f, indent=1)
            f.write("\n")
        write_scene_metadata(os.path.join(meta_dir, "scene_%03d.txt" % index), meta)
        metas.append(meta)

    write_table(os.path.join(out, "metadata.txt"), metas, args.seed)

    print("%d scenes -> %s" % (len(metas), os.path.relpath(scenes_dir, ROOT)))
    print("  %s spheres, %s cubes, %s illuminants across the set"
          % (sum(m["spheres"] for m in metas), sum(m["cubes"] for m in metas),
             sum(m["lights"] for m in metas)))
    print("  %d glass and %d mirror objects in the %d specular scenes"
          % (sum(m["glass"] for m in metas), sum(m["mirror"] for m in metas),
             args.specular))
    print("  %s at %s spp -> metadata.txt + metadata/scene_NNN.txt"
          % (args.resolution, args.samples))
    print("\nrender with:  ./dataset/render_all.sh")


if __name__ == "__main__":
    main()
