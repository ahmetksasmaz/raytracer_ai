#pragma once
#include <iostream>

class BaseExporter {
 public:
  BaseExporter() {}
  virtual void Export(const std::string& filename, const unsigned char* data,
                      const int width, const int height) const = 0;
  virtual ~BaseExporter() {}
};
