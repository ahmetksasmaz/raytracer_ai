#include "SpectrumLibrary.hpp"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace {

std::string Trim(const std::string &s) {
  const auto b = s.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return "";
  return s.substr(b, s.find_last_not_of(" \t\r\n") - b + 1);
}

bool IsDirectory(const std::string &path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool EndsWith(const std::string &s, const std::string &suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Puts a library light on the same footing as the built-in illuminants.
//
// Published SPDs are relative at whatever level the source used -- the CIE
// tables sit at 100 at 560 nm -- while IlluminantD65() and friends are
// normalised to 1.0 at 560 nm so a scene's _scale means an absolute level.
// Without this, "light:d65" and the illuminant name "D65" would differ by a
// factor of 100 for the same nominal spectrum.
//
// Narrowband sources can have almost no power at 560 nm, and dividing by that
// would explode; those normalise on their peak band instead. Reflectances and
// sensor curves are left alone: reflectance is already absolute, and the
// measured camera curves arrive peak-normalised per channel.
Spectrum NormalizeLight(const Spectrum &s) {
  FP_PRECISION peak = 0.0;
  for (int band = 0; band < kSpectralBands; ++band) {
    peak = std::max(peak, s[band]);
  }
  if (peak <= 0.0) return s;

  const int reference =
      static_cast<int>((560.0 - kLambdaMin) / kBandWidth + 0.5);
  if (reference >= 0 && reference < kSpectralBands &&
      s[reference] > 0.01 * peak) {
    return s / s[reference];
  }
  return s / peak;
}

// Splits "key: value" on the FIRST colon only, so a value may contain colons
// (source lines carry URLs).
bool SplitKeyValue(const std::string &line, std::string &key,
                   std::string &value) {
  const auto colon = line.find(':');
  if (colon == std::string::npos) return false;
  key = Trim(line.substr(0, colon));
  value = Trim(line.substr(colon + 1));
  return !key.empty();
}

}  // namespace

bool SpectrumKindFromString(const std::string &text, SpectrumKind &out) {
  if (text == "light") { out = SpectrumKind::kLight; return true; }
  if (text == "material") { out = SpectrumKind::kMaterial; return true; }
  if (text == "sensor") { out = SpectrumKind::kSensor; return true; }
  return false;
}

const char *SpectrumKindToString(SpectrumKind kind) {
  switch (kind) {
    case SpectrumKind::kLight: return "light";
    case SpectrumKind::kMaterial: return "material";
    case SpectrumKind::kSensor: return "sensor";
  }
  return "material";
}

SpectrumLibrary &SpectrumLibrary::Instance() {
  static SpectrumLibrary instance;
  return instance;
}

// STEP 1: parse one .spd file into records.
//
// The format is deliberately line-oriented rather than JSON: these files are
// generated, occasionally hand-edited, and up to 2 MB, so a format that diffs
// cleanly and parses in one pass is worth more than a nested one.
int SpectrumLibrary::LoadFile(const std::string &path) {
  std::ifstream file(path);
  if (!file) return 0;

  int added = 0;
  bool in_record = false, in_data = false;
  SpectrumRecord record;
  std::vector<FP_PRECISION> wavelengths;
  std::vector<FP_PRECISION> values[3];
  int channel_count = 1;

  // Turns the sample rows accumulated so far into a record and files it.
  auto flush = [&]() {
    if (!in_record || record.name.empty() || wavelengths.size() < 2) {
      in_record = in_data = false;
      return;
    }
    if (record.multichannel) {
      for (int c = 0; c < 3; ++c)
        record.channels[c] = ResampleSpectrum(wavelengths, values[c]);
    } else {
      record.value = ResampleSpectrum(wavelengths, values[0]);
      if (record.kind == SpectrumKind::kLight) {
        record.value = NormalizeLight(record.value);
      }
    }
    record.origin = path;
    records_[std::string(SpectrumKindToString(record.kind)) + ":" +
             record.name] = record;
    ++added;
    in_record = in_data = false;
  };

  std::string line;
  while (std::getline(file, line)) {
    const std::string trimmed = Trim(line);
    if (trimmed.empty()) {
      if (in_data) flush();
      continue;
    }
    if (trimmed[0] == '#') continue;

    // A sample row inside a data block: leading digit, no key.
    if (in_data && (std::isdigit(static_cast<unsigned char>(trimmed[0])) ||
                    trimmed[0] == '-' || trimmed[0] == '+')) {
      std::istringstream row(trimmed);
      FP_PRECISION wavelength;
      if (!(row >> wavelength)) continue;
      FP_PRECISION channel[3] = {0, 0, 0};
      int read = 0;
      while (read < channel_count && (row >> channel[read])) ++read;
      if (read < channel_count) continue;
      wavelengths.push_back(wavelength);
      for (int c = 0; c < channel_count; ++c) values[c].push_back(channel[c]);
      continue;
    }

    std::string key, value;
    if (!SplitKeyValue(trimmed, key, value)) continue;

    if (key == "type") {
      flush();
      record = SpectrumRecord();
      wavelengths.clear();
      for (int c = 0; c < 3; ++c) values[c].clear();
      channel_count = 1;
      if (!SpectrumKindFromString(value, record.kind)) {
        throw std::runtime_error("Unknown spectrum type '" + value + "' in " +
                                 path + ". Known: light, material, sensor.");
      }
      in_record = true;
    } else if (!in_record) {
      continue;  // stray key before the first record
    } else if (key == "name") {
      record.name = value;
    } else if (key == "label") {
      record.label = value;
    } else if (key == "source") {
      record.source = value;
    } else if (key == "channels") {
      std::istringstream names(value);
      std::string name;
      channel_count = 0;
      while (names >> name) ++channel_count;
      if (channel_count != 3) {
        throw std::runtime_error(
            "Spectrum '" + record.name + "' in " + path + " declares " +
            std::to_string(channel_count) +
            " channels; only 3 (R G B) are supported.");
      }
      record.multichannel = true;
    } else if (key == "data") {
      in_data = true;
    }
  }
  flush();
  return added;
}

// STEP 2: walk a directory tree collecting .spd files.
int SpectrumLibrary::LoadDirectory(const std::string &dir) {
  if (!IsDirectory(dir)) return 0;

  int added = 0;
  std::vector<std::string> pending{dir};
  while (!pending.empty()) {
    const std::string current = pending.back();
    pending.pop_back();

    DIR *handle = opendir(current.c_str());
    if (!handle) continue;
    while (dirent *entry = readdir(handle)) {
      const std::string name = entry->d_name;
      if (name == "." || name == "..") continue;
      const std::string full = current + "/" + name;
      if (IsDirectory(full)) {
        pending.push_back(full);
      } else if (EndsWith(name, ".spd")) {
        added += LoadFile(full);
      }
    }
    closedir(handle);
  }

  if (added > 0) directories_.push_back(dir);
  return added;
}

// STEP 3: find the library without the scene author having to say where it is.
//
// Asset paths in this renderer resolve from the current directory, which makes
// running a scene from its own folder the norm (see renderall.sh). The library
// is shared across scenes rather than sitting next to one, so it is looked for
// beside the scene as well as in the current directory.
void SpectrumLibrary::LoadDefault(const std::string &scene_declared,
                                  const std::string &scene_directory) {
  std::vector<std::string> candidates;
  if (!scene_declared.empty()) candidates.push_back(scene_declared);
  if (const char *env = std::getenv("RAYTRACER_SPECTRA_DIR")) {
    if (*env) candidates.push_back(env);
  }
  if (!scene_directory.empty()) {
    candidates.push_back(scene_directory + "/spectra");
    candidates.push_back(scene_directory + "/../spectra");
    candidates.push_back(scene_directory + "/../../spectra");
  }
  candidates.push_back("spectra");
  candidates.push_back("../spectra");

  for (const std::string &candidate : candidates) {
    if (LoadDirectory(candidate) > 0) break;
  }

  // A scene the author explicitly pointed at a directory that turned out to be
  // empty is a mistake worth reporting; the other candidates are guesses.
  if (!scene_declared.empty() && records_.empty()) {
    throw std::runtime_error("SpectralLibrary '" + scene_declared +
                             "' contains no .spd files.");
  }
}

const SpectrumRecord *SpectrumLibrary::Find(const std::string &ref) const {
  const auto it = records_.find(ref);
  return it == records_.end() ? nullptr : &it->second;
}

const SpectrumRecord &SpectrumLibrary::Require(const std::string &ref) const {
  if (const SpectrumRecord *found = Find(ref)) return *found;

  // Nothing matched. Say why in enough detail to fix it without grepping the
  // library by hand: an empty library is a different problem from a typo, and a
  // typo is easiest to spot next to the names that nearly matched.
  std::ostringstream message;
  message << "Unknown spectrum reference '" << ref << "'.";

  if (records_.empty()) {
    message << " The spectral library is empty -- no .spd files were found."
               " Run the renderer from the repository root, set"
               " RAYTRACER_SPECTRA_DIR, or add \"SpectralLibrary\" to the"
               " scene.";
    throw std::runtime_error(message.str());
  }

  const auto colon = ref.find(':');
  const std::string kind =
      colon == std::string::npos ? "" : ref.substr(0, colon);
  const std::string stem =
      colon == std::string::npos ? ref : ref.substr(colon + 1);

  if (!kind.empty()) {
    SpectrumKind parsed;
    if (!SpectrumKindFromString(kind, parsed)) {
      message << " '" << kind
              << "' is not a spectrum type; expected light, material or"
                 " sensor.";
      throw std::runtime_error(message.str());
    }
  } else {
    message << " References look like \"light:d65\" -- the type prefix is"
               " required.";
    throw std::runtime_error(message.str());
  }

  std::vector<std::string> near;
  for (const auto &entry : records_) {
    if (entry.first.compare(0, kind.size(), kind) != 0) continue;
    if (entry.second.name.find(stem) != std::string::npos ||
        (stem.size() > 3 &&
         entry.second.name.compare(0, stem.size(), stem) == 0)) {
      near.push_back(entry.first);
      if (near.size() >= 8) break;
    }
  }

  if (near.empty()) {
    message << " No " << kind << " spectrum contains '" << stem << "'.";
  } else {
    message << " Did you mean:";
    for (const std::string &candidate : near) message << "\n    " << candidate;
  }
  message << "\n  (" << records_.size() << " spectra loaded from ";
  for (size_t i = 0; i < directories_.size(); ++i) {
    message << (i ? ", " : "") << directories_[i];
  }
  message << ")";
  throw std::runtime_error(message.str());
}
