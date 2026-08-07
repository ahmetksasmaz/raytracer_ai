# Provenance and licences

Every `.spd` file here is derived from published data. The `source:` line on each
record carries its attribution, so provenance survives being copied out of this
directory. Terms differ per source — check this table before redistributing.

| Files | Upstream | Licence |
|---|---|---|
| `lights/cie_illuminants.spd`, `lights/measured_lamps.spd`, `materials/colorchecker.spd`, `sensors/dslr_permissive.spd` | [colour-science](https://github.com/colour-science/colour) | BSD-3-Clause |
| `materials/artist_paints.spd` | [painting_tools](https://github.com/rubenwiersma/painting_tools) (Okumura/RIT measurement archive) | MIT |
| `sensors/camspec.spd` | [Jiang et al. 2013](https://zenodo.org/records/3245883) | **CC BY-NC-SA 4.0** |
| `materials/forest.spd`, `materials/natural.spd`, `materials/munsell_matt.spd` | [UEF Computational Spectral Imaging](https://sites.uef.fi/spectral/databases-software/) | **Academic use only** |

## The two restricted sets

**`sensors/camspec.spd` — CC BY-NC-SA 4.0.** Non-commercial use only, attribution
required, and derivatives must be shared alike. Fine for coursework and research;
not for a commercial product. `sensors/dslr_permissive.spd` covers two cameras
under BSD-3 if you need an unrestricted alternative.

**The UEF sets — academic use only.** The group does not grant commercial
licences. Academic use should cite the group and the papers listed with each
dataset.

## Citations

Jiang, J., Liu, D., Gu, J., & Süsstrunk, S. (2013). *What is the space of spectral
sensitivity functions for digital color cameras?* IEEE Workshop on Applications of
Computer Vision (WACV), 168–179.

Computational Spectral Imaging Research Group, University of Eastern Finland —
Munsell matt, natural colours, and forest reflectance databases.

Okumura, Y. (2005). *Developing Spectral and Colorimetric Databases of Artist Paint
Materials.* MSc thesis, Rochester Institute of Technology — redistributed via
`painting_tools`.

Colour Developers. *Colour: a Python package implementing a comprehensive number of
colour theory transformations and algorithms.* — CIE 15:2018 illuminants, NIST and
Philips lamp measurements, BabelColor ColorChecker averages, DSLR sensitivities.

## What is not measured

Two things in the renderer remain synthetic, and results that depend on them are
about the model rather than about any real device:

- the default Bayer CFA (Gaussians in `include/Sensor/SensorModel.hpp`) when no
  `sensor:` reference is given, and
- the compiled-in ColorChecker table in `include/SpectralData.hpp`, which is
  published sRGB uplifted to a spectrum. `materials/colorchecker.spd` is the
  measured replacement — prefer it.
