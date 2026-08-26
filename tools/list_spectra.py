#!/usr/bin/env python3
"""Writes the three listing files enumerating the measured spectral library.

    spectra/ILLUMINANTS.txt   every `light:`    reference
    spectra/MATERIALS.txt     every `material:` reference
    spectra/SENSORS.txt       every `sensor:`   reference

These are generated, not hand-maintained -- rerun after `fetch_spectra.py`.

The names are taken verbatim from each record's `name:` line, because that is
exactly what the C++ loader keys on: SpectrumLibrary::LoadFile assigns
`record.name = value` with no normalisation, and Find() is a case-sensitive
std::map lookup. So `light:d65` resolves and `light:D65` does not, and a
listing that prettified the names would be actively misleading.

The slugging that produced those names happened once, upstream, in
fetch_spectra.py: runs of non-alphanumerics collapsed to `_`, lowercased.
"""

import os
import re

ROOT = os.path.dirname(os.path.abspath(os.path.dirname(__file__)))
SPECTRA = os.path.join(ROOT, "spectra")

# Per-dataset licence terms, from spectra/LICENSES.md. Carried into the
# listings because they are not uniform and they constrain what the data may be
# used for -- which is not something anyone should have to go looking for.
LICENCES = {
    "cie_illuminants.spd": "BSD-3-Clause (free to redistribute)",
    "measured_lamps.spd": "BSD-3-Clause (free to redistribute)",
    "colorchecker.spd": "BSD-3-Clause (free to redistribute)",
    "artist_paints.spd": "MIT (free to redistribute)",
    "forest.spd": "ACADEMIC USE ONLY -- University of Eastern Finland",
    "natural.spd": "ACADEMIC USE ONLY -- University of Eastern Finland",
    "munsell_matt.spd": "ACADEMIC USE ONLY -- University of Eastern Finland",
    "camspec.spd": "CC BY-NC-SA 4.0 -- NON-COMMERCIAL, attribution, share-alike",
    "dslr_permissive.spd": "BSD-3-Clause (free to redistribute)",
}

HEADERS = {
    "light": """\
Illuminants with a measured or published spectral power distribution.

Reference one from a scene with the `_ref` key, e.g.

    "RadianceSpectrum": {"_ref": "light:d65", "_scale": "42"}

or from sensor_ccm with --illuminant light:<name>.

Names are case-sensitive and must be spelled exactly as listed.

NORMALISATION. Every light here is rescaled on load so it reads 1.0 at 560 nm
(NormalizeLight, src/SpectrumLibrary.cpp). Published CIE tables sit at 100 at
560 nm, so without this `light:d65` and the built-in `D65` would differ by a
factor of 100 for the same nominal spectrum. The exception is narrowband
sources -- where 560 nm carries under 1% of the peak, the curve is normalised
to its peak instead, because dividing by a near-zero value would explode. So
`_scale` does NOT mean quite the same thing for `light:lps` as for `light:d65`.

BUILT-INS. `D65`, `A` and `E` also exist compiled in, reached by the
`_illuminant` key (uppercase) rather than `_ref`, and work with no spectra/
directory present. They agree with their library namesakes to within the
resampling.
""",
    "material": """\
Materials with a measured spectral reflectance.

Reference one from a scene with the `_ref` key, e.g.

    "DiffuseSpectrum": {"_ref": "material:dark_skin"}

Names are case-sensitive and must be spelled exactly as listed.

Reflectances are absolute and are NOT renormalised on load, unlike the lights.

COLORCHECKER. The 24 `colorchecker.spd` names deliberately collide with the
compiled-in table in include/SpectralData.hpp. That table is published sRGB
uplifted to a smooth spectrum -- one metamer of infinitely many -- and is a
placeholder for wiring up a pipeline, not for drawing conclusions. The entries
below are the measured replacements. They are not interchangeable.

LICENSING. 1491 of the 2000 materials (munsell_matt, natural, forest) are
academic use only. Check the group header before using any of them.
""",
    "sensor": """\
Camera spectral sensitivities.

Reference one from a sensor config with the top-level `_ref` key, e.g.

    {"_ref": "sensor:nikon_d700", "_pattern": "RGGB", ...}

Names are case-sensitive and must be spelled exactly as listed.

THESE ARE THE WHOLE OPTICAL RESPONSE -- quantum efficiency times colour filter
times everything else in the path, each channel normalised to a peak of 1. A
`_ref` therefore fills the three CFA curves AND forces quantum efficiency to 1;
splitting them would count the sensor's own efficiency twice. Each record has
three channels, and using one as a plain single-curve spectrum is a hard error.

LICENSING. 28 of the 30 (camspec) are CC BY-NC-SA 4.0: non-commercial only,
attribution required, share-alike. The 2 `_npl` entries are BSD-3-Clause and
are the unrestricted alternative, but they cover only two cameras.
""",
}

