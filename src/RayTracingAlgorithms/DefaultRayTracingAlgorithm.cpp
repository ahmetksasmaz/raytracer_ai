#include "Scene.hpp"

std::map<int, float> Scene::DefaultRayTracingAlgorithm(
    Ray& ray,
    const std::shared_ptr<BoundingVolumeHierarchyElement> inside_object_ptr,
    int, int) {
  return {};
}