#pragma once

#include "BaseExporter.hpp"

class EXRExporter : public BaseExporter<float> {
 public:
  EXRExporter() {}
  void Export(const std::string& filename, const int width, const int height, const float *float_data = nullptr) const override;
  ~EXRExporter() {}
};