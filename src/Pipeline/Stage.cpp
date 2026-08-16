#include "Pipeline/Stage.hpp"

#include <cstdio>
#include <cstdlib>

namespace pipeline {
namespace {

// The channel names a three-channel sensor-space or XYZ image uses. XYZ images
// keep R/G/B names because that is what every EXR viewer expects to find; which
// space the numbers are in is recorded by the file's postfix, not its channels.
const char* kTripleNames[3] = {"R", "G", "B"};

}  // namespace

Arguments::Arguments(int argc, char** argv) {
  program_ = argc > 0 ? argv[0] : "stage";
  const size_t slash = program_.find_last_of("/\\");
  if (slash != std::string::npos) program_ = program_.substr(slash + 1);

  for (int i = 1; i < argc; i++) {
    std::string token = argv[i];
    if (token.rfind("--", 0) != 0) continue;
    const std::string name = token.substr(2);
    // A flag with no value (e.g. --gamma) records an empty string, which Has()
    // still reports as present.
    if (i + 1 < argc && std::string(argv[i + 1]).rfind("--", 0) != 0) {
      values_.emplace_back(name, argv[++i]);
    } else {
      values_.emplace_back(name, "");
    }
  }
}

std::string Arguments::Get(const std::string& name,
                           const std::string& fallback) const {
  for (const auto& entry : values_) {
    if (entry.first == name) return entry.second.empty() ? fallback : entry.second;
  }
  return fallback;
}

bool Arguments::Has(const std::string& name) const {
  for (const auto& entry : values_) {
    if (entry.first == name) return true;
  }
  return false;
}

FP_PRECISION Arguments::Number(const std::string& name,
                               FP_PRECISION fallback) const {
  const std::string text = Get(name);
  return text.empty() ? fallback : std::atof(text.c_str());
}

int Arguments::Integer(const std::string& name, int fallback) const {
  const std::string text = Get(name);
  return text.empty() ? fallback : std::atoi(text.c_str());
}

int Usage(const std::string& program, const std::string& usage) {
  std::fprintf(stderr, "usage: %s %s\n", program.c_str(), usage.c_str());
  return 1;
}

int Fail(const std::string& program, const std::string& message) {
  std::fprintf(stderr, "%s: %s\n", program.c_str(), message.c_str());
  return 1;
}

bool ReadSingle(const std::string& path, int* width, int* height,
                std::vector<FP_PRECISION>* values, std::string* error) {
  image_io::ImagePlanes image;
  if (!image_io::ReadMultiChannelEXR(path, &image, error)) return false;
  if (image.ChannelCount() < 1) {
    *error = path + " has no channels";
    return false;
  }
  if (image.ChannelCount() > 1) {
    *error = path + " has " + std::to_string(image.ChannelCount()) +
             " channels; this stage expects a single-channel image."
             " Is it pointed at a demosaiced image or a spectral cube?";
    return false;
  }

  *width = image.width;
  *height = image.height;
  values->assign(image.planes[0].begin(), image.planes[0].end());
  return true;
}

bool ReadTriple(const std::string& path, int* width, int* height,
                std::vector<FP_PRECISION> rgb[3], std::string* error) {
  image_io::ImagePlanes image;
  if (!image_io::ReadMultiChannelEXR(path, &image, error)) return false;
  if (image.ChannelCount() != 3) {
    *error = path + " has " + std::to_string(image.ChannelCount()) +
             " channels; this stage expects three (R, G, B)."
             " Is it pointed at a mosaic or a spectral cube?";
    return false;
  }

  *width = image.width;
  *height = image.height;
  for (int c = 0; c < 3; c++) {
    const int index = image.IndexOf(kTripleNames[c]);
    if (index < 0) {
      *error = path + " has no '" + kTripleNames[c] + "' channel";
      return false;
    }
    rgb[c].assign(image.planes[index].begin(), image.planes[index].end());
  }
  return true;
}

bool ReadSpectral(const std::string& path, int* width, int* height,
                  std::vector<Spectrum>* spectra, std::string* error) {
  image_io::ImagePlanes image;
  if (!image_io::ReadMultiChannelEXR(path, &image, error)) return false;

  // The band grid is recorded only in the channel names, so it is parsed rather
  // than assumed. kSpectralBands is a compile-time knob: a cube rendered at 61
  // bands must not be silently misread as 31, and a cube rendered at 31 must
  // still load into a binary built for 61.
  std::vector<FP_PRECISION> wavelengths;
  if (!image_io::ParseBandWavelengths(image, &wavelengths)) {
    *error = path +
             " does not look like a spectral cube: its channels are not named"
             " by wavelength (expected '0400nm', '0410nm', ...)";
    return false;
  }

  *width = image.width;
  *height = image.height;
  const size_t pixel_count = image.PixelCount();
  spectra->assign(pixel_count, Spectrum::Zero());

  const bool same_grid =
      static_cast<int>(wavelengths.size()) == kSpectralBands &&
      std::fabs(wavelengths.front() - BandWavelength(0)) < 1e-6 &&
      std::fabs(wavelengths.back() - BandWavelength(kSpectralBands - 1)) < 1e-6;

  if (same_grid) {
    for (int band = 0; band < kSpectralBands; band++) {
      const std::vector<float>& plane = image.planes[band];
      for (size_t i = 0; i < pixel_count; i++) {
        (*spectra)[i][band] = plane[i];
      }
    }
    return true;
  }

  // Different grid: resample each pixel onto the compiled-in bands.
  std::vector<FP_PRECISION> values(wavelengths.size());
  for (size_t i = 0; i < pixel_count; i++) {
    for (size_t band = 0; band < wavelengths.size(); band++) {
      values[band] = image.planes[band][i];
    }
    (*spectra)[i] = ResampleSpectrum(wavelengths, values);
  }
  return true;
}

bool WriteSingle(const std::string& path, int width, int height,
                 const std::vector<FP_PRECISION>& values,
                 const std::string& channel_name, std::string* error) {
  image_io::ImagePlanes image;
  image.width = width;
  image.height = height;
  image.names.push_back(channel_name);
  image.planes.emplace_back(values.begin(), values.end());
  return image_io::WriteMultiChannelEXR(path, image, error);
}

bool WriteTriple(const std::string& path, int width, int height,
                 const std::vector<FP_PRECISION> rgb[3], std::string* error) {
  image_io::ImagePlanes image;
  image.width = width;
  image.height = height;
  for (int c = 0; c < 3; c++) {
    image.names.push_back(kTripleNames[c]);
    image.planes.emplace_back(rgb[c].begin(), rgb[c].end());
  }
  return image_io::WriteMultiChannelEXR(path, image, error);
}

bool WriteSpectral(const std::string& path, int width, int height,
                   const std::vector<Spectrum>& spectra, std::string* error) {
  image_io::ImagePlanes image;
  image.width = width;
  image.height = height;
  const size_t pixel_count = image.PixelCount();

  for (int band = 0; band < kSpectralBands; band++) {
    image.names.push_back(image_io::BandChannelName(BandWavelength(band)));
    std::vector<float> plane(pixel_count);
    for (size_t i = 0; i < pixel_count; i++) {
      plane[i] = static_cast<float>(spectra[i][band]);
    }
    image.planes.push_back(std::move(plane));
  }
  return image_io::WriteMultiChannelEXR(path, image, error);
}

}  // namespace pipeline
