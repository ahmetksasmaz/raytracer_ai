#pragma once
#include <iostream>
#include <memory>

#include "../extern/parser.h"
#include "AmbientLightSource.hpp"
#include "BaseCamera.hpp"
#include "BaseMaterial.hpp"
#include "BaseObject.hpp"
#include "PointLightSource.hpp"

using namespace parser;

class Scene {
 public:
  Scene(std::string filename);
  void Render();
  ~Scene();

 private:
  void LoadScene();
  void PreprocessScene();

  std::string filename_;

  RawScene raw_scene_;
  Vec3i background_color_;
  float shadow_ray_epsilon_;
  int max_recursion_depth_;

  std::vector<std::shared_ptr<BaseCamera>> cameras_;
  std::vector<std::shared_ptr<PointLightSource>> point_lights_;
  std::vector<std::shared_ptr<AmbientLightSource>> ambient_lights_;
  std::vector<std::shared_ptr<BaseMaterial>> materials_;
  std::vector<std::shared_ptr<BaseObject>> objects_;
};