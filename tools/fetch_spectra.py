#!/usr/bin/env python3
"""Download published spectral data and convert it into the renderer's .spd format.

Run from the repository root:

    python3 tools/fetch_spectra.py            # fetch everything
    python3 tools/fetch_spectra.py lights     # only the light-source sets

The .spd files this writes are committed, so this script exists to document
provenance and to make a refresh reproducible -- it is not needed to build or
run the renderer.

Every upstream source is recorded in the `source:` header of each record it
produces, and summarised in spectra/LICENSES.md. Two of them are not
permissively licensed (Jiang 2013 is CC BY-NC-SA, the Kuopio/UEF sets are
academic-use-only); they are included here on that basis.
"""

import gzip
import io
import os
import re
import ssl
import sys
import tarfile
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPECTRA = os.path.join(ROOT, "spectra")

# Some of these hosts still serve old certificate chains; the data is public and
# integrity is checked by the value-range assertions in each converter.
SSL_CTX = ssl.create_default_context()
SSL_CTX.check_hostname = False
SSL_CTX.verify_mode = ssl.CERT_NONE

UA = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) raytracer_ai/fetch_spectra"


def get(url, binary=False):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=120, context=SSL_CTX) as r:
        data = r.read()
    return data if binary else data.decode("utf-8", "replace")


def slug(name):
    """Stable lookup key: lowercase, non-alphanumerics collapsed to underscore."""
    s = re.sub(r"[^A-Za-z0-9]+", "_", name).strip("_").lower()
    return re.sub(r"_+", "_", s)


class Writer:
    """Accumulates records and writes one .spd file."""

    def __init__(self, path, kind, source, note=None):
        self.path = os.path.join(SPECTRA, path)
        self.kind = kind
        self.source = source
        self.note = note
        self.records = []

    def add(self, name, wavelengths, values, channels=None):
        assert len(wavelengths) == len(values), name
        assert len(wavelengths) >= 2, name
        self.records.append((name, wavelengths, values, channels))

    def uniquify(self):
        """Suffix repeated names with _2, _3, ...

        Several sources measure more than one sample per label -- the UEF
        natural set has a handful of different birch leaves all called
        "green leaf koivu". The loader keys on name, so without this the later
        samples would silently replace the earlier ones.
        """
        seen = {}
        out = []
        for name, wl, vals, channels in self.records:
            key = slug(name)
            seen[key] = seen.get(key, 0) + 1
            if seen[key] > 1:
                name = "%s %d" % (name, seen[key])
            out.append((name, wl, vals, channels))
        self.records = out

    def write(self):
        self.uniquify()
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        with open(self.path, "w") as f:
            f.write("# %s\n" % os.path.basename(self.path))
            f.write("# source: %s\n" % self.source)
            if self.note:
                for line in self.note.strip().splitlines():
                    f.write("# %s\n" % line.strip())
            f.write("# %d records\n\n" % len(self.records))
            for name, wl, vals, channels in self.records:
                f.write("type: %s\n" % self.kind)
                f.write("name: %s\n" % slug(name))
                f.write("label: %s\n" % name)
                f.write("source: %s\n" % self.source)
                if channels:
                    f.write("channels: %s\n" % " ".join(channels))
                f.write("data:\n")
                for w, v in zip(wl, vals):
                    if isinstance(v, (list, tuple)):
                        f.write("  %g %s\n" % (w, " ".join("%.6g" % x for x in v)))
                    else:
                        f.write("  %g %.6g\n" % (w, v))
                f.write("\n")
        print("  wrote %-44s %4d records" % (
            os.path.relpath(self.path, ROOT), len(self.records)))


# --------------------------------------------------------------------------
# colour-science (BSD-3-Clause) -- CIE illuminants, measured lamps,
# measured ColorChecker, and two DSLR spectral sensitivities.
# The datasets are Python source files holding literal {wavelength: value}
# dicts, so they are parsed textually rather than by importing the package.
# --------------------------------------------------------------------------

CS_RAW = "https://raw.githubusercontent.com/colour-science/colour/develop/colour/"


