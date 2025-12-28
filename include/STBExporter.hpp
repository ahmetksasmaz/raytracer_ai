#pragma once

#include "BaseExporter.hpp"

class STBExporter : public BaseExporter<unsigned char> {
 public:
  STBExporter() {}
  void Export(const std::string& filename, const int width, const int height, const unsigned char *uchar_data = nullptr) const override;
  ~STBExporter() {}
};