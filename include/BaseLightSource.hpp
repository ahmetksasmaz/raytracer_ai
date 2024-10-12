#pragma once
#include "../extern/parser.h"

class BaseLightSource {
 public:
  BaseLightSource(Vec3f intensity) : intensity_(intensity) {}
  virtual ~BaseLightSource() = default;

  Vec3f intensity_;
};