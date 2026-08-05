# CLAUDE.md — raytracer_ai

METU CENG795 Advanced Ray Tracing. Single-binary C++17 CPU ray/path tracer. No CI, no test framework — correctness is covered by a self-validating scene suite under `tests/` (see below).

## Build & run

```bash
make release        # -> ./raytracer   (g++, -O3 -march=native -flto, warnings off)
make test           # build + run the correctness suite (~25 s)
```
`make debug` -> `raytracer_debug` (-O0 -g), `make profile`, `make imgdiff`, `make clean`.

`RELEASE_FLAGS` carries **`-fno-finite-math-only`** and it must stay. Plain `-ffast-math` implies `-ffinite-math-only`, under which the compiler assumes NaN/Inf cannot occur and folds every `std::isfinite` check to `true` — verified on this toolchain. The renderer relies on those guards.
Run: `./raytracer <scene.json|scene.xml>` — relative asset paths (ply/images) resolve from **cwd**, so cd into the scene's dir first (see `renderall.sh`).
Build is whole-program every time (`extern/*.cpp src/*.cpp src/*/*.cpp`); ~30-60s. Any new `.cpp` under `src/<Dir>/` is picked up automatically; new include dirs must be added to `INCLUDES` in the Makefile.

## Layout

```
extern/     parser.{h,cpp} (scene loader), json.hpp, tinyxml2, ply, stb_image(_write), tinyexr  — vendored, don't touch except parser
include/    headers, flat-ish; Makefile adds each subdir to -I so includes are by basename ("Scene.hpp", "BaseBRDF.hpp")
src/        RayTracer.cpp (main), Scene.cpp (raw->runtime build), plus one .cpp per algorithm/object
```
Subsystems (each `include/X/` + `src/X/`): `Objects/`, `Materials/`, `BRDFs/`, `LightSources/`, `Textures/`, `ToneMappingAlgorithms/`, `TracingAlgorithms/`, `SchedulingAlgorithms/`.

`FP_PRECISION` (= `double`, in `extern/parser.h`) is the scalar type everywhere. `parser::Vec2f/Vec3f/Vec4f/Vec5f/Vec2i/Mat4x4f` are the vector types; all math operators + `parse_transformation`, samplers, `FastRandom`, `BuildOrthonormalBasis` live in `include/Helper.hpp`. `using namespace parser;` is standard in headers here.

## Pipeline

`main` -> `Scene(file)` -> `LoadScene` (parse to `RawScene`, then build runtime objects) -> `PreprocessScene` (`object->Preprocess()`, `bvh_.BuildBVH`) -> `Render()`: per camera → scheduling → resolve → tone mapping → export.

**Film is an accumulation buffer.** `BaseCamera::SplatSample` scatters each sample into a 3×3 neighbourhood with its Gaussian reconstruction weight (σ=0.5 px), accumulating into `width*height*(kSpectralBands+1)` atomics — band sums plus summed weight. `ResolveAccumulator()` normalises once at the end, producing both `GetSpectralImage()` (sensor-independent) and `image_data_` (linear sRGB). There is **no separate filtering pass**; reconstruction happens during tracing.

Film memory is independent of sample count: measured 5.0 MB at 36 spp and 5.8 MB at 360 spp. The old per-sample store was `width*height*spp*40 B`, which with spectral radiance would have been ~200 GB at 800×800×1000 spp. Splats cross tile boundaries so accumulation is atomic (CAS loop; C++17 has no `atomic<double>::fetch_add`) — measured cost is nil, in fact slightly faster than the old gather filter.

Hard-coded defaults in `Scene::Scene` (src/Scene.cpp:7-24), **not** settable from the scene file:
- scheduler `ThreadQueueSchedulingAlgorithm` (32×32 tiles, `hardware_concurrency` threads, prints `Progress: N%` every 1s)
- ray tracer `RecursiveBRDFRayTracingAlgorithm`, path tracer `RecursiveBRDFPathTracingAlgorithm`
- area-light sampling `uniform_random_2d`
- camera pixel/aperture sampling Hammersley, time sampling jittered, circular aperture (`Scene.cpp:176-181`)

Per pixel the camera emits `NumSamples` rays; `path_tracing_enabled_` picks path tracer vs ray tracer. `DefaultRayTracingAlgorithm` / `RecursiveRayTracingAlgorithm` exist but are unwired.

