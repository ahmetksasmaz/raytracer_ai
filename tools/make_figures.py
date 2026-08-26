#!/usr/bin/env python3
"""Renders the pipeline figures into docs/figures/.

Four views of the same system, from the outside in:

    pipeline_overview.png   every executable, what feeds what
    renderer_outputs.png    the decomposition the renderer emits
    sensor_chain.png        the sensor stages, with units
    isp_chain.png           the ISP stages, with colour spaces

Run after changing the pipeline; these are checked in (docs/figures is excepted
from the blanket *.png ignore) so the repo carries a current diagram.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from figure_kit import Figure, BAND, EDGE, INK, MUTED, FILE_FILL, SCALE  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "figures")


def prog_box(f, x, y, w, h, name, subtitle, kind):
    f.box(x, y, w, h, BAND[kind], EDGE[kind])
    f.text(x + w / 2, y + h / 2 - 1, name, size=9.5, bold=True, mono=True,
           align="center")
    if subtitle:
        f.text(x + w / 2, y + h / 2 + 9, subtitle, size=7.2, colour=MUTED,
               align="center")


def file_box(f, x, y, w, h, label, detail):
    """A file on disk. Deliberately a different shape from a program."""
    f.box(x, y, w, h, FILE_FILL, MUTED, r=1.5, lw=0.9, dash=[2.5, 2.0])
    f.text(x + w / 2, y + h / 2 - 0.5, label, size=8, mono=True, align="center")
    if detail:
        f.text(x + w / 2, y + h / 2 + 8.5, detail, size=6.6, colour=MUTED,
               align="center")


def caption(f, x, y, lines, size=7.4):
    for i, line in enumerate(lines):
        f.text(x, y + i * (size + 2.6), line, size=size, colour=MUTED)


# ---------------------------------------------------------------------------
# 1. Overview
# ---------------------------------------------------------------------------
def pipeline_overview():
    W, H = 1180, 620
    f = Figure(W, H)

    f.text(40, 44, "raytracer_ai — the pipeline", size=21, bold=True)
    f.text(40, 64,
           "Every stage is its own executable: one file in, one file out. "
           "Files chain by postfix, <scene>_<stage>.<ext>.",
           size=9.5, colour=MUTED)

    bw, bh, gap = 132, 40, 20
    fw, fh = 132, 30

    # --- band 1: renderer ---------------------------------------------------
    y = 100
    f.text(40, y - 8, "RENDER — scene to spectral radiance", size=9,
           bold=True, colour=EDGE["render"])
    f.box(34, y, W - 68, 96, (0.975, 0.978, 0.99), (0.86, 0.88, 0.94), r=6,
          lw=0.8)

    prog_box(f, 52, y + 28, bw, bh, "raytracer", "path / ray tracer", "render")
    outs = [("_radiance.exr", "31 band · what the camera saw"),
            ("_illumination.exr", "31 band · the light"),
            ("_reflectance.exr", "31 band · the surface")]
    # Fanned to stacked boxes: a single horizontal arrow would run straight
    # through all three and read as one chained flow, which it is not.
    fx = 262
    for i, (label, detail) in enumerate(outs):
        fy = y + 8 + i * 30
        file_box(f, fx, fy, 190, 26, label, detail)
        f.arrow(52 + bw + 4, y + 48, fx - 5, fy + 13, colour=EDGE["render"])

    f.text(486, y + 34, "radiance  =  reflectance ⊙ illumination", size=9,
           mono=True)
    f.text(486, y + 48, "the decomposition, emitted by default;", size=7.6,
           colour=MUTED)
    f.text(486, y + 59, "--no-aov skips the last two", size=7.6, colour=MUTED)

    # --- band 2: sensor -----------------------------------------------------
    y = 232
    f.text(40, y - 8, "SENSOR — radiance to a RAW Bayer mosaic", size=9,
           bold=True, colour=EDGE["sensor"])
    f.box(34, y, W - 68, 150, (0.995, 0.982, 0.972), (0.95, 0.90, 0.85), r=6,
          lw=0.8)

    stages = [
        ("sensor_irradiance", "photons"),
        ("sensor_cfa", "electrons ×3"),
        ("sensor_mosaic", "1 of 3 per site"),
        ("sensor_noise", "shot+dark+read"),
        ("sensor_saturate", "full-well clamp"),
        ("sensor_adc", "gain + quantise"),
    ]
    sw = 158
    positions = []
    for i, (name, sub) in enumerate(stages):
        col, row = i % 3, i // 3
        sx = 56 + col * (sw + 40)
        sy = y + 22 + row * 62
        positions.append((sx, sy))
        prog_box(f, sx, sy, sw, bh, name, sub, "sensor")
        if col < 2:
            f.arrow(sx + sw + 4, sy + bh / 2, sx + sw + 36, sy + bh / 2,
                    colour=EDGE["sensor"])

    # The wrap from sensor_mosaic to sensor_noise, routed rather than drawn
    # straight: sensor_adc sits directly below sensor_mosaic, so a vertical
    # arrow would claim a connection that does not exist.
    (mx, my), (nx, ny) = positions[2], positions[3]
    f.elbow([(mx + sw / 2, my + bh + 3), (mx + sw / 2, my + bh + 12),
             (nx + sw / 2, my + bh + 12), (nx + sw / 2, ny - 4)],
            colour=EDGE["sensor"])

    # Separated: these two take no image, so they are not links in the chain.
    side_x = 56 + 3 * (sw + 40) + 14
    f.c.set_source_rgb(0.88, 0.84, 0.80)
    f.c.set_line_width(0.9)
    f.c.set_dash([3, 3])
    f.c.move_to(side_x - 18, y + 16); f.c.line_to(side_x - 18, y + 134)
    f.c.stroke(); f.c.set_dash([])
    f.text(side_x, y + 16, "take no image — only the sensor's own curves",
           size=7.0, colour=MUTED)
    prog_box(f, side_x, y + 22, 168, bh, "sensor_ccm", "wb gains + 2 matrices",
             "sensor")
    prog_box(f, side_x, y + 84, 168, bh, "sensor_illum_chroma",
             "per-pixel r/g, b/g", "sensor")

    # --- band 3: ISP --------------------------------------------------------
    y = 418
    f.text(40, y - 8, "ISP — RAW to a viewable image", size=9, bold=True,
           colour=EDGE["isp"])
    f.box(34, y, W - 68, 118, (0.972, 0.99, 0.977), (0.86, 0.94, 0.89), r=6,
          lw=0.8)

    isp_stages = [
        ("isp_blacklevel", "DN → [0,1]"),
        ("isp_demosaic", "→ sensorRGB"),
        ("isp_whitebalance", "von Kries"),
        ("isp_colormatrix", "→ CIE XYZ"),
        ("isp_srgb", "→ sRGB PNG"),
    ]
    iw = 178
    for i, (name, sub) in enumerate(isp_stages):
        sx = 52 + i * (iw + 34)
        if i < 4:
            prog_box(f, sx, y + 22, iw, bh, name, sub, "isp")
            f.arrow(sx + iw + 4, y + 42, sx + iw + 30, y + 42,
                    colour=EDGE["isp"])
        else:
            prog_box(f, sx, y + 22, iw, bh, name, sub, "isp")

    prog_box(f, 52, y + 76, iw, 32, "raw_preview", "RAW, uncorrected", "tool")
    prog_box(f, 52 + iw + 34, y + 76, iw, 32, "isp_preview",
             "no colour transform", "tool")
    prog_box(f, 52 + 2 * (iw + 34), y + 76, iw, 32, "chroma_preview",
             "the illuminant map", "tool")
    f.text(52 + 3 * (iw + 34), y + 92,
           "viewing tools — never in the measurement path", size=7.6,
           colour=MUTED)
    f.text(52 + 3 * (iw + 34), y + 103,
           "isp_srgb is the only output that leaves sensor space", size=7.6,
           colour=MUTED)

    caption(f, 40, H - 34, [
        "One render, many cameras: _radiance.exr is sensor-independent, so pointing the sensor stages at another config develops the same",
        "render through a different camera with no re-rendering. That is the reason for the split.",
    ])
    return f.save(os.path.join(OUT, "pipeline_overview.png"))


# ---------------------------------------------------------------------------
# 2. Renderer outputs
# ---------------------------------------------------------------------------
def renderer_outputs():
    W, H = 1080, 500
    f = Figure(W, H)
    f.text(40, 44, "What the renderer emits", size=21, bold=True)
    f.text(40, 64,
           "Not just a picture: the decomposition of what each pixel saw, "
           "captured at the first non-specular vertex.", size=9.5, colour=MUTED)

    prog_box(f, 46, 118, 150, 44, "raytracer", "one camera path", "render")

    rows = [
        (110, "_reflectance.exr", "diffuse albedo of the first",
         "non-specular surface, textures included"),
        (186, "_illumination.exr", "the radiance that pixel would have shown",
         "had the surface been perfectly white"),
        (262, "_radiance.exr", "what the camera actually saw",
         "the product of the two above"),
    ]
    for y, label, line1, line2 in rows:
        file_box(f, 250, y, 168, 34, label, "31 bands · float EXR")
        f.arrow(200, 140, 246, y + 17, colour=EDGE["render"])
        f.text(432, y + 15, line1, size=8.2)
        f.text(432, y + 26, line2, size=8.2, colour=MUTED)

    f.box(262, 322, W - 312, 54, (0.975, 0.978, 0.99), (0.80, 0.84, 0.93),
          r=5, lw=1.0)
    f.text(278, 344, "radiance(λ)  =  reflectance(λ)  ×  illumination(λ)",
           size=12, mono=True, bold=True)
    f.text(278, 362,
           "exact where the first vertex has no specular lobe — no per-band "
           "scalar splits a highlight into surface colour times light colour",
           size=7.6, colour=MUTED)

    f.text(46, 200, "why not", size=8, bold=True, colour=MUTED)
    f.text(46, 212, "radiance", size=8, mono=True, colour=MUTED)
    f.text(46, 223, "÷ albedo?", size=8, mono=True, colour=MUTED)
    caption(f, 46, 242, [
        "It is undefined on a", "black surface, which", "still receives light.",
        "And it would make the", "identity above true by", "construction, so no",
        "test could falsify it.",
    ], size=7.0)

    prog_box(f, 250, 400, 190, 40, "sensor_illum_chroma", "× the CFA curves",
             "sensor")
    file_box(f, 476, 404, 150, 32, "_illumchroma.exr", "2 ch · r/g and b/g")
    prog_box(f, 662, 400, 190, 40, "isp_whitebalance", "--chroma, per pixel",
             "isp")

    # From the ILLUMINATION map, not the radiance -- routed down the left
    # margin so it neither crosses the equation box nor appears to start at the
    # wrong file. Which cube feeds this is the whole point of the figure.
    f.elbow([(250, 203), (236, 203), (236, 420), (246, 420)],
            colour=EDGE["sensor"])
    f.arrow(444, 420, 472, 420, colour=EDGE["sensor"])
    f.arrow(630, 420, 658, 420, colour=EDGE["isp"])
    f.text(866, 414, "ground truth for", size=8)
    f.text(866, 426, white_balance_label := "white balance", size=8)
    f.text(866, 440, "— what an estimator", size=7.2, colour=MUTED)
    f.text(866, 450, "is trying to guess", size=7.2, colour=MUTED)
    return f.save(os.path.join(OUT, "renderer_outputs.png"))


# ---------------------------------------------------------------------------
# 3 & 4. Chain figures
# ---------------------------------------------------------------------------
def chain_figure(title, strapline, kind, rows, note, filename):
    W = 1160
    H = 150 + len(rows) * 74 + 60
    f = Figure(W, H)
    f.text(40, 44, title, size=21, bold=True)
    f.text(40, 64, strapline, size=9.5, colour=MUTED)

    f.text(52, 100, "STAGE", size=7.4, bold=True, colour=MUTED)
    f.text(232, 100, "READS", size=7.4, bold=True, colour=MUTED)
    f.text(470, 100, "WRITES", size=7.4, bold=True, colour=MUTED)
    f.text(716, 100, "WHAT THE NUMBERS MEAN", size=7.4, bold=True, colour=MUTED)
    f.c.set_source_rgb(*MUTED)
    f.c.set_line_width(0.6)
    f.c.move_to(46, 106); f.c.line_to(W - 46, 106); f.c.stroke()

    y = 120
    for name, sub, reads, reads_d, writes, writes_d, meaning in rows:
        prog_box(f, 46, y, 172, 42, name, sub, kind)
        file_box(f, 232, y + 4, 210, 34, reads, reads_d)
        f.arrow(446, y + 21, 466, y + 21, colour=EDGE[kind])
        file_box(f, 470, y + 4, 210, 34, writes, writes_d)
        for i, line in enumerate(meaning):
            f.text(716, y + 17 + i * 11, line, size=7.8,
                   colour=INK if i == 0 else MUTED)
        y += 74

    caption(f, 46, H - 40, note)
    return f.save(os.path.join(OUT, filename))


def sensor_chain():
    rows = [
        ("sensor_irradiance", "radiometry", "_radiance.exr", "31 band · L(λ)",
         "_photons.exr", "31 band · photon count",
         ["photons = L · A · Ω · t · λ/hc",
          "A = pixel area, Ω = π/4N². The λ/hc factor is",
          "wavelength dependent — impossible on an RGB triple."]),
        ("sensor_cfa", "QE × colour filter", "_photons.exr", "31 band",
         "_electrons.exr", "3 ch · electrons",
         ["Collapses 31 numbers into 3.",
          "Everything downstream is stuck with whatever",
          "these three curves preserved."]),
        ("sensor_mosaic", "Bayer sampling", "_electrons.exr", "3 ch",
         "_mosaic.exr", "1 ch · electrons",
         ["Keeps one channel per photosite.",
          "The other two are discarded — the demosaic",
          "later only ever guesses them back."]),
        ("sensor_noise", "seeded, reproducible", "_mosaic.exr", "1 ch",
         "_noisy.exr", "1 ch · electrons",
         ["Poisson shot + Poisson dark + Gaussian read.",
          "Shot noise is not a defect: photon arrival is",
          "Poisson, so even a perfect detector sees √N."]),
        ("sensor_saturate", "in the well", "_noisy.exr", "1 ch",
         "_wellclamped.exr", "1 ch · electrons",
         ["Clamped per channel, before readout.",
          "That ordering is why a blown highlight shifts",
          "hue instead of going neutral white."]),
        ("sensor_adc", "gain + quantise", "_wellclamped.exr", "1 ch",
         "_raw.pgm / .exr", "1 ch · integer DN",
         ["DN = floor(electrons / gain), clamped.",
          "Discrete from here on. Gain is e⁻/DN, applied",
          "after all noise — so it cannot change SNR."]),
    ]
    return chain_figure(
        "The sensor chain", "Spectral radiance in, a RAW Bayer mosaic out. "
        "Units change at every step.", "sensor", rows,
        ["The whole chain is driven by a standalone sensor config — a sensor is hardware, not a property of a scene.",
         "sensor_ccm sits beside this chain rather than in it: it needs no image, only the sensor's own curves."],
        "sensor_chain.png")


def isp_chain():
    rows = [
        ("isp_blacklevel", "fixed window", "_raw.pgm", "1 ch · DN",
         "_linearized.exr", "1 ch · [0,1]",
         ["Saturation at the top of the ADC range, black",
          "DynamicRange stops below. Constants of the",
          "sensor — never of the frame. No auto-exposure."]),
        ("isp_demosaic", "bilinear", "_linearized.exr", "1 ch · [0,1]",
         "_demosaiced.exr", "3 ch · sensorRGB",
         ["Still sensor space. These channels are what",
          "THIS camera's filters recorded, not red, green",
          "and blue in any standard sense."]),
        ("isp_whitebalance", "global or per-pixel", "_demosaiced.exr",
         "3 ch · sensorRGB", "_wb.exr", "3 ch · sensorRGB",
         ["R /= r/g, G untouched, B /= b/g.",
          "Green is never scaled — that is what keeps the",
          "exposure window and the matrix anchor valid."]),
        ("isp_colormatrix", "fitted 3×3", "_wb.exr", "3 ch · sensorRGB",
         "_xyz.exr", "3 ch · CIE XYZ",
         ["Leaves the camera behind: now comparable",
          "across sensors. Never exact — real filters fail",
          "the Luther condition, and the residual says how far."]),
        ("isp_srgb", "display encode", "_xyz.exr", "3 ch · CIE XYZ",
         "_srgb.png", "3 ch · 8-bit sRGB",
         ["The sRGB transfer function, not a 2.2 gamma.",
          "The only output meant for looking at rather",
          "than computing on."]),
    ]
    return chain_figure(
        "The ISP chain", "A RAW in, a viewable image out. The colour space "
        "changes under you at each step.", "isp", rows,
        ["matrix_after_wb pairs with the white balance; matrix_no_wb pairs with skipping it. Mixing them double- or under-corrects,",
         "and both produce a perfectly plausible image — which is why the pipeline asserts the pairing rather than trusting it."],
        "isp_chain.png")


def main():
    os.makedirs(OUT, exist_ok=True)
    for path in (pipeline_overview(), renderer_outputs(), sensor_chain(),
                 isp_chain()):
        size = os.path.getsize(path)
        import struct
        with open(path, "rb") as h:
            h.read(16)
            w, ht = struct.unpack(">II", h.read(8))
        print("  %-28s %5dx%-5d %6.1f KB" %
              (os.path.relpath(path, ROOT), w, ht, size / 1024.0))


if __name__ == "__main__":
    main()
