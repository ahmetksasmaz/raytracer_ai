#pragma once

#include <memory>

#include "../extern/parser.h"
#include "BaseMaterial.hpp"
#include "BoundingVolumeHierarchy.hpp"
#include "Ray.hpp"

using namespace parser;

class BaseObject : public BoundingVolumeHierarchyElement,
                   public std::enable_shared_from_this<BaseObject> {
 public:
  BaseObject(std::shared_ptr<BaseMaterial> material,
             const Mat4x4f& transform_matrix)
      : material_(material), transform_matrix_(transform_matrix) {
    inverse_transform_matrix_ = ~transform_matrix_;
    inverse_transpose_transform_matrix_ = !inverse_transform_matrix_;
  }

  virtual void InitializeSelf(const Vec3f& min_point,
                              const Vec3f& max_point) override final {
    min_point_ = min_point;
    max_point_ = max_point;
  }

  std::shared_ptr<BaseMaterial> material_;

  virtual ~BaseObject() = default;
  virtual void Preprocess(bool high_level_bvh_enabled,
                          bool low_level_bvh_enabled,
                          bool transform_enabled = true) {};

  Mat4x4f transform_matrix_;
  Mat4x4f inverse_transform_matrix_;
  Mat4x4f inverse_transpose_transform_matrix_;
};