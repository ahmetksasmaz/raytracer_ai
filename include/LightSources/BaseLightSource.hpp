#pragma once
#include "../extern/parser.h"

using namespace parser;

class BaseLightSource {
 public:
  BaseLightSource(const Vec3f& intensity) : intensity_(intensity) {}
  virtual ~BaseLightSource() = default;

  const Vec3f intensity_;
};