#include "Scene.hpp"

Scene::Scene(const std::string &filename,
             const RayTracingAlgorithm ray_tracing_algorithm,
             const SchedulingAlgorithm scheduling_algorithm,
             const ToneMappingAlgorithm tone_mapping_algorithm,
             const ExporterType exporter_type)
    : filename_(filename) {
  switch (exporter_type) {
    case ExporterType::kPPM:
      exporter_ = std::make_shared<PPMExporter>();
      break;
    case ExporterType::kSTB:
      exporter_ = std::make_shared<STBExporter>();
      break;
  }

  switch (ray_tracing_algorithm) {
    case RayTracingAlgorithm::kDefault:
      ray_tracing_algorithm_ = std::bind(
          &Scene::DefaultRayTracingAlgorithm, this, std::placeholders::_1,
          std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
      break;
    case RayTracingAlgorithm::kRecursive:
      ray_tracing_algorithm_ = std::bind(
          &Scene::RecursiveRayTracingAlgorithm, this, std::placeholders::_1,
          std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
      break;
  }

  switch (scheduling_algorithm) {
    case SchedulingAlgorithm::kNonThread:
      scheduling_algorithm_ = std::bind(&Scene::NonThreadSchedulingAlgorithm,
                                        this, std::placeholders::_1);
      break;
  }

  switch (tone_mapping_algorithm) {
    case ToneMappingAlgorithm::kClamp:
      tone_mapping_algorithm_ =
          std::bind(&Scene::ClampToneMappingAlgorithm, this,
                    std::placeholders::_1, std::placeholders::_2);
      break;
  }

  LoadScene();
  PreprocessScene();
}

Scene::~Scene() {
  cameras_.clear();
  point_lights_.clear();
  ambient_lights_.clear();
  materials_.clear();
  objects_.clear();
}

void Scene::LoadScene() {
  RawScene raw_scene;
  raw_scene.loadFromXml(filename_);

  background_color_ = raw_scene.background_color;
  shadow_ray_epsilon_ = raw_scene.shadow_ray_epsilon;
  max_recursion_depth_ = raw_scene.max_recursion_depth;

  ambient_lights_.push_back(
      std::make_shared<AmbientLightSource>(raw_scene.ambient_light));

  for (const auto &raw_point_light : raw_scene.point_lights) {
    point_lights_.push_back(std::make_shared<PointLightSource>(
        raw_point_light.position, raw_point_light.intensity));
  }

  for (const auto &raw_camera : raw_scene.cameras) {
    cameras_.push_back(std::make_shared<BaseCamera>(
        raw_camera.position, raw_camera.gaze, raw_camera.up,
        raw_camera.near_plane, raw_camera.near_distance, raw_camera.image_width,
        raw_camera.image_height, raw_camera.image_name));
  }

  for (const auto &raw_material : raw_scene.materials) {
    switch (raw_material.material_type) {
      case RawMaterialType::kDefault:
        materials_.push_back(std::make_shared<BaseMaterial>(
            raw_material.ambient, raw_material.diffuse, raw_material.specular,
            raw_material.phong_exponent));
        break;

      case RawMaterialType::kMirror:
        materials_.push_back(std::make_shared<MirrorMaterial>(
            raw_material.ambient, raw_material.diffuse, raw_material.specular,
            raw_material.phong_exponent, raw_material.mirror));
        break;
      case RawMaterialType::kConductor:
        materials_.push_back(std::make_shared<ConductorMaterial>(
            raw_material.ambient, raw_material.diffuse, raw_material.specular,
            raw_material.phong_exponent, raw_material.mirror,
            raw_material.refraction_index, raw_material.absorption_index));
        break;
      case RawMaterialType::kDielectric:
        materials_.push_back(std::make_shared<DielectricMaterial>(
            raw_material.ambient, raw_material.diffuse, raw_material.specular,
            raw_material.phong_exponent, raw_material.mirror,
            raw_material.absorption_coefficient,
            raw_material.refraction_index));
        break;
    }
  }

  for (const auto &raw_sphere : raw_scene.spheres) {
    objects_.push_back(std::make_shared<SphereObject>(
        materials_[raw_sphere.material_id - 1],
        raw_scene.vertex_data[raw_sphere.center_vertex_id - 1],
        raw_sphere.radius));
  }

  for (const auto &raw_triangle : raw_scene.triangles) {
    objects_.push_back(std::make_shared<TriangleObject>(
        materials_[raw_triangle.material_id - 1],
        raw_scene.vertex_data[raw_triangle.indices.v0_id - 1],
        raw_scene.vertex_data[raw_triangle.indices.v1_id - 1],
        raw_scene.vertex_data[raw_triangle.indices.v2_id - 1]));
  }

  for (const auto &raw_mesh : raw_scene.meshes) {
    objects_.push_back(
        std::make_shared<MeshObject>(materials_[raw_mesh.material_id - 1],
                                     raw_mesh.faces, raw_scene.vertex_data));
  }
}

void Scene::PreprocessScene() {
  for (const auto &object : objects_) {
    object->Preprocess();
  }
}

void Scene::Render() {
  for (const auto &camera : cameras_) {
    scheduling_algorithm_(camera);
    tone_mapping_algorithm_(camera->GetImageDataReference(),
                            camera->GetTonemappedImageDataReference());
    camera->ExportView(exporter_);
  }
}