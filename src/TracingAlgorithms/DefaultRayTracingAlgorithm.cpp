#include "Scene.hpp"

Vec3f Scene::DefaultRayTracingAlgorithm(
    Ray& ray,
    const std::shared_ptr<BaseObject> inside_object_ptr,
    int, int) {
  return {0, 0, 0};
}