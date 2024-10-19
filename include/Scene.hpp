#pragma once
#include <functional>
#include <iostream>
#include <memory>

#include "../extern/parser.h"
#include "AmbientLightSource.hpp"
#include "BaseCamera.hpp"
#include "BaseExporter.hpp"
#include "BaseMaterial.hpp"
#include "BaseObject.hpp"
#include "ConductorMaterial.hpp"
#include "DielectricMaterial.hpp"
#include "MeshObject.hpp"
#include "MirrorMaterial.hpp"
#include "PPMExporter.hpp"
#include "PointLightSource.hpp"
#include "STBExporter.hpp"
#include "SphereObject.hpp"
#include "TriangleObject.hpp"

using namespace parser;

enum class RayTracingAlgorithm {
  kDefault = 0,
  kRecursive = 1,
  kBest = 1,
  kMax = 2
};

enum class SchedulingAlgorithm { kNonThread = 0, kBest = 0, kMax = 0 };

enum class ToneMappingAlgorithm { kClamp = 0, kBest = 0, kMax = 0 };

enum class ExporterType { kPPM = 0, kSTB = 1, kBest = 1, kMax = 1 };

class Scene {
 public:
  Scene(const std::string& filename,
        const RayTracingAlgorithm ray_tracing_algorithm,
        const SchedulingAlgorithm scheduling_algorithm,
        const ToneMappingAlgorithm tone_mapping_algorithm,
        const ExporterType exporter_type);
  void Render();
  ~Scene();

 private:
  void LoadScene();
  void PreprocessScene();

  const std::string filename_;

  Vec3i background_color_;
  float shadow_ray_epsilon_;
  int max_recursion_depth_;

  std::vector<std::shared_ptr<BaseCamera>> cameras_;
  std::vector<std::shared_ptr<PointLightSource>> point_lights_;
  std::vector<std::shared_ptr<AmbientLightSource>> ambient_lights_;
  std::vector<std::shared_ptr<BaseMaterial>> materials_;
  std::vector<std::shared_ptr<BaseObject>> objects_;

  std::function<void(const std::shared_ptr<BaseCamera>)> scheduling_algorithm_;
  std::function<Vec3f(const Ray&, const std::shared_ptr<BaseObject>, int, int)>
      ray_tracing_algorithm_;
  std::function<void(const std::vector<Vec3f>&, std::vector<unsigned char>&)>
      tone_mapping_algorithm_;

  std::shared_ptr<BaseExporter> exporter_;

  Vec3f DefaultRayTracingAlgorithm(
      const Ray& ray, const std::shared_ptr<BaseObject> inside_object_ptr, int,
      int);
  Vec3f RecursiveRayTracingAlgorithm(
      const Ray& ray, const std::shared_ptr<BaseObject> inside_object_ptr,
      int remaining_recursion, int max_recursion);

  void NonThreadSchedulingAlgorithm(const std::shared_ptr<BaseCamera> camera);

  void ClampToneMappingAlgorithm(const std::vector<Vec3f>&,
                                 std::vector<unsigned char>&);
};