#include "STBExporter.hpp"
#include <algorithm>
#include <vector>

// Encoding goes through rt_imageio, which is the only translation unit that
// instantiates stb. This file used to define STB_IMAGE_WRITE_IMPLEMENTATION
// itself, which is a duplicate-symbol link error once several executables link
// the same libraries.
#include "ImageIO/ImageIO.hpp"

void STBExporter::Export(const std::string &filename, const int width, const int height, const unsigned char *uchar_data) const {
  std::string extension = filename.substr(filename.find_last_of(".") + 1);

  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (extension != "png") {
    // bmp/tga/jpg used to be reachable here. Nothing in the project writes them
    // -- every scene names a .png or a .exr -- so rather than widen the shared
    // I/O library for formats with no caller, say so plainly.
    std::cerr << "Unsupported file format '" << extension << "' for "
              << filename << "; LDR output is PNG." << std::endl;
    return;
  }

  const size_t count = static_cast<size_t>(width) * height * 3;
  const std::vector<unsigned char> pixels(uchar_data, uchar_data + count);

  std::string error;
  if (!image_io::WritePNG8(filename, width, height, 3, pixels, &error)) {
    std::cerr << error << std::endl;
  }
}
