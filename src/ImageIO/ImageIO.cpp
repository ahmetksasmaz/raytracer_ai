#include "ImageIO/ImageIO.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

// This is the ONE translation unit in the whole project that instantiates
// tinyexr and stb. Every executable links this library and no other file
// defines these macros, so the "one implementation per binary" rule is
// structural rather than a convention someone has to remember.
//
// The tinyexr configuration must stay exactly as it is: it decompresses through
// stb's zlib rather than miniz, so stb_image has to be pulled in first.
#define STB_IMAGE_IMPLEMENTATION
#include "../../extern/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../../extern/stb_image_write.h"

#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#define TINYEXR_IMPLEMENTATION
#include "../../extern/tinyexr.h"

namespace image_io {
namespace {

void SetError(std::string* error, const std::string& message) {
  if (error) *error = message;
}

}  // namespace

int ImagePlanes::IndexOf(const std::string& name) const {
  for (size_t i = 0; i < names.size(); i++) {
    if (names[i] == name) return static_cast<int>(i);
  }
  return -1;
}

bool WriteMultiChannelEXR(const std::string& path, const ImagePlanes& image,
                          std::string* error) {
  if (image.names.size() != image.planes.size() || image.planes.empty()) {
    SetError(error, "channel names and planes disagree, or there are none");
    return false;
  }
  const size_t expected = image.PixelCount();
  for (const auto& plane : image.planes) {
    if (plane.size() != expected) {
      SetError(error, "a plane does not match width*height");
      return false;
    }
  }

  std::vector<int> order(image.names.size());
  for (size_t i = 0; i < order.size(); i++) order[i] = static_cast<int>(i);
  std::sort(order.begin(), order.end(),
            [&](int a, int b) { return image.names[a] < image.names[b]; });

  std::vector<float*> plane_ptrs(image.planes.size());
  for (size_t i = 0; i < order.size(); i++) {
    plane_ptrs[i] = const_cast<float*>(image.planes[order[i]].data());
  }

  EXRHeader header;
  InitEXRHeader(&header);
  EXRImage exr_image;
  InitEXRImage(&exr_image);

  exr_image.num_channels = static_cast<int>(image.planes.size());
  exr_image.images = reinterpret_cast<unsigned char**>(plane_ptrs.data());
  exr_image.width = image.width;
  exr_image.height = image.height;

  header.num_channels = exr_image.num_channels;
  header.channels = static_cast<EXRChannelInfo*>(
      malloc(sizeof(EXRChannelInfo) * header.num_channels));
  header.pixel_types =
      static_cast<int*>(malloc(sizeof(int) * header.num_channels));
  header.requested_pixel_types =
      static_cast<int*>(malloc(sizeof(int) * header.num_channels));

  for (int i = 0; i < header.num_channels; i++) {
    const std::string& name = image.names[order[i]];
    std::strncpy(header.channels[i].name, name.c_str(), 255);
    header.channels[i].name[std::min<size_t>(name.size(), 255)] = '\0';
    header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
    // Full float, not half: spectral radiance and digital numbers both span a
    // wide range, and half would quantise the data this whole pipeline exists
    // to preserve.
    header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
  }

  const char* err = nullptr;
  const int ret = SaveEXRImageToFile(&exr_image, &header, path.c_str(), &err);

  free(header.channels);
  free(header.pixel_types);
  free(header.requested_pixel_types);

  if (ret != TINYEXR_SUCCESS) {
    SetError(error, std::string("cannot write ") + path + ": " +
                        (err ? err : "unknown error"));
    if (err) FreeEXRErrorMessage(err);
    return false;
  }
  return true;
}

bool ReadMultiChannelEXR(const std::string& path, ImagePlanes* out,
                         std::string* error) {
  if (!out) return false;

  EXRVersion version;
  if (ParseEXRVersionFromFile(&version, path.c_str()) != TINYEXR_SUCCESS) {
    SetError(error, std::string("cannot read ") + path + ": not an EXR file");
    return false;
  }

  EXRHeader header;
  InitEXRHeader(&header);
  const char* err = nullptr;
  if (ParseEXRHeaderFromFile(&header, &version, path.c_str(), &err) !=
      TINYEXR_SUCCESS) {
    SetError(error, std::string("cannot read header of ") + path + ": " +
                        (err ? err : "unknown error"));
    if (err) FreeEXRErrorMessage(err);
    return false;
  }

  // Ask for float regardless of how the file stores it, so a half-float EXR
  // (which is what the conventional renderer output is) reads back through the
  // same path as a full-float sensor product.
  for (int i = 0; i < header.num_channels; i++) {
    if (header.pixel_types[i] == TINYEXR_PIXELTYPE_HALF) {
      header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
    }
  }

  EXRImage exr_image;
  InitEXRImage(&exr_image);
  if (LoadEXRImageFromFile(&exr_image, &header, path.c_str(), &err) !=
      TINYEXR_SUCCESS) {
    SetError(error, std::string("cannot read pixels of ") + path + ": " +
                        (err ? err : "unknown error"));
    if (err) FreeEXRErrorMessage(err);
    FreeEXRHeader(&header);
    return false;
  }

  out->width = exr_image.width;
  out->height = exr_image.height;
  out->names.clear();
  out->planes.clear();

  const size_t pixel_count = out->PixelCount();
  bool ok = true;
  for (int c = 0; c < exr_image.num_channels; c++) {
    out->names.push_back(header.channels[c].name);
    std::vector<float> plane(pixel_count, 0.0f);
    if (header.pixel_types[c] == TINYEXR_PIXELTYPE_UINT) {
      // Integer channels are not something this pipeline writes, but refusing
      // silently would be worse than converting.
      const unsigned int* src =
          reinterpret_cast<const unsigned int*>(exr_image.images[c]);
      for (size_t i = 0; i < pixel_count; i++) {
        plane[i] = static_cast<float>(src[i]);
      }
    } else {
      const float* src = reinterpret_cast<const float*>(exr_image.images[c]);
      if (!src) {
        ok = false;
        break;
      }
      std::memcpy(plane.data(), src, pixel_count * sizeof(float));
    }
    out->planes.push_back(std::move(plane));
  }

  FreeEXRImage(&exr_image);
  FreeEXRHeader(&header);

  if (!ok) {
    SetError(error, std::string("cannot read pixels of ") + path);
    return false;
  }
  return true;
}

std::string BandChannelName(FP_PRECISION wavelength_nm) {
  // Zero-padded so lexical channel ordering matches wavelength ordering, which
  // is what lets an EXR reader hand the bands back in the right order.
  char name[32];
  std::snprintf(name, sizeof(name), "%04.0fnm", wavelength_nm);
  return name;
}

bool ParseBandWavelengths(const ImagePlanes& image,
                          std::vector<FP_PRECISION>* wavelengths_nm) {
  if (!wavelengths_nm) return false;
  wavelengths_nm->clear();
  wavelengths_nm->reserve(image.names.size());

  for (const std::string& name : image.names) {
    // Expect "<digits>nm". Anything else means this is not a spectral cube.
    if (name.size() < 3) return false;
    if (name.compare(name.size() - 2, 2, "nm") != 0) return false;
    const std::string digits = name.substr(0, name.size() - 2);
    if (digits.empty()) return false;
    for (char c : digits) {
      if (!std::isdigit(static_cast<unsigned char>(c)) && c != '.') return false;
    }
    wavelengths_nm->push_back(std::atof(digits.c_str()));
  }
  return !wavelengths_nm->empty();
}

bool WritePGM16(const std::string& path, int width, int height,
                const std::vector<FP_PRECISION>& values, int bit_depth,
                const std::string& comment, std::string* error) {
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    SetError(error, std::string("cannot open ") + path);
    return false;
  }

