#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "STBExporter.hpp"

#include <algorithm>

void STBExporter::Export(const std::string &filename, const unsigned char *data,
                         const int width, const int height) const {
  std::string extension = filename.substr(filename.find_last_of(".") + 1);

  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (extension == "png") {
    stbi_write_png(filename.c_str(), width, height, 3, data, width * 3);
  } else if (extension == "bmp") {
    stbi_write_bmp(filename.c_str(), width, height, 3, data);
  } else if (extension == "tga") {
    stbi_write_tga(filename.c_str(), width, height, 3, data);
  } else if (extension == "jpg" || extension == "jpeg") {
    stbi_write_jpg(filename.c_str(), width, height, 3, data, 100);
  } else {
    std::cerr << "Unsupported file format" << std::endl;
  }

  // stbi_write_hdr(filename.c_str(), width, height, 0, const float *data);
}