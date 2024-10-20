#pragma once

#include "BaseObject.hpp"
#include "Helper.hpp"

class SphereObject : public BaseObject {
 public:
  SphereObject(std::shared_ptr<BaseMaterial> material, const Vec3f& center,
               const float radius)
      : BaseObject(material), center_(center), radius_(radius) {};

  bool Intersect(const Ray& ray, float& t_hit, Vec3f& intersection_normal, bool,
                 bool) const override;

  virtual ~SphereObject() = default;

 private:
  const float radius_;
  const Vec3f center_;
};