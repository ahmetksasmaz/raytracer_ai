#include "PPMExporter.hpp"

void PPMExporter::Export(const std::string& filename, const unsigned char* data,
                         const int width, const int height) const {
  std::string changed_filename =
      filename.substr(0, filename.find_last_of('.')) + ".ppm";

  write_ppm(changed_filename.c_str(), data, width, height);
}