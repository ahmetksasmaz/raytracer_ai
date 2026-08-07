#!/usr/bin/env python3
"""Generate the spectral-library demo scenes.

    python3 scenes/spectral_demo/generate.py

Everything here draws its light, its reflectances and its camera response from
spectra/ rather than from RGB triples, so these scenes are also the worked
examples for the `_ref` syntax. They are generated rather than hand-written
because a 24-patch chart is 96 vertices and 24 materials of near-identical JSON.

Kept small on purpose (see the RESOLUTION/SAMPLES knobs) -- the point is to show
the spectral difference, not to produce a final frame.
"""

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
OUT = os.path.join(ROOT, "outputs", "spectral_demo")

RESOLUTION = "400 400"
SAMPLES = "96"

# ---------------------------------------------------------------------------
# Shared pieces
# ---------------------------------------------------------------------------

CHART = [
    "dark_skin", "light_skin", "blue_sky", "foliage", "blue_flower",
    "bluish_green", "orange", "purplish_blue", "moderate_red", "purple",
    "yellow_green", "orange_yellow", "blue", "green", "red", "yellow",
    "magenta", "cyan", "white_9_5", "neutral_8", "neutral_6_5",
    "neutral_5", "neutral_3_5", "black_2",
]

# Four oils whose colour index differs, so they are genuinely different
# pigments rather than four brands' names for the same one.
PAINTS = [
    ("daler_rowney_georgian_oil_382_viridian_hue_tr_p_g_7", "viridian (PG7)"),
    ("daler_rowney_georgian_oil_123_french_ultramarine_tr_p_b_29",
     "french ultramarine (PB29)"),
    ("daler_rowney_georgian_oil_503_cadmium_red_hue_tr_p_y_73_p_r_112_p_r_210",
     "cadmium red hue"),
    ("daler_rowney_georgian_oil_135_prussian_blue_tr_p_b_27",
     "prussian blue (PB27)"),
    ("daler_rowney_georgian_oil_221_burnt_sienna_tr_p_r_101",
     "burnt sienna (PR101)"),
    ("daler_rowney_georgian_oil_009_titanium_white_op_p_w_6_4",
     "titanium white (PW6)"),
]

# A modest CMOS, exposed for this scene the way a photographer would: the
# white patch sits just under saturation, so under 1% of the frame clips.
#
# There is no auto-exposure anywhere in the sensor path -- the display mapping
# is fixed by the sensor's own full scale and DynamicRange -- so getting the
# exposure right is part of setting up the shot, not something the renderer
# quietly fixes afterwards. Held constant across the sensor comparison so the
# sensitivity curve is the only variable.
SENSOR_BASE = {
    "_pattern": "RGGB",
    "ExposureTime": "8e-5",
    "DynamicRange": "12",
    "PixelPitch": "3.45e-6",
    "FNumber": "2.8",
    "FullWell": "60000",
    "Gain": "12.0",
    "BitDepth": "12",
    "ReadNoise": "2.0",
    "DarkCurrent": "5.0",
    "NoiseSources": "Shot Read",
}


def camera(name, position, gaze_point, fov="40", sensor_ref=None,
           resolution=RESOLUTION, samples=None):
    cam = {
        "_type": "lookAt",
        "Position": position,
        "GazePoint": gaze_point,
        "Up": "0 1 0",
        "FovY": fov,
        "NearDistance": "1",
        "ImageResolution": resolution,
        "NumSamples": samples or SAMPLES,
        "MaxRecursionDepth": "4",
        "MinRecursionDepth": "2",
        "Renderer": "PathTracing",
        "RendererParams": "ImportanceSampling NextEventEstimation MIS_BALANCE RussianRoulette",
        "ImageName": name,
        "Tonemap": {
            "TMO": "Photographic", "TMOOptions": "0.18 1.0",
            "Saturation": "1.0", "Gamma": "2.2", "Extension": ".png",
        },
    }
    if sensor_ref:
        sensor = dict(SENSOR_BASE)
        sensor["_ref"] = sensor_ref
        cam["Sensor"] = sensor
    return cam


def matte(index, spectrum_ref, rgb_hint="0.5 0.5 0.5"):
    """A Lambertian material whose reflectance comes from a measurement.

    The RGB keys still have to be present -- the parser requires them -- but
    they are ignored the moment DiffuseSpectrum resolves, so they are only a
    hint for anyone reading the file.
    """
    return {
        "_id": str(index),
        "AmbientReflectance": "0 0 0",
        "DiffuseReflectance": rgb_hint,
        "SpecularReflectance": "0 0 0",
        "DiffuseSpectrum": {"_ref": "material:" + spectrum_ref},
    }


def emitter(index, radiance_ref, scale):
    return {
        "_id": str(index),
        "AmbientReflectance": "0 0 0",
        "DiffuseReflectance": "0 0 0",
        "SpecularReflectance": "0 0 0",
        "RadianceSpectrum": {"_ref": "light:" + radiance_ref, "_scale": str(scale)},
    }


