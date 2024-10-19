#pragma once

#include "BaseObject.hpp"

class TriangleObject : public BaseObject {
 public:
  TriangleObject(const std::shared_ptr<BaseMaterial>& material, const Vec3f& v0,
                 const Vec3f& v1, const Vec3f& v2)
      : BaseObject(material), v0_(v0), v1_(v1), v2_(v2) {};

  bool Intersect(const Ray& ray, float& t_hit) const override;

  const Vec3f v0_;
  const Vec3f v1_;
  const Vec3f v2_;

  virtual ~TriangleObject() = default;

  void Preprocess() override;
};