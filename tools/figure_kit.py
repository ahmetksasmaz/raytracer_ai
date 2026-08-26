#!/usr/bin/env python3
"""Small cairo drawing helpers shared by the pipeline figures.

Everything is laid out in "design units" and rendered at SCALE, so the same
code produces a crisp 300-DPI-ish PNG without any hand-tuned pixel offsets.
matplotlib and graphviz are not available here; cairo is, and for boxes and
arrows it is the better tool anyway.
"""

import cairo
import math

SCALE = 3.0

# One palette for all four figures, so a stage keeps its colour wherever it
# appears. Chosen to stay distinguishable in greyscale, since these end up in
# printed reports.
INK = (0.11, 0.12, 0.14)
MUTED = (0.42, 0.45, 0.50)
PAPER = (1.0, 1.0, 1.0)
BAND = {
    "render": (0.90, 0.93, 0.99),
    "sensor": (0.99, 0.94, 0.88),
    "isp": (0.89, 0.96, 0.91),
    "tool": (0.95, 0.95, 0.96),
}
EDGE = {
    "render": (0.24, 0.36, 0.72),
    "sensor": (0.78, 0.45, 0.16),
    "isp": (0.16, 0.55, 0.32),
    "tool": (0.45, 0.47, 0.52),
}
FILE_FILL = (0.99, 0.99, 1.00)


class Figure:
    def __init__(self, width, height):
        self.w, self.h = width, height
        self.surface = cairo.ImageSurface(
            cairo.FORMAT_RGB24, int(width * SCALE), int(height * SCALE))
        self.c = cairo.Context(self.surface)
        self.c.scale(SCALE, SCALE)
        self.c.set_source_rgb(*PAPER)
        self.c.paint()
        self.c.set_line_join(cairo.LINE_JOIN_ROUND)
        self.c.set_line_cap(cairo.LINE_CAP_ROUND)

    # --- text ---------------------------------------------------------------
    def font(self, size, bold=False, mono=False):
        family = "DejaVu Sans Mono" if mono else "DejaVu Sans"
        self.c.select_font_face(
            family, cairo.FONT_SLANT_NORMAL,
            cairo.FONT_WEIGHT_BOLD if bold else cairo.FONT_WEIGHT_NORMAL)
        self.c.set_font_size(size)

    def text(self, x, y, s, size=9, bold=False, mono=False, colour=INK,
             align="left"):
        self.font(size, bold, mono)
        self.c.set_source_rgb(*colour)
        extents = self.c.text_extents(s)
        if align == "center":
            x -= extents.width / 2 + extents.x_bearing
        elif align == "right":
            x -= extents.width + extents.x_bearing
        self.c.move_to(x, y)
        self.c.show_text(s)
        return extents.width

    def width_of(self, s, size=9, bold=False, mono=False):
        self.font(size, bold, mono)
        return self.c.text_extents(s).width

    # --- shapes -------------------------------------------------------------
    def rounded(self, x, y, w, h, r=4):
        c = self.c
        c.new_sub_path()
        c.arc(x + w - r, y + r, r, -math.pi / 2, 0)
        c.arc(x + w - r, y + h - r, r, 0, math.pi / 2)
        c.arc(x + r, y + h - r, r, math.pi / 2, math.pi)
        c.arc(x + r, y + r, r, math.pi, 3 * math.pi / 2)
        c.close_path()

    def box(self, x, y, w, h, fill, edge, r=4, lw=1.1, dash=None):
        self.rounded(x, y, w, h, r)
        self.c.set_source_rgb(*fill)
        self.c.fill_preserve()
        self.c.set_source_rgb(*edge)
        self.c.set_line_width(lw)
        self.c.set_dash(dash or [])
        self.c.stroke()
        self.c.set_dash([])

    def arrow(self, x0, y0, x1, y1, colour=MUTED, lw=1.2, head=4.5, dash=None):
        c = self.c
        c.set_source_rgb(*colour)
        c.set_line_width(lw)
        c.set_dash(dash or [])
        c.move_to(x0, y0)
        c.line_to(x1, y1)
        c.stroke()
        c.set_dash([])
        angle = math.atan2(y1 - y0, x1 - x0)
        for sign in (1, -1):
            a = angle + sign * 2.6
            c.move_to(x1, y1)
            c.line_to(x1 + head * math.cos(a), y1 + head * math.sin(a))
        c.close_path()
        c.fill()

    def elbow(self, points, colour=MUTED, lw=1.2, head=4.5):
        """A polyline with an arrowhead on the last segment.

        Used where a straight arrow would cross other boxes and imply a
        connection that is not there -- e.g. the sensor chain wrapping from the
        end of one row back to the start of the next.
        """
        c = self.c
        c.set_source_rgb(*colour)
        c.set_line_width(lw)
        c.move_to(*points[0])
        for point in points[1:]:
            c.line_to(*point)
        c.stroke()
        (x0, y0), (x1, y1) = points[-2], points[-1]
        self.arrow(x0, y0, x1, y1, colour=colour, lw=lw, head=head)

    def save(self, path):
        self.surface.write_to_png(path)
        return path
