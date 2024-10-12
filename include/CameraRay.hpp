#pragma once
#include <memory>

#include "../extern/parser.h"
#include "BaseRay.hpp"
#include "Pixel.hpp"
#include "SubRay.hpp"

using namespace parser;

class CameraRay : public BaseRay {
 public:
  CameraRay(const Vec3f& origin, const Vec3f& direction,
            std::shared_ptr<Pixel> pixel)
      : BaseRay(origin, direction, 0), pixel_(pixel) {}
  SubRay GenerateSubRay(const Vec3f& origin, const Vec3f& direction) const;
  void FinishRay() override;
  ~CameraRay();

 private:
  std::shared_ptr<Pixel> pixel_;
};