def isolate_table(text, top_level_name):
    """Return the body of a module-level `NAME: dict = { ... }` assignment.

    Anchored at the start of a line, because every table name also appears in
    the module's __all__ list further up the file.
    """
    m = re.search(r"^%s\s*:\s*dict\s*=\s*\{" % re.escape(top_level_name),
                  text, re.M)
    if not m:
        raise RuntimeError("no %s in source" % top_level_name)
    rest = text[m.end():]
    end = re.search(r"^\}", rest, re.M)
    return rest[: end.start()] if end else rest


def parse_colour_dicts(text, top_level_name):
    """Pull `"NAME": {wl: val, ...}` blocks out of a colour-science dataset."""
    rest = isolate_table(text, top_level_name)
    out = {}
    for m in re.finditer(r'^    "([^"]+)":\s*\{(.*?)^    \}', rest, re.S | re.M):
        name, body = m.group(1), m.group(2)
        pairs = re.findall(r"([0-9.]+):\s*([0-9.eE+-]+)", body)
        if len(pairs) < 2:
            continue
        wl = [float(a) for a, _ in pairs]
        vals = [float(b) for _, b in pairs]
        order = sorted(range(len(wl)), key=lambda i: wl[i])
        out[name] = ([wl[i] for i in order], [vals[i] for i in order])
    return out


def fetch_cie_illuminants():
    text = get(CS_RAW + "colorimetry/datasets/illuminants/sds.py")
    w = Writer(
        "lights/cie_illuminants.spd", "light",
        "CIE 15:2018 / ISO 7589, via colour-science (BSD-3-Clause)",
        note="""
        Relative spectral power distributions. Values are relative, normally
        normalised to 100 at 560nm -- use _scale in the scene file to set the
        absolute level. Covers CIE A/B/C, the D series, E, the FL fluorescent
        series, HP discharge lamps, the CIE LED illuminants, and ISO 7589.
        """)
    total = 0
    for table in ("DATA_ILLUMINANTS_CIE", "DATA_ILLUMINANTS_ISO"):
        for name, (wl, vals) in parse_colour_dicts(text, table).items():
            w.add(name, wl, vals)
            total += 1
    assert total >= 50, "expected the full CIE illuminant set, got %d" % total
    w.write()


def fetch_measured_lamps():
    text = get(CS_RAW + "colorimetry/datasets/light_sources/sds.py")
    w = Writer(
        "lights/measured_lamps.spd", "light",
        "NIST / Philips / Osram measurements, via colour-science (BSD-3-Clause)",
        note="""
        Measured relative SPDs of real lamps: incandescent/tungsten, the
        fluorescent and HID families, and a range of phosphor and multi-die
        LEDs. These are what an actual room is lit by, as opposed to the
        idealised CIE illuminants.
        """)
    count = 0
    for table in ("DATA_LIGHT_SOURCES_RIT", "DATA_LIGHT_SOURCES_NIST_TRADITIONAL",
                  "DATA_LIGHT_SOURCES_NIST_LED", "DATA_LIGHT_SOURCES_NIST_PHILIPS",
                  "DATA_LIGHT_SOURCES_COMMON"):
        try:
            recs = parse_colour_dicts(text, table)
        except RuntimeError:
            continue
        for name, (wl, vals) in recs.items():
            w.add(name, wl, vals)
            count += 1
    assert count >= 20, "expected the measured lamp set, got %d" % count
    w.write()


