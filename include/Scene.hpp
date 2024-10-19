#pragma once
#include <iostream>
#include <memory>
#include <unordered_map>

#include "../extern/parser.h"
#include "AmbientLightSource.hpp"
#include "BaseCamera.hpp"
#include "BaseMaterial.hpp"
#include "BaseObject.hpp"
#include "ConductorMaterial.hpp"
#include "DielectricMaterial.hpp"
#include "MeshObject.hpp"
#include "MirrorMaterial.hpp"
#include "PointLightSource.hpp"
#include "SphereObject.hpp"
#include "TriangleObject.hpp"

using namespace parser;

class Scene {
 public:
  Scene(const std::string& filename);
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
};