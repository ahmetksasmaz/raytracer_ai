#pragma once
#include "../extern/ppm.h"
#include "BaseExporter.hpp"

class PPMExporter : public BaseExporter {
 public:
  PPMExporter() {}
  void Export(std::string filename, unsigned char* data, int width,
              int height) override;
  ~PPMExporter() {}
};