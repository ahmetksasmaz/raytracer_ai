#pragma once
#include "../extern/ppm.h"
#include "BaseExporter.hpp"

class PPMExporter : public BaseExporter {
 public:
  PPMExporter() {}
  void Export(const std::string& filename, const unsigned char* data,
              const int width, const int height) const override;
  ~PPMExporter() {}
};