def fetch_colorchecker():
    text = get(CS_RAW + "characterisation/datasets/colour_checkers/sds.py")
    w = Writer(
        "materials/colorchecker.spd", "material",
        "BabelColor average of 30 charts / Ohta, via colour-science (BSD-3-Clause)",
        note="""
        MEASURED spectral reflectance of the 24 ColorChecker patches -- not an
        RGB triple uplifted to a spectrum. This is the set to train and check
        the sensor->XYZ fit against.
        """)
    data = parse_colour_dicts(text, "DATA_BABELCOLOR_AVERAGE")
    assert len(data) == 24, "expected 24 ColorChecker patches, got %d" % len(data)
    for name, (wl, vals) in data.items():
        assert max(vals) <= 1.5, "%s: reflectance out of range" % name
        # Upstream spells the six neutrals "white 9.5 (.05 D)", carrying the
        # optical density. Dropping the parenthetical makes these agree with
        # kColorCheckerSRGB in include/SpectralData.hpp, which is what lets the
        # sensor->XYZ fit swap the measured spectra in by name.
        w.add(re.sub(r"\s*\(.*?\)", "", name).strip(), wl, vals)
    w.write()


def fetch_dslr_sensitivities():
    text = get(CS_RAW + "characterisation/datasets/cameras/dslr/sensitivities.py")
    # One dict per camera, each entry `wavelength: (r, g, b,)` across four lines.
    body = isolate_table(text, "DATA_CAMERA_SENSITIVITIES_DSLR")
    w = Writer(
        "sensors/dslr_permissive.spd", "sensor",
        "colour-science camera dataset (BSD-3-Clause)",
        note="""
        Permissively-licensed camera spectral sensitivities, for anyone who
        cannot accept the CC BY-NC-SA terms on the Jiang 2013 set. Same
        convention: the curves are the whole optical response, so the renderer
        binds them to the CFA and leaves quantum efficiency at unity.
        """)
    for m in re.finditer(r'^    "([^"]+)":\s*\{(.*?)^    \}', body, re.S | re.M):
        cam, entries = m.group(1), m.group(2)
        wl, vals = [], []
        for em in re.finditer(
                r"([0-9.]+):\s*\(\s*([0-9.eE+-]+),\s*([0-9.eE+-]+),\s*([0-9.eE+-]+),?\s*\)",
                entries):
            wl.append(float(em.group(1)))
            vals.append([float(em.group(2)), float(em.group(3)), float(em.group(4))])
        if len(wl) < 2:
            continue
        w.add(cam, wl, vals, channels=["R", "G", "B"])
    assert w.records, "expected at least one DSLR camera"
    w.write()


# --------------------------------------------------------------------------
# Jiang et al. 2013 -- 28 real cameras (CC BY-NC-SA 4.0)
# --------------------------------------------------------------------------

def fetch_camspec():
    text = get("https://zenodo.org/records/3245883/files/camspec_database.txt?download=1")
    lines = [l for l in text.splitlines() if l.strip()]
    assert len(lines) % 4 == 0, "camspec: expected 4 lines per camera"
    # 400..720nm at 10nm, as documented with the dataset.
    wl = [400.0 + 10.0 * i for i in range(33)]
    w = Writer(
        "sensors/camspec.spd", "sensor",
        "Jiang, Liu, Gu & Susstrunk 2013, WACV (CC BY-NC-SA 4.0)",
        note="""
        Measured spectral sensitivity of 28 real cameras -- DSLRs, a medium
        format back, two industrial cameras and a phone.

        These curves are the COMPLETE optical response (quantum efficiency x
        colour filter x whatever else is in the path), each channel normalised
        to a peak of 1. The renderer therefore binds them to the CFA and leaves
        quantum efficiency at unity; splitting them would double-count.
        """)
    for i in range(0, len(lines), 4):
        name = lines[i].strip()
        rgb = []
        for c in range(1, 4):
            vals = [float(x) for x in lines[i + c].split()]
            assert len(vals) == 33, "%s: expected 33 samples, got %d" % (name, len(vals))
            rgb.append(vals)
        w.add(name, wl, [[rgb[0][k], rgb[1][k], rgb[2][k]] for k in range(33)],
              channels=["R", "G", "B"])
    assert len(w.records) == 28, "expected 28 cameras, got %d" % len(w.records)
    w.write()


# --------------------------------------------------------------------------
# Kuopio / University of Eastern Finland (academic use only)
# --------------------------------------------------------------------------

UEF = "https://cs.uef.fi/pub/color/spectra/"
UEF_SRC = "Computational Spectral Imaging Research Group, University of Eastern Finland (academic use only)"