def quad(v0, v1, v2, v3, vertices):
    """Appends four corners and returns two 1-based triangle index strings.

    Winding is counter-clockwise seen from the front, and the front is the side
    the surface faces -- an emitter wound the wrong way radiates away from the
    scene and renders pure black, which is what the first draft of these scenes
    did.
    """
    base = len(vertices) + 1
    vertices.extend([v0, v1, v2, v3])
    return ["%d %d %d" % (base, base + 1, base + 2),
            "%d %d %d" % (base, base + 2, base + 3)]


def write(name, scene):
    os.makedirs(HERE, exist_ok=True)
    path = os.path.join(HERE, name + ".json")
    with open(path, "w") as f:
        json.dump({"Scene": scene}, f, indent=2)
    print("  %s" % os.path.relpath(path, ROOT))


# ---------------------------------------------------------------------------
# Scene 1 -- the same chart under different real light sources
#
# The reflectances and the camera are held fixed; only the emitter's spectrum
# changes. Differences between these renders are entirely the illuminant, which
# is the thing an RGB renderer cannot show honestly: it would have to collapse
# each lamp to a colour temperature first.
# ---------------------------------------------------------------------------

ILLUMINANTS = [
    ("d65", "light:d65", 42.0, "CIE D65 daylight"),
    ("incandescent", "light:incandescent", 42.0, "measured tungsten lamp"),
    ("fl11", "light:fl11", 42.0, "CIE FL11 narrowband fluorescent"),
    ("led_b3", "light:led_b3", 42.0, "CIE LED-B3 phosphor LED"),
    ("hps", "light:hps", 42.0, "measured high-pressure sodium"),
]


def chart_scene(tag, light_ref, scale, description, sensor_ref=None):
    vertices = []
    faces_by_patch = []

    # 6x4 grid of 1.4-unit patches on a 1.55 pitch, centred on the origin.
    pitch, size = 1.55, 1.4
    half_w = (5 * pitch + size) / 2.0
    half_h = (3 * pitch + size) / 2.0
    for row in range(4):
        for col in range(6):
            x0 = -half_w + col * pitch
            y0 = half_h - row * pitch
            # Wound so the patch faces +Z, towards the camera.
            faces_by_patch.append(quad(
                "%g %g 0" % (x0, y0 - size), "%g %g 0" % (x0 + size, y0 - size),
                "%g %g 0" % (x0 + size, y0), "%g %g 0" % (x0, y0),
                vertices))

    materials = [matte(i + 1, patch) for i, patch in enumerate(CHART)]
    backdrop_material = len(CHART) + 1
    materials.append(matte(backdrop_material, "neutral_5"))
    light_material = backdrop_material + 1
    materials.append(emitter(light_material, light_ref.split(":", 1)[1], scale))

    # A mid-grey backdrop filling the frame. Not decoration: the Photographic
    # operator normalises on the log-average luminance of the whole image, so a
    # black surround pulls that average towards zero and the operator responds
    # by over-brightening everything that is actually lit.
    backdrop_faces = quad("-12 -9 -0.05", "12 -9 -0.05",
                          "12 9 -0.05", "-12 9 -0.05", vertices)

    # A broad emitter panel above and in front, wound to face -Y so it radiates
    # down onto the chart, and kept outside the camera's field of view.
    light_faces = quad("-8 8 -1", "8 8 -1", "8 8 6", "-8 8 6", vertices)

    triangles = []
    for i, faces in enumerate(faces_by_patch):
        for face in faces:
            triangles.append({
                "_id": str(len(triangles) + 1),
                "Material": str(i + 1),
                "Indices": face,
            })
    for face in backdrop_faces:
        triangles.append({
            "_id": str(len(triangles) + 1),
            "Material": str(backdrop_material),
            "Indices": face,
        })

    scene = {
        "_comment": "ColorChecker under %s. Measured patch reflectances; only "
                    "the illuminant differs between these scenes." % description,
        "BackgroundColor": "0 0 0",
        "ShadowRayEpsilon": "0.001",
        "Cameras": {"Camera": [camera(
            os.path.join(OUT, "chart_%s.exr" % tag), "0 0 13.5", "0 0 0",
            sensor_ref=sensor_ref)]},
        "Lights": {"AmbientLight": "0 0 0"},
        "Materials": {"Material": materials},
        "VertexData": {"_data": "\n".join(vertices)},
        "Objects": {
            "Triangle": triangles,
            "LightMesh": [{
                "_id": "1",
                "Material": str(light_material),
                "RadianceSpectrum": {"_ref": light_ref, "_scale": str(scale)},
                "Faces": {"_data": "\n".join(light_faces)},
            }],
        },
    }
    return scene


# ---------------------------------------------------------------------------
# Scene 2 -- one scene, many real cameras
#
# Cameras are per-camera in the scene file, so every sensor sees numerically the
# same photons. Any difference between the outputs is the sensitivity curve
# alone.
# ---------------------------------------------------------------------------

