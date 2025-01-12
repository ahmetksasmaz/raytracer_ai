#pragma once
#include <functional>
#include <iostream>
#include <memory>

#include "../extern/parser.h"
#include "AmbientLightSource.hpp"
#include "AreaLightSource.hpp"
#include "BaseCamera.hpp"
#include "BaseExporter.hpp"
#include "BaseMaterial.hpp"
#include "BaseObject.hpp"
#include "ConductorMaterial.hpp"
#include "Configuration.hpp"
#include "DielectricMaterial.hpp"
#include "MeshInstanceObject.hpp"
#include "MeshObject.hpp"
#include "MirrorMaterial.hpp"
#include "PPMExporter.hpp"
#include "PointLightSource.hpp"
#include "STBExporter.hpp"
#include "SphereObject.hpp"
#include "TriangleObject.hpp"

using namespace parser;

class Scene {
 public:
  Scene(const std::string &filename, const Configuration &configuration);
  void Render();
  ~Scene();

 private:
  void LoadScene();
  void PreprocessScene();

  const std::string filename_;
  const Configuration configuration_;

  Vec3i background_color_;
  float shadow_ray_epsilon_;
  int max_recursion_depth_;

  std::vector<std::shared_ptr<BaseSpectrum>> spectrums_;
  std::vector<std::shared_ptr<BaseCamera>> cameras_;
  std::vector<std::shared_ptr<PointLightSource>> point_lights_;
  std::vector<std::shared_ptr<AreaLightSource>> area_lights_;
  std::vector<std::shared_ptr<AmbientLightSource>> ambient_lights_;
  std::vector<std::shared_ptr<BaseMaterial>> materials_;
  std::vector<std::shared_ptr<BoundingVolumeHierarchyElement>> objects_;

  std::shared_ptr<BoundingVolumeHierarchyElement> bvh_root_ = nullptr;

  std::function<void(const std::shared_ptr<BaseCamera>, int)>
      scheduling_algorithm_;
  std::function<Vec3f(Ray &, const std::shared_ptr<BaseObject>, int, int)>
      ray_tracing_algorithm_;
  std::function<void(Vec5f *, int, int, int, Vec3f *)> filtering_algorithm_;
  std::function<void(Vec3f *, int, int, std::vector<unsigned char> &)>
      tone_mapping_algorithm_;

  std::function<std::vector<Vec2f>(int)> area_light_sampling_algorithm_;

  std::shared_ptr<BaseExporter> exporter_;

  Vec3f DefaultRayTracingAlgorithm(
      Ray &ray,
      const std::shared_ptr<BoundingVolumeHierarchyElement> inside_object_ptr,
      int, int);
  Vec3f RecursiveRayTracingAlgorithm(
      Ray &ray,
      const std::shared_ptr<BoundingVolumeHierarchyElement> inside_object_ptr,
      int remaining_recursion, int max_recursion);

  void NonThreadSchedulingAlgorithm(const std::shared_ptr<BaseCamera> camera,
                                    int camera_index);
  void ThreadQueueSchedulingAlgorithm(const std::shared_ptr<BaseCamera> camera,
                                      int camera_index);

  void AveragingFilterAlgorithm(Vec5f *image_sampled_data, int image_width,
                                int image_height, int sample,
                                Vec3f *image_data);
  void GaussianFilterAlgorithm(Vec5f *image_sampled_data, int image_width,
                               int image_height, int sample, Vec3f *image_data);
  void ExtendedGaussianFilterAlgorithm(Vec5f *image_sampled_data,
                                       int image_width, int image_height,
                                       int sample, Vec3f *image_data);

  void ClampToneMappingAlgorithm(Vec3f *, int, int,
                                 std::vector<unsigned char> &);
};