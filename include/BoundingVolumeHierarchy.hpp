#pragma once

#include <memory>

#include "../extern/parser.h"
#include "Ray.hpp"

using namespace parser;

class BoundingVolumeHierarchyElement;

class BoundingVolumeHierarchyElement {
 public:
  virtual void InitializeSelf(const Vec3f& min_point, const Vec3f& max_point) {
  };
  void InitializeChildren(
      std::shared_ptr<BoundingVolumeHierarchyElement> left,
      std::shared_ptr<BoundingVolumeHierarchyElement> right) {
    left_ = left;
    right_ = right;
  }
  virtual std::shared_ptr<BoundingVolumeHierarchyElement> Intersect(
      const Ray& ray, float& t_hit, Vec3f& intersection_normal,
      bool backface_culling = true, bool stop_at_any_hit = false) const;

  virtual ~BoundingVolumeHierarchyElement() = default;

  static std::shared_ptr<BoundingVolumeHierarchyElement> Construct(
      std::vector<std::shared_ptr<BoundingVolumeHierarchyElement>>& leafs,
      int start, int end, int axis);

  std::shared_ptr<BoundingVolumeHierarchyElement> left_ = nullptr;
  std::shared_ptr<BoundingVolumeHierarchyElement> right_ = nullptr;

  Vec3f min_point_;
  Vec3f max_point_;
};