Geometry: everything goes into `objects_` and the BVH **except planes**, which live in `plane_objects_`. `light_objects_` is a parallel list of emissive objects (LightSphere/LightMesh) used for NEE/MIS. `objects_` order is: spheres, light spheres, triangles, meshes, light meshes, mesh instances (mesh-instance resolution in `Scene.cpp` relies on that ordering).

**Always trace rays through `Scene::IntersectScene`**, never `bvh_.Intersect` directly. It covers the BVH *and* the plane list; going straight to the BVH is how planes ended up visible to primary rays but invisible to shadow and secondary rays. Object `Intersect` takes a `const Ray&` and must not mutate it.

Export: `.exr` → EXR of raw HDR data plus one LDR file per `Tonemap`. LDR output applies the first declared `Tonemap`, or clamps raw radiance to 0-255 if none is declared.

## Scene file format (the important part)

Two loaders, extension-dispatched (`Scene::LoadScene`). **JSON is canonical** — use it. The XML path (`loadFromXml`) is stale: it lacks Plane, MeshInstance, LightMesh, LightSphere, BRDFs, DirectionalLight, SpotLight, SphericalDirectionalLight, Tonemap, Renderer. Never generate XML scenes.

JSON is a mechanical XML→JSON transliteration, so:
- **Every value is a string**, including numbers: `"Radius": "1.5"`. Vectors are space-separated in one string: `"0 1 -2"`.
- XML attributes become `_`-prefixed keys: `_id`, `_type`, `_data`, `_plyFile`, `_baseMeshId`, `_BRDF`, `_degamma`, `_normalized`, `_kdfresnel`, `_handedness`, `_resetTransform`, `_vertexOffset`, `_textureOffset`.
- **All IDs are 1-based indices** into their list (`Material`, `Center`, `Point`, `Indices`, `Textures`, `ImageId`, `_BRDF`, transformation ids). BRDFs are sorted by `_id` after parse.
- A repeated element is an **array**; a single one is a **bare object**. The parser handles both (try-single, catch-array), so a single-element array is safe and preferred for generated scenes.
- Missing optional keys fall back to defaults — omit rather than guess.
- Root is `{"Scene": {...}}`.

### Skeleton

```json
{"Scene": {
  "BackgroundColor": "0 0 0",              // 0-255 ints, LDR pixel value
  "ShadowRayEpsilon": "0.001",             // default 1e-5
  "IntersectionTestEpsilon": "0.001",      // parsed, currently unused downstream
  "Cameras": {"Camera": [ ... ]},
  "Lights": {"AmbientLight": "25 25 25", "PointLight": [...], "AreaLight": [...],
             "DirectionalLight": [...], "SpotLight": [...], "SphericalDirectionalLight": {...}},
  "BRDFs": {"ModifiedBlinnPhong": [...], "TorranceSparrow": [...], ...},
  "Materials": {"Material": [ ... ]},
  "Textures": {"Images": {"Image": [{"_id":"1","_data":"tex.png"}]}, "TextureMap": [...]},
  "Transformations": {"Translation": [...], "Scaling": [...], "Rotation": [...], "Composite": [...]},
  "VertexData": {"_data": "x y z\nx y z\n..."},
  "TexCoordData": {"_data": "u v\nu v\n..."},
  "Objects": {"Mesh": [...], "MeshInstance": [...], "Triangle": [...], "Sphere": [...],
              "Plane": [...], "LightMesh": [...], "LightSphere": [...]}
}}
```

### Camera
Required: `Position`, `Up`, `NearDistance`, `ImageResolution` ("W H"), `ImageName`.
Two modes: `"_type": "lookAt"` + `GazePoint` + `FovY` (vertical, degrees), or default + `Gaze` + `NearPlane` ("l r b t").
Optional: `NumSamples` (rays/pixel, default 1), `Transformations`, `_handedness` ("left"), `FocusDistance` + `ApertureSize` (DOF; both > 0 to enable), `MaxRecursionDepth` (default 1), `MinRecursionDepth` (default 0, = RR start depth), `SampleMaxVal` (per-sample firefly clamp), `Tonemap`.
Path tracing: `"Renderer": "PathTracing"` plus `"RendererParams"` — a single space-separated string, substring-matched for `ImportanceSampling`, `NextEventEstimation`, `MIS_BALANCE`, `RussianRoulette` — and optional `"SplittingFactor"` (extra bounce samples at depth 1 only). Without `Renderer` the BRDF ray tracer runs.
`Tonemap`: `{"TMO": "Photographic"|"Filmic"|"ACES", "TMOOptions": "<key> <burn>", "Saturation": "1.0", "Gamma": "2.2", "Extension": ".png"}`. With an `.exr` output you get one LDR file per tone mapping, named `<base><Extension>`. With an LDR output the **first** tone mapping is applied to it (a warning is printed if more than one is declared); with no `Tonemap` at all, LDR output clamps raw radiance as before.

