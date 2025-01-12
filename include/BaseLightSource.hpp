#pragma once
#include "../extern/parser.h"
#include "BaseSpectrum.hpp"

using namespace parser;

class BaseLightSource {
 public:
  BaseLightSource(const float power,
                  const std::shared_ptr<BaseSpectrum> spectrum)
      : power_(power), spectrum_(spectrum) {}
  virtual ~BaseLightSource() = default;

  const float power_;
  const std::shared_ptr<BaseSpectrum> spectrum_;
};