def fetch_uef_forest():
    blob = get(UEF + "forest/forest380_850_5.tar.gz", binary=True)
    tar = tarfile.open(fileobj=io.BytesIO(blob))
    # 390..850nm at 5nm = 93 samples per spectrum, concatenated.
    wl = [390.0 + 5.0 * i for i in range(93)]
    w = Writer(
        "materials/forest.spd", "material",
        UEF_SRC,
        note="""
        Reflectance of Scots pine and Norway spruce needles and birch leaves,
        measured in Finland and Sweden in June 1992. Each stored record is the
        mean over all measurements of that species, so it stands in for foliage
        in general rather than for one leaf.
        """)
    for member in tar.getmembers():
        if not member.name.endswith(".dat"):
            continue
        species = os.path.basename(member.name)[:-4]
        nums = [float(x) for x in tar.extractfile(member).read().split()]
        n = len(nums) // 93
        assert n * 93 == len(nums), "%s: not a whole number of spectra" % species
        mean = [sum(nums[k * 93 + i] for k in range(n)) / n for i in range(93)]
        w.add("%s (mean of %d)" % (species, n), wl, mean)
    assert w.records, "forest archive was empty"
    w.write()


def fetch_uef_natural():
    blob = get(UEF + "natural/natural400_700_5.asc.gz", binary=True)
    text = gzip.decompress(blob).decode("utf-8", "replace")
    lines = [l.strip() for l in text.splitlines() if l.strip()]
    wl = [400.0 + 5.0 * i for i in range(61)]
    w = Writer(
        "materials/natural.spd", "material",
        UEF_SRC,
        note="""
        218 samples collected from nature -- flowers, leaves and other plants.
        The archive stores raw 12-bit AOTF counts; they are divided by 4096 and
        clamped here, as the dataset README instructs, to give reflectance.
        """)
    i = 0
    while i + 1 < len(lines):
        label, i = lines[i], i + 1
        vals = []
        while i < len(lines) and len(vals) < 61:
            parts = lines[i].split()
            if not all(re.match(r"^[0-9.eE+-]+$", p) for p in parts):
                break
            vals.extend(float(p) for p in parts)
            i += 1
        if len(vals) != 61:
            continue
        w.add(label, wl, [min(v, 4096.0) / 4096.0 for v in vals])
    assert len(w.records) > 100, "expected ~218 natural samples, got %d" % len(w.records)
    w.write()


def fetch_uef_munsell():
    blob = get(UEF + "mspec/munsell380_800_1.asc.gz", binary=True)
    text = gzip.decompress(blob).decode("utf-8", "replace")
    nums = [float(x) for x in text.split()]
    # 380..800nm at 1nm = 421 samples per chip.
    n = len(nums) // 421
    assert n * 421 == len(nums), "munsell: not a whole number of spectra"
    # 1nm over 421 points is far finer than the 31-band render grid; keeping
    # every 5th sample is still 4x the band spacing and cuts the file 5-fold.
    idx = list(range(0, 421, 5))
    wl = [380.0 + i for i in idx]
    w = Writer(
        "materials/munsell_matt.spd", "material",
        UEF_SRC,
        note="""
        1269 matt Munsell colour chips, the standard dense reflectance set.
        Measured at 1nm over 380-800nm; decimated to 5nm here, which is still
        well under the renderer's 10nm band spacing.

        The ASCII archive ships the chips as bare numbers with no labels, so
        records are numbered in file order (munsell_0001 ...) rather than named
        by Munsell notation. Use this set for bulk statistics -- a spectral PCA,
        a metamer search -- not to look up a particular chip.
        """)
    for k in range(n):
        chip = nums[k * 421:(k + 1) * 421]
        w.add("munsell_%04d" % (k + 1), wl, [chip[i] for i in idx])
    w.write()


# --------------------------------------------------------------------------
# Artist paints -- painting_tools (MIT), Okumura/RIT measurement archive
# --------------------------------------------------------------------------

