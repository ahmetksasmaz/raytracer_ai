#pragma once

#include "BaseObject.hpp"
#include "Helper.hpp"

class TriangleObject : public BaseObject {
 public:
  TriangleObject(std::shared_ptr<BaseMaterial> material, const Vec3f& v0,
                 const Vec3f& v1, const Vec3f& v2)
      : BaseObject(material), v0_(v0), v1_(v1), v2_(v2) {};

  bool Intersect(const Ray& ray, float& t_hit, Vec3f& intersection_normal,
                 bool backface_culling = true,
                 bool stop_at_any_hit = false) const override;

  virtual ~TriangleObject() = default;
  void Preprocess() override;

 private:
  const Vec3f v0_;
  const Vec3f v1_;
  const Vec3f v2_;
  Vec3f normal_;
};