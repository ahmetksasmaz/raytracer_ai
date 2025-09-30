#pragma once

#include "BaseExporter.hpp"

class STBExporter : public BaseExporter {
 public:
  STBExporter() {}
  void Export(const std::string& filename, const unsigned char* data,
              const int width, const int height) const override;
  ~STBExporter() {}
};