Recursion caps: path tracer hard-stops at depth 32; with `RussianRoulette` on, `MaxRecursionDepth` is bypassed and RR (survival = min(max throughput, 0.95)) terminates paths past `MinRecursionDepth`.

### Lights
- `PointLight`: `Position`, `Intensity` (falls off 1/r²), optional `Transformations`.
- `AreaLight`: `Position`, `Normal`, `Size` (square edge), `Radiance`, optional `Transformations`.
- `DirectionalLight`: `Direction` (points *from* light), `Radiance`.
- `SpotLight`: `Position`, `Direction`, `Intensity`, `CoverageAngle`, `FalloffAngle` (full angles, degrees).
- `SphericalDirectionalLight`: `_type` `"latlong"`|`"probe"`, `ImageId`, optional `Sampler` `"cosine"` (default) | `"uniform"`. Also serves as background for escaping rays.
- Emissive geometry — `LightSphere` / `LightMesh` under `Objects` with a `Radiance` — is what NEE/MIS sample. Analytic lights above are evaluated deterministically every bounce and are *not* in `light_objects_`. Cornell-box style scenes use `LightMesh`/`LightSphere`.

### Materials
`_type`: omitted/other = diffuse+specular (`BaseMaterial`), `"mirror"`, `"conductor"`, `"dielectric"`.
Always present: `AmbientReflectance`, `DiffuseReflectance`, `SpecularReflectance`.
Optional: `MirrorReflectance` (mirror/conductor/dielectric), `RefractionIndex`, `AbsorptionIndex` (conductor k), `AbsorptionCoefficient` (dielectric, Beer-Lambert), `PhongExponent`, `Roughness` (perturbs the reflected/refracted normal), `_degamma` ("true" raises the three reflectances to 2.2), `_BRDF` (1-based BRDF id; absent → `OriginalBlinnPhong(PhongExponent)`).

### BRDFs
Keys `OriginalPhong`, `ModifiedPhong`, `OriginalBlinnPhong`, `ModifiedBlinnPhong`, `TorranceSparrow`; each entry `{"_id":"1","Exponent":"100"}` plus `_normalized` ("true", the Modified/Original variants) or `_kdfresnel` (TorranceSparrow).

### Textures
`Images.Image[]` → `{"_id", "_data": <path>}`, loaded in order (id = 1-based position).
`TextureMap[]`: `_type` `"image"|"perlin"|"checkerboard"`; `DecalMode` `replace_kd|blend_kd|replace_ks|replace_background|replace_normal|bump_normal|replace_all`; image: `ImageId`, `Interpolation` (`nearest`|`bilinear`|`trilinear`), `Normalizer` (255), `BumpFactor` (1), `_degamma` ("true" decodes sRGB to linear; off by default); perlin: `NoiseConversion` (`absval`|`linear`), `NoiseScale`, `NumOctaves`; checkerboard: `Scale`, `Offset`, `BlackColor`, `WhiteColor`.
An object's `Textures` is a space-separated list of 1-based texture-map ids. A `replace_background` image map also becomes the scene background.

### Transformations
`Translation`/`Scaling` `_data` = "x y z", `Rotation` `_data` = "angle x y z" (degrees; each nonzero axis applies a separate axis rotation, composed X→Y→Z — not a true arbitrary-axis rotation), `Composite` `_data` = 16 row-major floats.
An object's `Transformations` string is space-separated refs like `"t1 s2 r1 c3"`, applied **left to right as left-multiplications** (`M = last * ... * first * I`). Negative scale components are tracked in `RawScalingFlip` for normal correction.

