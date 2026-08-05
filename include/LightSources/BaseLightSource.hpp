#pragma once
#include "../extern/parser.h"
#include "Spectrum.hpp"

using namespace parser;

class BaseLightSource {
 public:
  BaseLightSource(const Spectrum& intensity) : intensity_(intensity) {}
  virtual ~BaseLightSource() = default;

  const Spectrum intensity_;
};