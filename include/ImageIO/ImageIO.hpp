#pragma once

// Image file I/O for the whole pipeline.
//
// This is the one place that instantiates tinyexr and stb. Both are
// single-header libraries whose *_IMPLEMENTATION macro may be defined in
// exactly one translation unit per binary; before the split that constraint was
// satisfied by accident, spread across src/BaseImage.cpp, src/STBExporter.cpp
// and tools/imgdiff.cpp, which is why imgdiff had to be built as its own
// program. Centralising it in one library is what makes a dozen executables
// possible at all.
//
// The formats here are the pipeline's interchange formats:
//
//   ImagePlanes  -- a named set of float planes, which is what a multi-channel
//                   EXR is. A 31-band spectral cube, a 3-channel sensorRGB
//                   image and a 1-channel mosaic are all the same type.
//   PGM16        -- the RAW, as genuine integers rather than floats.
//   PNG8         -- display output only.

#include <string>
#include <vector>

#include "Core/Types.hpp"

namespace image_io {

// A multi-channel image as one contiguous float plane per named channel, each
// width*height in row-major order.
struct ImagePlanes {
  int width = 0;
  int height = 0;
  std::vector<std::string> names;
  std::vector<std::vector<float>> planes;

  size_t PixelCount() const {
    return static_cast<size_t>(width) * static_cast<size_t>(height);
  }
  int ChannelCount() const { return static_cast<int>(names.size()); }

  // Index of a named channel, or -1.
  int IndexOf(const std::string& name) const;
};

// --- Multi-channel EXR ------------------------------------------------------
// Channels are written sorted by name, which is what EXR readers expect and
// what makes the spectral cube's zero-padded "0400nm" naming order correctly.
bool WriteMultiChannelEXR(const std::string& path, const ImagePlanes& image,
                          std::string* error = nullptr);

// Reads every channel of an EXR, whatever they are called and however many
// there are.
//
// This cannot be done with tinyexr's LoadEXR(): that is the RGBA convenience
// API, which always returns four interleaved floats and cannot see a cube whose
// channels are named after wavelengths. Reading a spectral cube back needs the
// lower-level header/image API, which is what this wraps.
bool ReadMultiChannelEXR(const std::string& path, ImagePlanes* out,
                         std::string* error = nullptr);

// --- Spectral cube helpers --------------------------------------------------
// The band structure of a spectral EXR is recorded only in its channel names
// ("0400nm", "0410nm", ...), so it is parsed back out rather than assumed.
// kSpectralBands is a documented free knob, so a cube rendered at 61 bands must
// not be silently misread as 31 -- hence parsing rather than counting.
std::string BandChannelName(FP_PRECISION wavelength_nm);

// Extracts the wavelengths of an image's channels, in file order. Returns false
// if any channel is not a "<number>nm" name.
bool ParseBandWavelengths(const ImagePlanes& image,
                          std::vector<FP_PRECISION>* wavelengths_nm);

// --- 16-bit PGM -------------------------------------------------------------
// A genuine integer format for the RAW, readable by anything. stb only writes
// 8-bit PNG, which would throw away most of a 12-bit range.
bool WritePGM16(const std::string& path, int width, int height,
                const std::vector<FP_PRECISION>& values, int bit_depth,
                const std::string& comment, std::string* error = nullptr);

bool ReadPGM16(const std::string& path, int* width, int* height,
               std::vector<FP_PRECISION>* values, int* max_value,
               std::string* error = nullptr);

// --- 8-bit PNG --------------------------------------------------------------
bool WritePNG8(const std::string& path, int width, int height, int channels,
               const std::vector<unsigned char>& data,
               std::string* error = nullptr);

// --- LDR / RGB read ---------------------------------------------------------
// 8-bit images via stb, for texture loading in the renderer.
bool ReadLDR(const std::string& path, int* width, int* height, int* channels,
             std::vector<unsigned char>* data, std::string* error = nullptr);

}  // namespace image_io
