#include "SensorConfig.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "SpectrumLibrary.hpp"

#include "../../extern/json.hpp"

namespace sensor_config {
namespace {

std::string DirectoryOf(const std::string& path) {
  const size_t slash = path.find_last_of("/\\");
  return slash == std::string::npos ? std::string(".") : path.substr(0, slash);
}

// Numbers are accepted as JSON strings or JSON numbers. Scene files spell every
// value as a string because the JSON format is a mechanical transliteration of
// the XML one; a config written by hand should not have to inherit that.
FP_PRECISION Number(const nlohmann::json& node, const std::string& key,
                    FP_PRECISION fallback) {
  if (!node.contains(key)) return fallback;
  const auto& value = node[key];
  if (value.is_number()) return value.get<FP_PRECISION>();
  if (value.is_string()) return std::stod(value.get<std::string>());
  return fallback;
}

std::string Text(const nlohmann::json& node, const std::string& key,
                 const std::string& fallback) {
  if (!node.contains(key) || !node[key].is_string()) return fallback;
  return node[key].get<std::string>();
}

// A spectral curve, in the same four spellings the scene format accepts:
// a bare name, {"_ref": ...}, {"_illuminant": ...} or {"_data": "nm v nm v"}.
// Precedence is ref > illuminant > data, matching ResolveSpectrum in Scene.cpp.
bool ResolveCurve(const nlohmann::json& parent, const std::string& key,
                  Spectrum* out) {
  if (!parent.contains(key)) return false;
  const auto& node = parent[key];

  std::string ref, illuminant;
  FP_PRECISION scale = 1.0;
  std::vector<FP_PRECISION> wavelengths, values;

  if (node.is_string()) {
    const std::string text = node.get<std::string>();
    // No standard illuminant name contains a colon, so the two spellings
    // cannot collide.
    if (text.find(':') != std::string::npos) {
      ref = text;
    } else {
      illuminant = text;
    }
  } else if (node.is_object()) {
    ref = Text(node, "_ref", "");
    illuminant = Text(node, "_illuminant", "");
    scale = Number(node, "_scale", 1.0);
    if (node.contains("_data")) {
      std::stringstream stream(node["_data"].get<std::string>());
      FP_PRECISION wavelength, value;
      while (stream >> wavelength >> value) {
        wavelengths.push_back(wavelength);
        values.push_back(value);
      }
    }
  } else {
    return false;
  }

  if (!ref.empty()) {
    const SpectrumRecord& record = SpectrumLibrary::Instance().Require(ref);
    if (record.multichannel) {
      throw std::runtime_error(
          "Spectrum reference '" + ref +
          "' has three channels and cannot be used as a single curve."
          " Multi-channel records are camera sensitivities; name one with"
          " \"_ref\" at the top level of the sensor config instead.");
    }
    *out = record.value * scale;
    return true;
  }
  if (!illuminant.empty()) {
    Spectrum resolved;
    if (!IlluminantByName(illuminant, resolved)) {
      throw std::runtime_error("Unknown illuminant '" + illuminant +
                               "'. Known names: D65, A, E.");
    }
    *out = resolved * scale;
    return true;
  }
  if (!values.empty()) {
    *out = ResampleSpectrum(wavelengths, values) * scale;
    return true;
  }
  return false;
}

std::string CurveToData(const Spectrum& curve) {
  std::ostringstream out;
  for (int band = 0; band < kSpectralBands; band++) {
    if (band) out << " ";
    out << BandWavelength(band) << " " << curve[band];
  }
  return out.str();
}

const char* PatternName(BayerPattern pattern) {
  switch (pattern) {
    case BayerPattern::kRGGB: return "RGGB";
    case BayerPattern::kBGGR: return "BGGR";
    case BayerPattern::kGRBG: return "GRBG";
    case BayerPattern::kGBRG: return "GBRG";
  }
  return "RGGB";
}

}  // namespace

SensorModel Load(const std::string& path, const std::string& spectra_directory) {
  std::ifstream file(path);
  if (!file.is_open()) {
    throw std::runtime_error("Cannot open sensor config '" + path + "'.");
  }

  nlohmann::json json;
  try {
    file >> json;
  } catch (const std::exception& e) {
    throw std::runtime_error("Sensor config '" + path +
                             "' is not valid JSON: " + e.what());
  }

  // Allow the config to be wrapped in a "Sensor" key, so a block lifted
  // verbatim out of an old scene file loads without editing.
  const nlohmann::json& node = json.contains("Sensor") ? json["Sensor"] : json;

  SpectrumLibrary::Instance().LoadDefault(spectra_directory, DirectoryOf(path));

  SensorModel sensor;
  sensor.exposure_time_s = Number(node, "ExposureTime", sensor.exposure_time_s);
  sensor.pixel_pitch_m = Number(node, "PixelPitch", sensor.pixel_pitch_m);
  sensor.f_number = Number(node, "FNumber", sensor.f_number);
  sensor.full_well_e = Number(node, "FullWell", sensor.full_well_e);
  sensor.gain_e_per_dn = Number(node, "Gain", sensor.gain_e_per_dn);
  sensor.bit_depth = static_cast<int>(Number(node, "BitDepth", sensor.bit_depth));
  sensor.dynamic_range_stops =
      Number(node, "DynamicRange", sensor.dynamic_range_stops);
  sensor.noise.read_noise_sigma_e =
      Number(node, "ReadNoise", sensor.noise.read_noise_sigma_e);
  sensor.noise.dark_current_e_per_s =
      Number(node, "DarkCurrent", sensor.noise.dark_current_e_per_s);

  const std::string pattern = Text(node, "_pattern", "RGGB");
  if (pattern == "RGGB") sensor.pattern = BayerPattern::kRGGB;
  else if (pattern == "BGGR") sensor.pattern = BayerPattern::kBGGR;
  else if (pattern == "GRBG") sensor.pattern = BayerPattern::kGRBG;
  else if (pattern == "GBRG") sensor.pattern = BayerPattern::kGBRG;
  else throw std::runtime_error("Unknown Bayer pattern '" + pattern +
                                "' in " + path +
                                ". Known: RGGB, BGGR, GRBG, GBRG.");

  // Substring-matched, exactly as the scene format did. "None" -- or any string
  // naming none of the three -- disables all of them.
  const std::string noise = Text(node, "NoiseSources", "Shot Read Dark");
  sensor.noise.shot_noise = noise.find("Shot") != std::string::npos;
  sensor.noise.read_noise = noise.find("Read") != std::string::npos;
  sensor.noise.dark_current = noise.find("Dark") != std::string::npos;

  // A measured camera sensitivity is the whole optical response -- quantum
  // efficiency times colour filter times everything else in the path -- so it
  // fills the three CFA curves and leaves quantum efficiency flat at 1. Folding
  // it into the CFA *and* keeping a separate QE would apply the sensor's own
  // efficiency twice.
  const std::string ref = Text(node, "_ref", "");
  if (!ref.empty()) {
    const SpectrumRecord& record = SpectrumLibrary::Instance().Require(ref);
    if (!record.multichannel) {
      throw std::runtime_error(
          "Sensor reference '" + ref +
          "' is a single-curve spectrum, not a camera sensitivity. Sensor"
          " references must name a record with three channels.");
    }
    sensor.quantum_efficiency = Spectrum::Constant(1.0);
    for (int channel = 0; channel < 3; ++channel) {
      sensor.cfa[channel] = record.channels[channel];
    }
  } else {
    DefaultBayerCFA(sensor.cfa);
  }

  ResolveCurve(node, "QuantumEfficiency", &sensor.quantum_efficiency);
  ResolveCurve(node, "FilterRed", &sensor.cfa[0]);
  ResolveCurve(node, "FilterGreen", &sensor.cfa[1]);
  ResolveCurve(node, "FilterBlue", &sensor.cfa[2]);

  return sensor;
}

bool Save(const std::string& path, const SensorModel& sensor,
          std::string* error) {
  std::ofstream out(path);
  if (!out) {
    if (error) *error = "cannot open " + path;
    return false;
  }

  std::string noise_sources;
  if (sensor.noise.shot_noise) noise_sources += "Shot ";
  if (sensor.noise.read_noise) noise_sources += "Read ";
  if (sensor.noise.dark_current) noise_sources += "Dark";
  if (noise_sources.empty()) noise_sources = "None";

  out << "{\n";
  out << "  \"_comment\": \"Sensor configuration. Spectral curves are expanded "
         "to explicit tables so this file records exactly what produced a RAW, "
         "even if the library reference it came from later changes.\",\n";
  out << "  \"_pattern\": \"" << PatternName(sensor.pattern) << "\",\n";
  out << "  \"ExposureTime\": " << sensor.exposure_time_s << ",\n";
  out << "  \"PixelPitch\": " << sensor.pixel_pitch_m << ",\n";
  out << "  \"FNumber\": " << sensor.f_number << ",\n";
  out << "  \"FullWell\": " << sensor.full_well_e << ",\n";
  out << "  \"Gain\": " << sensor.gain_e_per_dn << ",\n";
  out << "  \"BitDepth\": " << sensor.bit_depth << ",\n";
  out << "  \"DynamicRange\": " << sensor.dynamic_range_stops << ",\n";
  out << "  \"ReadNoise\": " << sensor.noise.read_noise_sigma_e << ",\n";
  out << "  \"DarkCurrent\": " << sensor.noise.dark_current_e_per_s << ",\n";
  out << "  \"NoiseSources\": \"" << noise_sources << "\",\n";
  out << "  \"QuantumEfficiency\": {\"_data\": \""
      << CurveToData(sensor.quantum_efficiency) << "\"},\n";
  out << "  \"FilterRed\": {\"_data\": \"" << CurveToData(sensor.cfa[0])
      << "\"},\n";
  out << "  \"FilterGreen\": {\"_data\": \"" << CurveToData(sensor.cfa[1])
      << "\"},\n";
  out << "  \"FilterBlue\": {\"_data\": \"" << CurveToData(sensor.cfa[2])
      << "\"}\n";
  out << "}\n";
  return static_cast<bool>(out);
}

}  // namespace sensor_config
