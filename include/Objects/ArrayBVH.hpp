#pragma once

#include <vector>
#include <algorithm>
#include <memory>
#include <limits>
#include <stdexcept>
#include "../extern/parser.h"
#include "Ray.hpp"
#include "Helper.hpp"

using namespace parser;

class BaseObject;
struct alignas(32) ArrayBVHNode {
    Vec3f min_point;
    Vec3f max_point;
    int left_or_prim;
    int right_or_count;
    
    inline bool IsLeaf() const { return left_or_prim < 0; }
    inline int GetPrimitiveIndex() const { return ~left_or_prim; }
    inline int GetPrimitiveCount() const { return right_or_count; }
    inline int LeftChild() const { return left_or_prim; }
    inline int RightChild() const { return right_or_count; }
};
class ArrayBVH {
public:
    template<typename PrimitiveType>
    void BuildBVH(std::vector<std::shared_ptr<PrimitiveType>>& primitives);
    template<typename PrimitiveType>
    int Intersect(const Ray& ray, 
                  const std::vector<std::shared_ptr<PrimitiveType>>& primitives,
                  FP_PRECISION& t_hit,
                  Vec3f& hit_normal,
                  Vec2f& tex_coords,
                  Vec2f& hit_u_vector,
                  Vec2f& hit_v_vector,
                  Vec3f& tangent_vector,
                  Vec3f& bitangent_vector,
                  bool backface_culling = true,
                  bool stop_at_any_hit = false) const;
    
    bool IsEmpty() const { return nodes_.empty(); }
    size_t NodeCount() const { return nodes_.size(); }
    
private:
    std::vector<ArrayBVHNode> nodes_;
    std::vector<int> primitive_indices_;
        template<typename PrimitiveType>
    int BuildBVHRecursive(std::vector<std::shared_ptr<PrimitiveType>>& primitives,
                       std::vector<int>& indices,
                       int start, int end, int axis);
};

inline bool FastBVIntersect(const Vec3f& min_point, const Vec3f& max_point,
                                   const Vec3f& origin, const Vec3f& inverse_direction,
                                   FP_PRECISION t_max) {
    FP_PRECISION t1 = (min_point.x - origin.x) * inverse_direction.x;
    FP_PRECISION t2 = (max_point.x - origin.x) * inverse_direction.x;
    
    FP_PRECISION tmin = std::min(t1, t2);
    FP_PRECISION tmax = std::max(t1, t2);
    
    t1 = (min_point.y - origin.y) * inverse_direction.y;
    t2 = (max_point.y - origin.y) * inverse_direction.y;
    
    tmin = std::max(tmin, std::min(t1, t2));
    tmax = std::min(tmax, std::max(t1, t2));
    
    t1 = (min_point.z - origin.z) * inverse_direction.z;
    t2 = (max_point.z - origin.z) * inverse_direction.z;
    
    tmin = std::max(tmin, std::min(t1, t2));
    tmax = std::min(tmax, std::max(t1, t2));
    
    return tmax >= std::max(tmin, static_cast<FP_PRECISION>(0.0)) && tmin < t_max;
}

template<typename PrimitiveType>
void ArrayBVH::BuildBVH(std::vector<std::shared_ptr<PrimitiveType>>& primitives) {
    if (primitives.empty()) return;
    nodes_.reserve(2 * primitives.size());
    primitive_indices_.reserve(primitives.size());
    
    std::vector<int> indices(primitives.size());
    for (size_t i = 0; i < primitives.size(); i++) {
        indices[i] = static_cast<int>(i);
    }
    
    BuildBVHRecursive(primitives, indices, 0, static_cast<int>(primitives.size()), 0);
}

