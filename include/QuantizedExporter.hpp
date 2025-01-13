#pragma once
#include "BaseExporter.hpp"

class QuantizedExporter : public BaseExporter {
 public:
  QuantizedExporter() {}
  void Export(const std::string& filename, const unsigned char* data,
              const int width, const int height) const override;
  ~QuantizedExporter() {}
};