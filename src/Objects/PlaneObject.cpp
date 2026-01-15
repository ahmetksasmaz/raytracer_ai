#include "PlaneObject.hpp"

std::shared_ptr<BaseObject> PlaneObject::IntersectPlane(
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
            return std::const_pointer_cast<BaseObject>(this->shared_from_this());
        }

    }
    return nullptr;
}

void PlaneObject::Preprocess(bool high_level_bvh_enabled,
                              bool low_level_bvh_enabled, bool) {
}