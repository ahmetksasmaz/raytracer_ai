#pragma once

#include "BaseObject.hpp"

class SphereObject : public BaseObject {
 public:
  SphereObject(const std::shared_ptr<BaseMaterial>& material,
               const Vec3f& center, const float radius)
      : BaseObject(material), center_(center), radius_(radius) {};

  bool Intersect(const Ray& ray, float& t_hit) const override;
  const Vec3f center_;
  const float radius_;

  virtual ~SphereObject() = default;
};