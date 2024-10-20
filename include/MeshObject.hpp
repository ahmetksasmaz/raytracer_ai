#pragma once

#include <unordered_map>

#include "BaseObject.hpp"
#include "TriangleObject.hpp"

class MeshObject : public BaseObject {
 public:
  MeshObject(std::shared_ptr<BaseMaterial> material,
             const std::vector<RawFace>& raw_face_data,
             const std::vector<Vec3f>& raw_vertex_data);

  bool Intersect(const Ray& ray, float& t_hit, Vec3f& intersection_normal,
                 bool backface_culling = true,
                 bool stop_at_any_hit = false) const override;

  virtual ~MeshObject() = default;

  void Preprocess() override;

 private:
  std::vector<Vec3f> vertex_data_;
  std::vector<Vec3i> face_data_;
  std::vector<Vec3f> normals_;
};