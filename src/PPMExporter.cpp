#include "PPMExporter.hpp"

void PPMExporter::Export(std::string filename, unsigned char* data, int width,
                         int height) {
  write_ppm(filename.c_str(), data, width, height);
}