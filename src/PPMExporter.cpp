#include "PPMExporter.hpp"

void PPMExporter::Export(const std::string& filename, const unsigned char* data,
                         const int width, const int height) const {
  write_ppm(filename.c_str(), data, width, height);
}