COMPARISON_CAMERAS = [
    ("nikon_d700", "Nikon D700 (full-frame DSLR)"),
    ("canon_5dmarkii", "Canon 5D Mark II (full-frame DSLR)"),
    ("nokia_n900", "Nokia N900 (phone)"),
    ("point_grey_grasshopper_50s5c", "Point Grey Grasshopper (industrial)"),
    ("phase_one", "Phase One (medium format back)"),
]


def camera_comparison_scene():
    scene = chart_scene("cameras", "light:d65", 42.0, "CIE D65 daylight")
    scene["_comment"] = (
        "One chart, five real camera sensitivities. Identical photons reach "
        "every sensor, so the differences are the sensor alone. Compare the "
        "_srgb.png outputs, and the residual in each _sensor_to_xyz.json -- a "
        "sensor that fails the Luther condition cannot be corrected by any 3x3.")
    scene["Cameras"]["Camera"] = [
        camera(os.path.join(OUT, "camera_%s.exr" % slug), "0 0 12", "0 0 0",
               sensor_ref="sensor:" + slug)
        for slug, _ in COMPARISON_CAMERAS
    ]
    return scene


# ---------------------------------------------------------------------------
# Scene 3 -- named artist oils and real foliage, under daylight
# ---------------------------------------------------------------------------

def palette_scene():
    materials = []
    spheres = []
    vertices = []

    subjects = [(ref, label) for ref, label in PAINTS]
    subjects.append(("pine_mean_of_370", "Scots pine needles"))
    subjects.append(("birch_mean_of_337", "birch leaves"))

    for i, (ref, _) in enumerate(subjects):
        materials.append(matte(i + 1, ref))
        col = i % 4
        row = i // 4
        vertices.append("%g %g 0" % (-3.3 + col * 2.2, 1.2 - row * 2.4))
        spheres.append({
            "_id": str(i + 1),
            "Material": str(i + 1),
            "Center": str(len(vertices)),
            "Radius": "1.0",
        })

    # Backdrop, so the spheres sit on something rather than floating in black.
    backdrop = len(materials) + 1
    materials.append(matte(backdrop, "neutral_5"))
    emitter_id = backdrop + 1
    materials.append(emitter(emitter_id, "d65", 30.0))
    fill_id = emitter_id + 1
    materials.append(emitter(fill_id, "d65", 6.0))

    # Sized to overfill the frame -- see the note in chart_scene about the
    # Photographic operator and black surrounds.
    backdrop_faces = quad("-14 -10 -3", "14 -10 -3", "14 10 -3", "-14 10 -3",
                          vertices)
    light_faces = quad("-6 7 0", "6 7 0", "6 7 6", "-6 7 6", vertices)
    # A second, dimmer panel low and in front, standing in for a bounce card.
    # Without it the undersides of the spheres are lit by indirect light alone
    # and stay visibly noisy at any sample count this demo can afford.
    fill_faces = quad("-6 -6 6", "6 -6 6", "6 -6 0", "-6 -6 0", vertices)

    return {
        "_comment": "Named artist oils and measured conifer/birch foliage under "
                    "D65. Every reflectance is a published measurement; the RGB "
                    "keys are ignored.",
        "BackgroundColor": "0 0 0",
        "ShadowRayEpsilon": "0.001",
        # More samples than the charts: the spheres have curved, partly
        # shadowed surfaces where the charts are flat and evenly lit.
        "Cameras": {"Camera": [camera(
            os.path.join(OUT, "palette.exr"), "0 -0.5 13", "0 -0.5 0",
            samples="256")]},
        "Lights": {"AmbientLight": "0 0 0"},
        "Materials": {"Material": materials},
        "VertexData": {"_data": "\n".join(vertices)},
        "Objects": {
            "Sphere": spheres,
            "Mesh": [{
                "_id": str(len(spheres) + 1),
                "Material": str(backdrop),
                "Faces": {"_data": "\n".join(backdrop_faces)},
            }],
            "LightMesh": [
                {
                    "_id": str(len(spheres) + 2),
                    "Material": str(emitter_id),
                    "RadianceSpectrum": {"_ref": "light:d65", "_scale": "30"},
                    "Faces": {"_data": "\n".join(light_faces)},
                },
                {
                    "_id": str(len(spheres) + 3),
                    "Material": str(fill_id),
                    "RadianceSpectrum": {"_ref": "light:d65", "_scale": "6"},
                    "Faces": {"_data": "\n".join(fill_faces)},
                },
            ],
        },
    }


def main():
    print("generating:")
    for tag, ref, scale, description in ILLUMINANTS:
        write("chart_%s" % tag, chart_scene(tag, ref, scale, description))
    write("camera_comparison", camera_comparison_scene())
    write("palette", palette_scene())
    print("\nrender with:  ./tools/render_demo.sh")


if __name__ == "__main__":
    main()
