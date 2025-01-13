#include "QuantizedExporter.hpp"
#include <fstream>

void QuantizedExporter::Export(const std::string& filename, const unsigned char* data,
                         const int width, const int height) const {
  std::string changed_filename =
      filename.substr(0, filename.find_last_of('.')) + ".raw";

  std::ofstream outfile(std::to_string(width)+"_"+std::to_string(height)+"_"+changed_filename, std::ios::binary);
  if (!outfile) {
    throw std::runtime_error("Cannot open file for writing: " + changed_filename);
  }

  for (int i = 0; i < width * height; ++i) {
    outfile.put(data[i]);
  }

  outfile.close();
}