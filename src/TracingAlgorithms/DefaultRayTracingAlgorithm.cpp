#include "Scene.hpp"

Spectrum Scene::DefaultRayTracingAlgorithm(
    Ray& ray,
    const std::shared_ptr<BaseObject> inside_object_ptr,
    int, int) {
  return Spectrum();
}