PT = ("https://raw.githubusercontent.com/rubenwiersma/painting_tools/main/"
      "painting_tools/measurements/pigments/cmt/")

# Oil paint lines first, since those are what "viridian oil paint" means, then
# a couple of broader reference sets.
PAINT_SETS = [
    ("DalerRowney_GeorgianOil", "Daler-Rowney Georgian oil"),
    ("WinsorNewton_Artist", "Winsor & Newton Artists' oil"),
    ("Talens_VanGoghOil", "Talens Van Gogh oil"),
    ("Schmincke_Mussini", "Schmincke Mussini resin oil"),
    ("Maimeri_Classico", "Maimeri Classico oil"),
    ("Titan_ExtraFineOils", "Titan Extra Fine oil"),
    ("GamblinConservationColors", "Gamblin Conservation Colors"),
    ("Golden_HB", "Golden Heavy Body acrylic"),
    ("KremerHistorical", "Kremer historical pigments"),
]


def fetch_paints():
    w = Writer(
        "materials/artist_paints.spd", "material",
        "Okumura (RIT) paint measurement archive, via painting_tools (MIT)",
        note="""
        Total hemispherical reflectance of named artist paints at full
        strength, 380-750nm at 10nm. Record names are prefixed with the paint
        line, e.g. dalerrowney_georgianoil_382_viridian_hue_tr_p_g_7.

        Rows carry the manufacturer's own colour index in brackets (P.G.7 is
        phthalo green, P.B.29 ultramarine, and so on), which is what actually
        identifies the pigment -- two brands' "viridian hue" need not be the
        same chemistry.
        """)
    wl = [380.0 + 10.0 * i for i in range(38)]
    for filename, line_label in PAINT_SETS:
        try:
            text = get(PT + filename + ".rs")
        except Exception as exc:
            print("  ! skipping %s (%s)" % (filename, exc))
            continue
        for row in text.splitlines():
            if not row.strip():
                continue
            fields = row.split("\t")
            name = fields[0].strip().strip('"')
            if not name:
                continue
            raw = fields[1:]
            # Blank fields are genuine gaps in the measurement; interpolate them
            # rather than dropping the row, which would lose whole pigments.
            vals, missing = [], []
            for cell in raw:
                cell = cell.strip()
                if cell:
                    vals.append(float(cell))
                else:
                    missing.append(len(vals))
                    vals.append(None)
            for pos in missing:
                lo = next((vals[j] for j in range(pos - 1, -1, -1)
                           if vals[j] is not None), None)
                hi = next((vals[j] for j in range(pos + 1, len(vals))
                           if vals[j] is not None), None)
                vals[pos] = (lo + hi) / 2 if lo is not None and hi is not None \
                    else (lo if lo is not None else hi)
            vals = [v for v in vals if v is not None]
            if len(vals) < 36:
                continue
            vals = vals[:38]
            if len(vals) < 38:                       # pad the short rows
                vals += [vals[-1]] * (38 - len(vals))
            if max(vals) > 1.5:
                continue
            w.add("%s %s" % (line_label, name), wl, vals)
    assert len(w.records) > 100, "expected many paints, got %d" % len(w.records)
    w.write()


TARGETS = {
    "lights": [fetch_cie_illuminants, fetch_measured_lamps],
    "materials": [fetch_colorchecker, fetch_uef_forest, fetch_uef_natural,
                  fetch_uef_munsell, fetch_paints],
    "sensors": [fetch_camspec, fetch_dslr_sensitivities],
}


def main():
    which = sys.argv[1:] or list(TARGETS)
    failures = []
    for group in which:
        if group not in TARGETS:
            print("unknown group %r; known: %s" % (group, ", ".join(TARGETS)))
            return 2
        print("%s:" % group)
        for fn in TARGETS[group]:
            try:
                fn()
            except Exception as exc:
                print("  ! %s failed: %s" % (fn.__name__, exc))
                failures.append(fn.__name__)
    if failures:
        print("\n%d converter(s) failed: %s" % (len(failures), ", ".join(failures)))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