  const int max_value = (1 << bit_depth) - 1;
  out << "P5\n"
      << "# " << comment << "\n"
      << width << " " << height << "\n"
      << max_value << "\n";
  // 16-bit binary PGM: big-endian sample order, per the format spec.
  for (size_t i = 0; i < values.size(); i++) {
    const int v = std::min(std::max(static_cast<int>(values[i]), 0), max_value);
    const unsigned char bytes[2] = {static_cast<unsigned char>(v >> 8),
                                    static_cast<unsigned char>(v & 0xFF)};
    out.write(reinterpret_cast<const char*>(bytes), 2);
  }
  return static_cast<bool>(out);
}

bool ReadPGM16(const std::string& path, int* width, int* height,
               std::vector<FP_PRECISION>* values, int* max_value,
               std::string* error) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    SetError(error, std::string("cannot open ") + path);
    return false;
  }

  // Header fields are whitespace-separated ASCII with '#' comments allowed
  // anywhere between them.
  auto next_token = [&](std::string* token) -> bool {
    token->clear();
    int c;
    while ((c = in.get()) != EOF) {
      if (c == '#') {
        while ((c = in.get()) != EOF && c != '\n') {
        }
        continue;
      }
      if (std::isspace(c)) {
        if (!token->empty()) return true;
        continue;
      }
      token->push_back(static_cast<char>(c));
    }
    return !token->empty();
  };

  std::string magic, w, h, maxv;
  if (!next_token(&magic) || magic != "P5") {
    SetError(error, path + " is not a binary PGM");
    return false;
  }
  if (!next_token(&w) || !next_token(&h) || !next_token(&maxv)) {
    SetError(error, path + " has a truncated PGM header");
    return false;
  }

  *width = std::atoi(w.c_str());
  *height = std::atoi(h.c_str());
  const int maximum = std::atoi(maxv.c_str());
  if (max_value) *max_value = maximum;
  if (*width <= 0 || *height <= 0 || maximum <= 0) {
    SetError(error, path + " has an invalid PGM header");
    return false;
  }
  if (maximum <= 255) {
    SetError(error, path + " is an 8-bit PGM; this pipeline writes 16-bit");
    return false;
  }

  const size_t count = static_cast<size_t>(*width) * (*height);
  values->assign(count, 0.0);
  for (size_t i = 0; i < count; i++) {
    unsigned char bytes[2];
    if (!in.read(reinterpret_cast<char*>(bytes), 2)) {
      SetError(error, path + " ends before the pixel data does");
      return false;
    }
    (*values)[i] = static_cast<FP_PRECISION>((bytes[0] << 8) | bytes[1]);
  }
  return true;
}

bool WritePNG8(const std::string& path, int width, int height, int channels,
               const std::vector<unsigned char>& data, std::string* error) {
  const size_t expected =
      static_cast<size_t>(width) * height * static_cast<size_t>(channels);
  if (data.size() != expected) {
    SetError(error, "PNG data size does not match width*height*channels");
    return false;
  }
  if (!stbi_write_png(path.c_str(), width, height, channels, data.data(),
                      width * channels)) {
    SetError(error, std::string("cannot write ") + path);
    return false;
  }
  return true;
}

bool ReadLDR(const std::string& path, int* width, int* height, int* channels,
             std::vector<unsigned char>* data, std::string* error) {
  int native_channels = 0;
  unsigned char* pixels =
      stbi_load(path.c_str(), width, height, &native_channels, *channels);
  if (!pixels) {
    SetError(error, std::string("cannot read ") + path);
    return false;
  }
  const size_t count =
      static_cast<size_t>(*width) * (*height) * static_cast<size_t>(*channels);
  data->assign(pixels, pixels + count);
  stbi_image_free(pixels);
  return true;
}

}  // namespace image_io