template<typename PrimitiveType>
int ArrayBVH::BuildBVHRecursive(std::vector<std::shared_ptr<PrimitiveType>>& primitives,
                            std::vector<int>& indices,
                            int start, int end, int axis) {
    int node_index = static_cast<int>(nodes_.size());
    nodes_.push_back(ArrayBVHNode{});
    ArrayBVHNode& node = nodes_[node_index];
    
    node.min_point = {std::numeric_limits<FP_PRECISION>::max(),
                      std::numeric_limits<FP_PRECISION>::max(),
                      std::numeric_limits<FP_PRECISION>::max()};
    node.max_point = {std::numeric_limits<FP_PRECISION>::lowest(),
                      std::numeric_limits<FP_PRECISION>::lowest(),
                      std::numeric_limits<FP_PRECISION>::lowest()};
    
    for (int i = start; i < end; i++) {
        const auto& prim = primitives[indices[i]];
        node.min_point.x = std::min(node.min_point.x, prim->min_point_.x);
        node.min_point.y = std::min(node.min_point.y, prim->min_point_.y);
        node.min_point.z = std::min(node.min_point.z, prim->min_point_.z);
        node.max_point.x = std::max(node.max_point.x, prim->max_point_.x);
        node.max_point.y = std::max(node.max_point.y, prim->max_point_.y);
        node.max_point.z = std::max(node.max_point.z, prim->max_point_.z);
    }
    
    int count = end - start;
        if (count <= 4) {
        // Create leaf node
        int primary_start = static_cast<int>(primitive_indices_.size());
        for (int i = start; i < end; i++) {
            primitive_indices_.push_back(indices[i]);
        }
        node.left_or_prim = ~primary_start;
        node.right_or_count = count;
        return node_index;
    }
    
    std::sort(indices.begin() + start, indices.begin() + end,
              [&primitives, axis](int a, int b) {
                  FP_PRECISION ca = (primitives[a]->min_point_[axis] + primitives[a]->max_point_[axis]);
                  FP_PRECISION cb = (primitives[b]->min_point_[axis] + primitives[b]->max_point_[axis]);
                  return ca < cb;
              });
    
    int mid = start + count / 2;
    
    int left_child = BuildBVHRecursive(primitives, indices, start, mid, (axis + 1) % 3);
    int right_child = BuildBVHRecursive(primitives, indices, mid, end, (axis + 1) % 3);
    
    nodes_[node_index].left_or_prim = left_child;
    nodes_[node_index].right_or_count = right_child;
    
    return node_index;
}

template<typename PrimitiveType>
int ArrayBVH::Intersect(const Ray& ray,
                       const std::vector<std::shared_ptr<PrimitiveType>>& primitives,
                       FP_PRECISION& t_hit,
                       Vec3f& hit_normal,
                       Vec2f& tex_coords,
                       Vec2f& hit_u_vector,
                       Vec2f& hit_v_vector,
                       Vec3f& tangent_vector,
                       Vec3f& bitangent_vector,
                       bool backface_culling,
                       bool stop_at_any_hit) const {
    if (nodes_.empty()) return -1;
    
    if (!stop_at_any_hit) {
        t_hit = std::numeric_limits<FP_PRECISION>::max();
    }
    int hit_primitive = -1;
    
    static constexpr int STACK_SIZE = 128;
    int stack[STACK_SIZE];
    int stack_pointer = 0;
    stack[stack_pointer++] = 0;
    
    const Vec3f& origin = ray.origin_;
    const Vec3f& inverse_direction = ray.inverse_direction_;
    
    while (stack_pointer > 0) {
        int node_index = stack[--stack_pointer];
        const ArrayBVHNode& node = nodes_[node_index];
        
        if (!FastBVIntersect(node.min_point, node.max_point, origin, inverse_direction, t_hit)) {
            continue;
        }
        
        if (node.IsLeaf()) {
            // Test primitives in leaf
            int primary_start = node.GetPrimitiveIndex();
            int primCount = node.GetPrimitiveCount();
            
            for (int i = 0; i < primCount; i++) {
                int primary_index = primitive_indices_[primary_start + i];
                FP_PRECISION temp_t = std::numeric_limits<FP_PRECISION>::max();
                Vec3f temp_normal;
                Vec2f temp_tex_coords, temp_u, temp_v;
                Vec3f temp_tangent, temp_bitangent;
                
                if (primitives[primary_index]->Intersect(ray, temp_t, temp_normal, temp_tex_coords,
                                                    temp_u, temp_v, temp_tangent, temp_bitangent,
                                                    backface_culling, stop_at_any_hit)) {
                    if (temp_t < t_hit) {
                        t_hit = temp_t;
                        hit_normal = temp_normal;
                        tex_coords = temp_tex_coords;
                        hit_u_vector = temp_u;
                        hit_v_vector = temp_v;
                        tangent_vector = temp_tangent;
                        bitangent_vector = temp_bitangent;
                        hit_primitive = primary_index;
                        
                        if (stop_at_any_hit) {
                            return hit_primitive;
                        }
                    }
                }
            }
        } else {
            // Median splitting keeps depth at about log2(N), so 128 is far more
            // than any real scene needs. Silently dropping both children on
            // overflow would corrupt every subsequent intersection instead of
            // reporting a problem, so fail loudly if it ever happens.
            if (stack_pointer + 2 > STACK_SIZE) {
                throw std::runtime_error(
                    "ArrayBVH: traversal stack overflow -- BVH is deeper than "
                    "STACK_SIZE, intersections would be silently missed");
            }
            stack[stack_pointer++] = node.RightChild();
            stack[stack_pointer++] = node.LeftChild();
        }
    }
    
    return hit_primitive;
}