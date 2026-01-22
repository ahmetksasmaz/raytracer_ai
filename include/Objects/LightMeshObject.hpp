#pragma once

#include "BaseObject.hpp"
#include "ObjectLightSource.hpp"
#include "TriangleObject.hpp"
#include "ArrayBVH.hpp"

using namespace ply_reader;

class LightMeshObject : public BaseObject, public ObjectLightSource {
 public:
  LightMeshObject(std::shared_ptr<BaseMaterial> material, std::vector<std::shared_ptr<BaseTextureMap>> textures,
             const std::vector<RawFace>& raw_face_data,
             const std::vector<Vec3f>& raw_vertex_data, const std::vector<Vec2f>& raw_tex_coord_data, const long long vertex_offset, const long long tex_coord_offset, const Vec3f motion_blur,
             const Mat4x4f& transform_matrix, RawScalingFlip scaling_flip, Vec3f radiance);
  LightMeshObject(std::shared_ptr<BaseMaterial> material, std::vector<std::shared_ptr<BaseTextureMap>> textures,
             const std::string& ply_filename, const long long vertex_offset, const long long tex_coord_offset, const Vec3f motion_blur,
             const Mat4x4f& transform_matrix, RawScalingFlip scaling_flip, Vec3f radiance);

  bool Intersect(
      Ray& ray, FP_PRECISION& t_hit, Vec3f& intersection_normal, Vec2f& tex_coords,  Vec2f& hit_u_vector, Vec2f& hit_v_vector, Vec3f& tangent_vector, Vec3f& bitangent_vector,
      bool backface_culling = true,
      bool stop_at_any_hit = false) const override;

  virtual void Sample(const Vec3f& intersection_point, Vec3f &sample_point, Vec3f& sample_normal, FP_PRECISION &pdf) const override;

  virtual ~LightMeshObject() = default;

  void Preprocess(bool transform_enabled = true) override;

  std::vector<std::shared_ptr<TriangleObject>> triangle_objects_;
  ArrayBVH triangle_bvh_;
  std::vector<std::pair<FP_PRECISION, FP_PRECISION>> cdf_pdf_;
};