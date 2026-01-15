#pragma once
#include <functional>
#include <iostream>
#include <memory>
#include <algorithm>
#include <string>
#include <vector>
#include <cctype>

#include "../extern/parser.h"
#include "AmbientLightSource.hpp"
#include "AreaLightSource.hpp"
#include "BaseCamera.hpp"
#include "BaseImage.hpp"
#include "BaseMaterial.hpp"
#include "BaseObject.hpp"
#include "BaseTextureMap.hpp"
#include "ConductorMaterial.hpp"
#include "Configuration.hpp"
#include "DielectricMaterial.hpp"
#include "MeshInstanceObject.hpp"
#include "MeshObject.hpp"
#include "MirrorMaterial.hpp"
#include "PointLightSource.hpp"
#include "DirectionalLightSource.hpp"
#include "SpotLightSource.hpp"
#include "SphericalDirectionalLightSource.hpp"
#include "STBExporter.hpp"
#include "SphereObject.hpp"
#include "TriangleObject.hpp"
#include "PlaneObject.hpp"
#include "CheckerboardTextureMap.hpp"
#include "PerlinTextureMap.hpp"
#include "ImageTextureMap.hpp"
#include "PhotographicToneMapping.hpp"
#include "FilmicToneMapping.hpp"
#include "ACESToneMapping.hpp"
#include "BaseBRDF.hpp"
#include "OriginalBlinnPhong.hpp"
#include "OriginalPhong.hpp"
#include "ModifiedBlinnPhong.hpp"
#include "ModifiedPhong.hpp"
#include "TorranceSparrow.hpp"

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
  std::shared_ptr<BaseTextureMap> background_texture_map_ = nullptr;
  FP_PRECISION shadow_ray_epsilon_;
  int max_recursion_depth_;

  std::vector<std::shared_ptr<BaseCamera>> cameras_;
  std::vector<std::shared_ptr<PointLightSource>> point_lights_;
  std::vector<std::shared_ptr<AreaLightSource>> area_lights_;
  std::shared_ptr<AmbientLightSource> ambient_light_;
  std::vector<std::shared_ptr<DirectionalLightSource>> directional_lights_;
  std::vector<std::shared_ptr<SpotLightSource>> spot_lights_;
  std::shared_ptr<SphericalDirectionalLightSource> spherical_directional_light_;
  std::vector<std::shared_ptr<BaseBRDF>> brdfs_;
  std::vector<std::shared_ptr<BaseMaterial>> materials_;
  std::vector<std::shared_ptr<BoundingVolumeHierarchyElement>> objects_;
  std::vector<std::shared_ptr<PlaneObject>> plane_objects_;

  std::vector<std::shared_ptr<BaseImage>> images_;
  std::vector<std::shared_ptr<BaseTextureMap>> texture_maps_;

  std::shared_ptr<BoundingVolumeHierarchyElement> bvh_root_ = nullptr;

  std::function<void(const std::shared_ptr<BaseCamera>, int)>
      scheduling_algorithm_;
  std::function<Vec3f(Ray &, const std::shared_ptr<BaseObject>, int, int)>
      ray_tracing_algorithm_;
  std::function<void(Vec5f *, int, int, int, Vec3f *)> filtering_algorithm_;
  std::function<void(Vec3f *, int, int, std::vector<unsigned char> &)>
      tone_mapping_algorithm_;

  std::function<std::vector<Vec2f>(int)> area_light_sampling_algorithm_;

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

  void BlockDivideThreadSchedulingAlgorithm(
      const std::shared_ptr<BaseCamera> camera, int camera_index);

  void SlidingThreadSchedulingAlgorithm(const std::shared_ptr<BaseCamera> camera,
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
};