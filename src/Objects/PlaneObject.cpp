#include "PlaneObject.hpp"

bool PlaneObject::Intersect(
    Ray& ray, FP_PRECISION& t_hit, Vec3f& intersection_normal, Vec2f& tex_coords, Vec2f& hit_u_vector, Vec2f& hit_v_vector, Vec3f& tangent_vector, Vec3f& bitangent_vector,
    bool backface_culling, bool stop_at_any_hit) const {
    return IntersectPlane(ray, t_hit, intersection_normal, backface_culling, stop_at_any_hit);
}

bool PlaneObject::IntersectPlane(
    Ray& ray, FP_PRECISION& t_hit, Vec3f& intersection_normal, bool, bool) const {

        
    // Calculate the cos value between the ray and the plane normal
    FP_PRECISION denom = dot(ray.direction_, normal_);
    // Check if the ray intersects with the sphere
    // (P-P0).N = 0
    // Ray: P = O + tD
    // (O + tD - P0).N = 0
    // t = -(O.N + P0.N) / (D.N)
    if (abs(denom) > 1e-5) {
        // Calculate the intersection point
        FP_PRECISION t = dot(point_ - ray.origin_, normal_) / denom;
        if (t > 1e-5) {
            t_hit = t;
            intersection_normal = normal_;
            return true;
        }

    }
    return false;
}

void PlaneObject::Preprocess(bool) {
}