#pragma once

// Measured spectra loaded from the spectra/ directory at start-up.
//
// The renderer already accepts a spectrum inline in the scene file, but writing
// 31 wavelength/value pairs by hand is only tolerable for a handful of
// materials. This is the same data by reference: a scene says
//
//     "DiffuseSpectrum": {"_ref": "material:foliage"}
//
// and the curve comes from a published measurement instead of a number someone
// typed. See spectra/README.md for the file format and spectra/LICENSES.md for
// where each set came from.
//
// The library is a process-wide singleton filled once before rendering starts
// and only read afterwards, which keeps it safe to touch from the tile threads.

#include <map>
#include <string>
#include <vector>

#include "Spectrum.hpp"

enum class SpectrumKind { kLight, kMaterial, kSensor };

struct SpectrumRecord {
  SpectrumKind kind = SpectrumKind::kMaterial;
  std::string name;    // lookup key, e.g. "d65"
  std::string label;   // original spelling, for messages
  std::string source;  // provenance, carried from the .spd header
  std::string origin;  // file the record was read from

  // Single-curve records (lights, materials).
  Spectrum value = Spectrum::Zero();

  // Sensor records carry one curve per channel. These are the complete optical
  // response -- quantum efficiency times colour filter -- so a caller binding
  // them to the CFA must leave quantum efficiency at unity or it counts the
  // same physics twice.
  bool multichannel = false;
  Spectrum channels[3] = {Spectrum::Zero(), Spectrum::Zero(), Spectrum::Zero()};
};

class SpectrumLibrary {
 public:
  static SpectrumLibrary &Instance();

  // Loads every .spd under `dir`. Safe to call more than once; later records
  // with the same key replace earlier ones. Returns the number added.
  int LoadDirectory(const std::string &dir);

  // Resolves the library directory and loads it, trying in order:
  //   1. `scene_declared` (the scene file's "SpectralLibrary" key)
  //   2. $RAYTRACER_SPECTRA_DIR
  //   3. "spectra" beside the scene file
  //   4. "spectra" in the current directory
  // Missing directories are not an error -- a scene that never uses _ref does
  // not need the library at all. A bad _ref is caught at resolve time instead.
  void LoadDefault(const std::string &scene_declared,
                   const std::string &scene_directory);

  // `ref` is "kind:name", e.g. "light:d65". Returns nullptr if absent.
  const SpectrumRecord *Find(const std::string &ref) const;

  // Same, but throws with a message listing near-misses. Used by the scene
  // loader, where a silent fallback would corrupt a colour study invisibly.
  const SpectrumRecord &Require(const std::string &ref) const;

  int size() const { return static_cast<int>(records_.size()); }
  const std::vector<std::string> &directories() const { return directories_; }

 private:
  SpectrumLibrary() = default;

  int LoadFile(const std::string &path);

  std::map<std::string, SpectrumRecord> records_;  // keyed by "kind:name"
  std::vector<std::string> directories_;
};

// "light" | "material" | "sensor" -> enum. Returns false for anything else.
bool SpectrumKindFromString(const std::string &text, SpectrumKind &out);
const char *SpectrumKindToString(SpectrumKind kind);
