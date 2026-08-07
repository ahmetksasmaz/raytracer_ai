# Spectral library

Measured spectra for real light sources, real materials and real camera sensors,
in a single plain-text format the renderer loads at start-up.

Regenerate everything from the upstream sources with:

```bash
python3 tools/fetch_spectra.py
```

The `.spd` files are committed, so this is only needed to refresh them. See
[LICENSES.md](LICENSES.md) for provenance and terms — two of the sets are not
permissively licensed.

## Contents

| File | Records | What |
|---|---:|---|
| `lights/cie_illuminants.spd` | 59 | CIE A/B/C, D50–D75, E, FL1–FL12, FL3.x, HP1–HP5, the CIE LED illuminants, ISO 7589 |
| `lights/measured_lamps.spd` | 56 | Measured real lamps: tungsten, fluorescent, HID, phosphor and multi-die LEDs |
| `materials/colorchecker.spd` | 24 | ColorChecker patches, **measured** reflectance (not uplifted sRGB) |
| `materials/artist_paints.spd` | 485 | Named artist paints across nine lines — six of them oils |
| `materials/natural.spd` | 219 | Flowers, leaves and other plants |
| `materials/forest.spd` | 3 | Scots pine, Norway spruce, birch foliage |
| `materials/munsell_matt.spd` | 1269 | Matt Munsell chips, for bulk statistics |
| `sensors/camspec.spd` | 28 | Real camera spectral sensitivities — DSLRs, a medium-format back, two industrial cameras, a phone |
| `sensors/dslr_permissive.spd` | 2 | Same, under a permissive licence |

## Format

A `.spd` file holds one or more records. Blank lines and `#` comments are
ignored; keys are `key: value`, and `data:` is followed by indented rows.

```
type: light                       # light | material | sensor
name: d65                         # lookup key: lowercase, _-separated
label: D65                        # original name, for humans
source: CIE 15:2018 via ...       # provenance, carried into every record
data:
  300 0.0341
  305 1.6643
```

Sensors carry three columns and declare them:

```
type: sensor
name: nikon_d700
channels: R G B
data:
  400 0.0017775 0.0014335 0.0059157
```

Rules that matter:

- **Wavelengths are nanometres, ascending.** Ranges vary by source (300–830 for
  the CIE illuminants, 400–720 for camspec); the loader resamples onto the
  render's band grid and clamps outside the measured range.
- **Names must be unique within a type**, not across types — `type:name` is the
  lookup key, so a light and a material may both be called `a`.
- **Lights are relative SPDs.** They are not absolute radiance; set the level
  with `_scale` in the scene file.
- **Materials are reflectance in 0–1.**
- **Sensor curves are the complete optical response** — quantum efficiency times
  colour filter times everything else in the path, each channel normalised to a
  peak of 1. The renderer binds them to the CFA and leaves quantum efficiency at
  unity, because splitting them would double-count.

## Using them in a scene

Any spectral key accepts `_ref`, which takes precedence over `_data`,
`_illuminant` and the RGB fallback:

```json
"DiffuseSpectrum": {"_ref": "material:daler_rowney_georgian_oil_382_viridian_hue_tr_p_g_7"},
"RadianceSpectrum": {"_ref": "light:incandescent", "_scale": "8"},
"Sensor": {"_ref": "sensor:nikon_d700", "ExposureTime": "1e-3"}
```

An unknown reference is a hard error, on the same reasoning as an unknown
illuminant name: quietly rendering under the wrong spectrum would corrupt a
white-balance study invisibly.

By default the renderer looks for `spectra/` next to the scene file, then in the
current directory, then beside the binary. Override it per scene with
`"SpectralLibrary": "some/other/dir"` at the scene root, or globally with the
`RAYTRACER_SPECTRA_DIR` environment variable.

## Caveats

- The Munsell chips are unlabelled in the upstream archive, so they are numbered
  in file order. They are for bulk work, not for looking up a named chip.
- `natural.spd` is derived from raw 12-bit AOTF counts divided by 4096, per the
  dataset's own README. Treat it as relative reflectance.
- Artist paint names carry the manufacturer's colour index in brackets — `P.G.7`
  is phthalo green, `P.B.29` ultramarine. That, not the marketing name,
  identifies the pigment: two brands' "viridian hue" need not share chemistry,
  and none of the four viridians here is the historic Cr₂O₃·2H₂O.
- Every light in `cie_illuminants.spd` is a *definition*; the ones in
  `measured_lamps.spd` are *measurements* of particular physical lamps. Prefer
  the latter when the question is what a real room looks like.
