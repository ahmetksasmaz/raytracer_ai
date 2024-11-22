#include "BoundingVolumeHierarchy.hpp"

#include "Helper.hpp"

uint64_t BoundingVolumeHierarchyElement::id_counter_ = 0;
bool BoundingVolumeHierarchyElement::trace_ = false;
std::vector<Vec2i> BoundingVolumeHierarchyElement::trace_pixels_;

std::shared_ptr<BoundingVolumeHierarchyElement>
BoundingVolumeHierarchyElement::Intersect(Ray& ray, float& t_hit,
                                          Vec3f& intersection_normal,
                                          bool backface_culling,
                                          bool stop_at_any_hit) const {
  if (trace_ && std::find(trace_pixels_.begin(), trace_pixels_.end(),
                          ray.pixel_) != trace_pixels_.end()) {
    std::cout << "Checking node id: " << id_ << std::endl;
    std::cout << "Min point: " << min_point_ << std::endl;
    std::cout << "Max point: " << max_point_ << std::endl;
    std::cout << "Ray pixel: " << ray.pixel_ << std::endl;
    std::cout << "Ray origin: " << ray.origin_ << std::endl;
    std::cout << "Ray direction: " << ray.direction_ << std::endl;
  }
  // Calculate the intersection of the ray with the bounding box
  float t_min = (min_point_.x - ray.origin_.x) / ray.direction_.x;
  float t_max = (max_point_.x - ray.origin_.x) / ray.direction_.x;

  if (t_min > t_max) {
    std::swap(t_min, t_max);
  }

  float t_y_min = (min_point_.y - ray.origin_.y) / ray.direction_.y;
  float t_y_max = (max_point_.y - ray.origin_.y) / ray.direction_.y;

  if (t_y_min > t_y_max) {
    std::swap(t_y_min, t_y_max);
  }

  if ((t_min > t_y_max) || (t_y_min > t_max)) {
    return nullptr;
  }

  if (t_y_min > t_min) {
    t_min = t_y_min;
  }

  if (t_y_max < t_max) {
    t_max = t_y_max;
  }

  float t_z_min = (min_point_.z - ray.origin_.z) / ray.direction_.z;
  float t_z_max = (max_point_.z - ray.origin_.z) / ray.direction_.z;

  if (t_z_min > t_z_max) {
    std::swap(t_z_min, t_z_max);
  }

  if ((t_min > t_z_max) || (t_z_min > t_max)) {
    return nullptr;
  }

  if (t_z_min > t_min) {
    t_min = t_z_min;
  }

  if (t_z_max < t_max) {
    t_max = t_z_max;
  }

  // Check if the intersection is within the valid range
  if (t_max < 0) {
    return nullptr;
  }

  if (trace_ && std::find(trace_pixels_.begin(), trace_pixels_.end(),
                          ray.pixel_) != trace_pixels_.end()) {
    std::cout << "Intersects with node id: " << id_ << std::endl;
  }

  float left_t_hit, right_t_hit;
  Vec3f left_intersection_normal, right_intersection_normal;
  std::shared_ptr<BoundingVolumeHierarchyElement> left_intersection = nullptr,
                                                  right_intersection = nullptr;
  if (left_) {
    left_intersection =
        left_->Intersect(ray, left_t_hit, left_intersection_normal,
                         backface_culling, stop_at_any_hit);
  }
  if (right_) {
    right_intersection =
        right_->Intersect(ray, right_t_hit, right_intersection_normal,
                          backface_culling, stop_at_any_hit);
  }

  if (left_intersection && right_intersection) {
    if (left_t_hit < right_t_hit) {
      t_hit = left_t_hit;
      intersection_normal = left_intersection_normal;
      return left_intersection;
    } else {
      t_hit = right_t_hit;
      intersection_normal = right_intersection_normal;
      return right_intersection;
    }
  } else if (left_intersection) {
    t_hit = left_t_hit;
    intersection_normal = left_intersection_normal;
    return left_intersection;
  } else if (right_intersection) {
    t_hit = right_t_hit;
    intersection_normal = right_intersection_normal;
    return right_intersection;
  } else {
    return nullptr;
  }
}

std::shared_ptr<BoundingVolumeHierarchyElement>
BoundingVolumeHierarchyElement::Construct(
    std::vector<std::shared_ptr<BoundingVolumeHierarchyElement>>& leafs,
    int start, int end, int axis) {
  if (end - start == 0) {
    return nullptr;
  }
  if (end - start == 1) {
    return leafs[start];
  }

  std::sort(leafs.begin() + start, leafs.begin() + end,
            [axis](const std::shared_ptr<BoundingVolumeHierarchyElement>& a,
                   const std::shared_ptr<BoundingVolumeHierarchyElement>& b) {
              return ((a->max_point_ + a->min_point_)[axis]) <
                     ((b->max_point_ + b->min_point_)[axis]);
            });

  int mid = start + (end - start) / 2;
  std::shared_ptr<BoundingVolumeHierarchyElement> left =
      Construct(leafs, start, mid, (axis + 1) % 3);
  std::shared_ptr<BoundingVolumeHierarchyElement> right =
      Construct(leafs, mid, end, (axis + 1) % 3);

  std::shared_ptr<BoundingVolumeHierarchyElement> node =
      std::make_shared<BoundingVolumeHierarchyElement>();
  node->InitializeChildren(left, right);
  if (left && right) {
    node->min_point_ = {std::min(left->min_point_.x, right->min_point_.x),
                        std::min(left->min_point_.y, right->min_point_.y),
                        std::min(left->min_point_.z, right->min_point_.z)};
    node->max_point_ = {std::max(left->max_point_.x, right->max_point_.x),
                        std::max(left->max_point_.y, right->max_point_.y),
                        std::max(left->max_point_.z, right->max_point_.z)};
  } else if (left) {
    node->min_point_ = left->min_point_;
    node->max_point_ = left->max_point_;
  } else if (right) {
    node->min_point_ = right->min_point_;
    node->max_point_ = right->max_point_;
  }

  return node;
}

void BoundingVolumeHierarchyElement::PrintBVH(
    const std::shared_ptr<BoundingVolumeHierarchyElement>& root) {
  if (root) {
    std::cout << "Node id: " << root->id_ << std::endl;
    std::cout << "Min point: " << root->min_point_ << std::endl;
    std::cout << "Max point: " << root->max_point_ << std::endl;
    if (root->left_) {
      std::cout << "Left child id: " << root->left_->id_ << std::endl;
    }
    if (root->right_) {
      std::cout << "Right child id: " << root->right_->id_ << std::endl;
    }
    std::cout << std::endl;
    PrintBVH(root->left_);
    PrintBVH(root->right_);
  }
}