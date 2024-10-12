#pragma once
#include <iostream>

class BaseExporter {
 public:
  BaseExporter() {}
  virtual void Export(std::string filename, unsigned char* data, int width,
                      int height) = 0;
  virtual ~BaseExporter() {}
};