### Objects (all take `_id`, `Material`, optional `Transformations`, `Textures`, `MotionBlur` "dx dy dz")
- `Sphere` / `LightSphere`: `Center` (vertex id), `Radius`; LightSphere adds `Radiance`.
- `Triangle`: `Indices` "v0 v1 v2" (1-based into `VertexData`; tex coords come from the same indices in `TexCoordData`).
- `Mesh` / `LightMesh`: `Faces` either `{"_data": "v0 v1 v2\n..."}` or `{"_plyFile": "bunny.ply"}`, plus optional `_vertexOffset` / `_textureOffset`. LightMesh adds `Radiance`.
- `MeshInstance`: `_baseMeshId`, optional `_resetTransform` ("true" = ignore base transform), `Material`, `Textures`.
- `Plane`: `Point` (vertex id), `Normal`. Infinite; outside the BVH.

## Spectral core

Radiance, reflectance and emission are `Spectrum` (31 bands, 400-700nm @ 10nm, `include/Spectrum.hpp`). Geometry stays `Vec3f`. Image textures also stay `Vec3f` — `GetColorAt` doubles as the normal-map accessor — and are uplifted where they modulate a reflectance.

`kSpectralBands` is a real knob: all reference data lives in `include/SpectralData.hpp` at its own master resolution (380-780nm @ 10nm) and is resampled at start-up. Verified at 16, 31 and 61 bands.

Two invariants keep neutral scenes bit-stable across the RGB→spectral conversion, and both are asserted by `./spectraltest`:
- Smits' white basis is replaced with an exactly flat curve (also the more correct choice — a neutral grey *is* a wavelength-flat reflectance), so neutral RGB round-trips through `UpliftRGB` exactly.
- `SpectrumToLinearSRGB` divides by a flat spectrum's RGB — a von Kries adaptation from equal-energy white E to the sRGB white — pinning flat unit spectrum to exactly (1,1,1).

**Scene syntax.** Any radiometric quantity accepts a spectral override, which takes precedence over the RGB key; without one the RGB is uplifted, so old scenes are unaffected:
```json
"RadianceSpectrum": "D65"
"RadianceSpectrum": {"_illuminant": "D65", "_scale": "5"}
"DiffuseSpectrum":  {"_data": "400 0.04 550 0.9 700 0.05"}
```
Keys: `IntensitySpectrum` (point/spot), `RadianceSpectrum` (area/directional/LightMesh/LightSphere), `DiffuseSpectrum` / `SpecularSpectrum` (materials). Illuminants: `D65`, `A`, `E`. An unknown name is a **hard error** — silently rendering under the wrong illuminant would corrupt a white-balance study invisibly.

The ColorChecker table in `SpectralData.hpp` is **published sRGB, not measured spectra**. It is fine for wiring up a pipeline but not for research conclusions: an uplifted spectrum is one of infinitely many metamers, and distinguishing metamers is the point of a spectral study. Supply measured reflectances via `DiffuseSpectrum` for real work.

## Camera sensor simulation

Declared per camera under `"Sensor"`. Absent means no sensor simulation and the conventional image path only.

```json
"Sensor": {
  "_pattern": "RGGB",              // or BGGR / GRBG / GBRG; unknown = hard error
  "ExposureTime": "1e-4", "PixelPitch": "3.45e-6", "FNumber": "2.8",
  "FullWell": "60000", "Gain": "16.0", "BitDepth": "12",
  "ReadNoise": "2.0", "DarkCurrent": "5.0",
  "NoiseSources": "Shot Read Dark",           // "None" for a noise-free reference
  "QuantumEfficiency": {"_data": "400 0.3 550 0.6 700 0.4"},
  "FilterRed": {"_data": "..."}, "FilterGreen": {...}, "FilterBlue": {...}
}
```

