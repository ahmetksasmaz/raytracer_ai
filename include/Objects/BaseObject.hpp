#pragma once

#include <memory>

#include "../extern/parser.h"
#include "BaseMaterial.hpp"
#include "BaseTextureMap.hpp"
#include "BoundingVolumeHierarchy.hpp"
#include "Ray.hpp"

using namespace parser;

class BaseObject : public std::enable_shared_from_this<BaseObject> {
 public:
  BaseObject(std::shared_ptr<BaseMaterial> material, std::vector<std::shared_ptr<BaseTextureMap>> textures, const Vec3f motion_blur,
             const Mat4x4f& transform_matrix, RawScalingFlip scaling_flip)
      : material_(material),
        textures_(textures),
        motion_blur_(motion_blur),
        transform_matrix_(transform_matrix),
        scaling_flip_(scaling_flip) {
    inverse_transform_matrix_ = ~transform_matrix_;
    inverse_transpose_transform_matrix_ = !inverse_transform_matrix_;
    transpose_transform_matrix_ = !transform_matrix_;
  }

  void InitializeSelf(const Vec3f& min_point, const Vec3f& max_point) {
    min_point_ = min_point;
    max_point_ = max_point;
  }

  // t_hit is a distance along the ray, measured in world space. Implementations
  // intersect in object space and convert back via
  // t_hit = |transform * local_point - ray.origin|, which is a valid parameter
  // for the original ray as long as ray.direction_ is unit length (every ray in
  // the renderer is). Implementations used to also overwrite ray.direction_
  // with the normalized hit vector; that was a no-op compensating for a wrong
  // direction transform in SphereObject, and taking a mutable Ray& forced the
  // BVH to copy a Ray per primitive test.
  virtual bool Intersect(
      const Ray& ray, FP_PRECISION& t_hit, Vec3f& intersection_normal, Vec2f& tex_coords, Vec2f& hit_u_vector, Vec2f& hit_v_vector, Vec3f& tangent_vector, Vec3f& bitangent_vector,
      bool backface_culling = true, bool stop_at_any_hit = false) const = 0;

  std::shared_ptr<BaseMaterial> material_;

  virtual ~BaseObject() = default;
  virtual void Preprocess(bool transform_enabled = true) {};

  Vec3f motion_blur_;
  Mat4x4f transform_matrix_;
  Mat4x4f transpose_transform_matrix_;
  Mat4x4f inverse_transform_matrix_;
  Mat4x4f inverse_transpose_transform_matrix_;
  RawScalingFlip scaling_flip_;
  std::vector<std::shared_ptr<BaseTextureMap>> textures_;
  
  Vec3f min_point_;
  Vec3f max_point_;
};