KIND_FILE = {
    "light": "ILLUMINANTS.txt",
    "material": "MATERIALS.txt",
    "sensor": "SENSORS.txt",
}


def parse(path):
    """Yields (kind, name, label, source, channels) per record.

    Mirrors SpectrumLibrary::LoadFile: `type:` opens a record, `key: value`
    splits on the FIRST colon only, `#` lines are comments. A record with no
    name is dropped, as the loader drops it.
    """
    records, current = [], None
    with open(path) as handle:
        for line in handle:
            line = line.rstrip("\n")
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            if ":" not in stripped:
                continue
            key, _, value = stripped.partition(":")
            key, value = key.strip(), value.strip()
            if key == "type":
                if current and current.get("name"):
                    records.append(current)
                current = {"type": value, "channels": 1}
            elif current is None:
                continue
            elif key in ("name", "label", "source"):
                current[key] = value
            elif key == "channels":
                current["channels"] = len(value.split())
    if current and current.get("name"):
        records.append(current)
    return records


def wavelength_range(path):
    """First and last sample wavelength, and the step, for the header line."""
    values = []
    with open(path) as handle:
        for line in handle:
            stripped = line.strip()
            if stripped[:1].isdigit():
                values.append(float(stripped.split()[0]))
            if len(values) > 3 and values[-1] < values[-2]:
                break  # wrapped into the next record
    if len(values) < 2:
        return "unknown"
    step = values[1] - values[0]
    return "%.0f-%.0f nm at %.0f nm" % (min(values), max(values), step)


def main():
    by_kind = {}
    for directory in sorted(os.listdir(SPECTRA)):
        full = os.path.join(SPECTRA, directory)
        if not os.path.isdir(full):
            continue
        for filename in sorted(os.listdir(full)):
            if not filename.endswith(".spd"):
                continue
            path = os.path.join(full, filename)
            records = parse(path)
            if not records:
                continue
            kind = records[0]["type"]
            by_kind.setdefault(kind, []).append((filename, path, records))

    for kind, groups in sorted(by_kind.items()):
        total = sum(len(r) for _, _, r in groups)
        out = os.path.join(SPECTRA, KIND_FILE[kind])
        with open(out, "w") as handle:
            handle.write("%s -- %d entries\n" % (KIND_FILE[kind][:-4], total))
            handle.write("=" * 78 + "\n\n")
            handle.write(HEADERS[kind])
            handle.write("\nGenerated by tools/list_spectra.py -- do not edit by hand.\n")

            for filename, path, records in groups:
                handle.write("\n" + "-" * 78 + "\n")
                handle.write("%s -- %d entries\n" % (filename, len(records)))
                handle.write("  source:     %s\n" % records[0].get("source", "?"))
                handle.write("  licence:    %s\n" % LICENCES.get(filename, "see spectra/LICENSES.md"))
                handle.write("  wavelength: %s\n" % wavelength_range(path))
                handle.write("-" * 78 + "\n\n")
                width = max(len(r["name"]) for r in records)
                for record in sorted(records, key=lambda r: r["name"]):
                    label = record.get("label", "")
                    if label and label != record["name"]:
                        handle.write("  %-*s  %s\n" % (width, record["name"], label))
                    else:
                        handle.write("  %s\n" % record["name"])
        print("%-18s %5d entries  -> %s" % (kind, total, os.path.relpath(out, ROOT)))


if __name__ == "__main__":
    main()