Chain: spectral radiance → photons (`L·A·Ω·t·λ/hc`, Ω = π/4N²) → electrons (QE × CFA for the pixel's Bayer channel, integrated over λ) → Poisson shot + Poisson dark + Gaussian read → full-well clamp → gain → quantise. The λ/hc factor is why this needs the spectral core.

Writes four products alongside `<base>.exr`:
1. `<base>_spectral.exr` — **sensor-independent** N-band cube at the sensor plane, channels named `0400nm`…. One render replays through any sensor.
2. `<base>_raw.pgm` + `_raw.exr` — RAW Bayer mosaic in sensorRGB digital numbers (16-bit PGM because stb only writes 8-bit PNG).
3. `<base>_demosaiced.exr` — bilinear, still sensor space.
4. `<base>_sensor_to_xyz.json` — least-squares 3×3 plus its residual. The residual is part of the result: a sensor failing the Luther condition cannot be corrected exactly by any 3×3.

**Default CFA filters are Gaussians, not measured curves**, and the ColorChecker training set is uplifted sRGB. Both are fine for wiring up a pipeline and wrong for research conclusions — supply measured data via the spectral syntax.

Two testing traps worth remembering: whole-image variance on a mosaic measures the *mosaic pattern*, not noise (compute it within a Bayer parity class), and an over-exposed sensor pegs every parity at full well so CFA differences vanish — both cost real debugging time here.

## Path tracer invariants

These are load-bearing; breaking one produces a plausible-looking but wrong image rather than a crash.

- **One pdf formula per strategy.** `hemisphere_pdf()` in `Helper.hpp` is the only definition of the hemisphere sampling density, and `ObjectLightSource::PdfSolidAngle()` the only definition of the light-sampling density. `Sample()` returns its pdf *by calling* `PdfSolidAngle`. MIS weights must combine two pdfs describing the **same direction** — drawing a fresh random sample to obtain the second one is the classic way to get silently wrong weights.
- **Emission is gated, not unconditional.** `PathState::prev_specular` and `prev_bsdf_pdf` travel down the recursion so an emitter hit knows whether NEE already accounted for it. Without the gate, NEE double-counts direct lighting (~2× too bright).
- **Russian roulette scales survivors by `1/p`.** Both the returned radiance and the throughput handed to children.
- **The 1/N light-selection probability belongs in `LightSamplingPdf`**, so NEE and MIS cannot disagree about it.
- **Analytic lights are accumulated separately from the splitting loop** — only the per-sample terms are divided by `sample_count`.
- **Default BRDF depends on the renderer.** With `Renderer=PathTracing`, a material with no `_BRDF` gets a *normalized* `ModifiedBlinnPhong`; the ray tracer keeps `OriginalBlinnPhong`. `Original*` omits the `1/π` factor, which makes albedo-1 diffuse reflect π× the incident energy per bounce under a path tracer.

## Tests

```bash
./tests/run_tests.sh            # all checks
./tests/run_tests.sh furnace    # only names matching "furnace"
```

`tools/imgdiff.cpp` (built as `./imgdiff`) reads EXR *and* PNG via the vendored tinyexr/stb — no external deps. Modes: `--stats`, `--mean`, `--argmax` (locate a firefly), `--compare a b --tol T`, `--expect-constant f L --tol T [--max-dev D]`, `--expect-ratio a b R`, `--expect-nonnegative`, `--expect-finite`, `--expect-below f V`.

Every check is **self-validating** — it asserts analytic ground truth or an invariance, never a blessed reference image. Scenes in `tests/scenes/` are self-contained (inline geometry, no PLY, no image textures); renders land in the gitignored `tests/out/`.

The load-bearing ones: `furnace*` (a closed emissive box with an albedo-1 sphere must read exactly the emitted radiance — catches any energy gain/loss); `cornell_{brute,nee,mis}` (multi-bounce equivalence — **a furnace cannot detect MIS weighting errors**, since both strategies share one expectation and the weights sum to 1, so bias cancels regardless); `beer_*` (transmission ratio vs analytic `exp(-σd)`); `sphere_{norot,rot}` (rotating a sphere about its own centre must be a no-op).

When adding a check, prefer an invariance (two configurations that must agree) or a closed-form expectation over a threshold someone has to eyeball.

## Working here

- Match the surrounding style: `Class::Method` in its own `.cpp`, members `trailing_underscore_`, `kCamelCase` enum values, `std::shared_ptr` for scene entities, comments sparse and only for pipeline STEP markers.
- Adding a scene-file feature means touching all of: `RawX` struct in `extern/parser.h`, `loadFromJSON` in `extern/parser.cpp`, and the corresponding build loop in `src/Scene.cpp`.
- Renderer state is read-only during rendering (tiles are the only mutable output) — anything added to the trace path must be thread-safe; use `FastRandom()` (thread-local RNG), never `rand()`.
- Renders are slow. When verifying a change, generate/patch a small scene (low `ImageResolution`, `NumSamples` ~4-16, low depth) rather than running `renderall.sh`.
- `.gitignore` excludes `build/*`, `*.png`, `*.xml`, `*.ply`, `hw*/*` — scene assets and outputs are untracked, so referenced input files may not exist in a clean checkout. `renderall.sh` expects `build/hw6/{brdf,directLighting,pathTracing}/inputs/`.
