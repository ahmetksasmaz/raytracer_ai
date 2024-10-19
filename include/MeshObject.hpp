#pragma once

#include "BaseObject.hpp"
#include "TriangleObject.hpp"

class MeshObject : public BaseObject {
 public:
  MeshObject(const std::shared_ptr<BaseMaterial>& material)
      : BaseObject(material) {};

  bool Intersect(const Ray& ray, float& t_hit) const override;

  std::vector<Vec3f> vertex_data_;
  std::vector<Vec3i> face_data_;

  virtual ~MeshObject() = default;

  void Preprocess() override;
};