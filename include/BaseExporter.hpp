#pragma once
#include <iostream>

template <typename T>
class BaseExporter {
 public:
  BaseExporter() {}
  virtual void Export(const std::string& filename, const int width, const int height, const T *data) const = 0;
  virtual ~BaseExporter() {}
};
