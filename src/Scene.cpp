#include "Scene.hpp"

#include "Timer.hpp"

Scene::Scene(const std::string &filename, const Configuration &configuration)
    : filename_(filename), configuration_(configuration)
{
  switch (configuration_.strategies_.exporter_type_)
  {
  case ExporterType::kPPM:
    exporter_ = std::make_shared<PPMExporter>();
    break;
  case ExporterType::kSTB:
    exporter_ = std::make_shared<STBExporter>();
    break;
  }

  switch (configuration_.strategies_.ray_tracing_algorithm_)
  {
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

  switch (configuration_.strategies_.scheduling_algorithm_)
  {
  case SchedulingAlgorithm::kNonThread:
    scheduling_algorithm_ =
        std::bind(&Scene::NonThreadSchedulingAlgorithm, this,
                  std::placeholders::_1, std::placeholders::_2);
    break;
  case SchedulingAlgorithm::kThreadQueue:
    scheduling_algorithm_ =
        std::bind(&Scene::ThreadQueueSchedulingAlgorithm, this,
                  std::placeholders::_1, std::placeholders::_2);
    break;
  }

  switch (configuration_.sampling_.area_light_sampling_)
  {
  case SamplingAlgorithm::kRandom:
    area_light_sampling_algorithm_ = uniform_random_2d;
    break;
  }

  switch (configuration_.sampling_.pixel_filtering_)
  {
  case FilteringAlgorithm::kBox:
    filtering_algorithm_ =
        std::bind(&Scene::AveragingFilterAlgorithm, this,
                  std::placeholders::_1, std::placeholders::_2);
    break;
  case FilteringAlgorithm::kGaussian:
    filtering_algorithm_ =
        std::bind(&Scene::GaussianFilterAlgorithm, this,
                  std::placeholders::_1, std::placeholders::_2);
    break;

  case FilteringAlgorithm::kExtendedGaussian:
    filtering_algorithm_ =
        std::bind(&Scene::ExtendedGaussianFilterAlgorithm, this,
                  std::placeholders::_1, std::placeholders::_2);
    break;
  }

  switch (configuration_.strategies_.tone_mapping_algorithm_)
  {
  case ToneMappingAlgorithm::kClamp:
    tone_mapping_algorithm_ =
        std::bind(&Scene::ClampToneMappingAlgorithm, this,
                  std::placeholders::_1, std::placeholders::_2);
    break;
  }

#ifdef DEBUG
  std::cout << "Loading scene." << std::endl;
#endif
  LoadScene();
#ifdef DEBUG
  std::cout << "Loading done." << std::endl;
  std::cout << "Preprocessing scene." << std::endl;
#endif
  if (timer.configuration_.timer_.preprocess_scene_)
    timer.AddTimeLog(Section::kPreprocessScene, Event::kStart);
  PreprocessScene();
  if (timer.configuration_.timer_.preprocess_scene_)
    timer.AddTimeLog(Section::kPreprocessScene, Event::kEnd);
#ifdef DEBUG
  std::cout << "Preprocessing done." << std::endl;
#endif
}

Scene::~Scene()
{
  cameras_.clear();
  point_lights_.clear();
  ambient_lights_.clear();
  materials_.clear();
  objects_.clear();
}

void Scene::LoadScene()
{
  RawScene raw_scene;
#ifdef DEBUG
  std::cout << "\tLoading scene from " << filename_ << std::endl;
#endif
  if (timer.configuration_.timer_.parse_xml_)
    timer.AddTimeLog(Section::kParseXML, Event::kStart);
  raw_scene.loadFromXml(filename_);
  if (timer.configuration_.timer_.parse_xml_)
    timer.AddTimeLog(Section::kParseXML, Event::kEnd);
#ifdef DEBUG
  std::cout << "\tLoading xml is done." << std::endl;
#endif

  if (timer.configuration_.timer_.load_scene_)
    timer.AddTimeLog(Section::kLoadScene, Event::kStart);

  background_color_ = raw_scene.background_color;
  shadow_ray_epsilon_ = raw_scene.shadow_ray_epsilon;
  max_recursion_depth_ = raw_scene.max_recursion_depth;

#ifdef DEBUG
  std::cout << "\tLoading ambient lights." << std::endl;
#endif
  ambient_lights_.push_back(
      std::make_shared<AmbientLightSource>(raw_scene.ambient_light));

#ifdef DEBUG
  std::cout << "\tLoading point lights." << std::endl;
#endif
  for (const auto &raw_point_light : raw_scene.point_lights)
  {
    point_lights_.push_back(std::make_shared<PointLightSource>(
        raw_point_light.position, raw_point_light.intensity));
  }
#ifdef DEBUG
  std::cout << "\tLoading area lights." << std::endl;
#endif
  for (const auto &raw_area_light : raw_scene.area_lights)
  {
    area_lights_.push_back(std::make_shared<AreaLightSource>(
        raw_area_light.position, raw_area_light.radiance, raw_area_light.normal, raw_area_light.size));
  }
#ifdef DEBUG
  std::cout << "\tLoading cameras." << std::endl;
#endif
  for (const auto &raw_camera : raw_scene.cameras)
  {
    cameras_.push_back(std::make_shared<BaseCamera>(
        raw_camera.position, raw_camera.gaze, raw_camera.up,
        raw_camera.near_plane, raw_camera.near_distance, raw_camera.image_width,
        raw_camera.image_height, raw_camera.image_name, raw_camera.num_samples,
        configuration_.sampling_.time_sampling_,
        configuration_.sampling_.pixel_sampling_, raw_camera.focus_distance,
        raw_camera.aperture_size, configuration_.sampling_.aperture_sampling_,
        configuration_.sampling_.aperture_type_));
  }

#ifdef DEBUG
  std::cout << "\tLoading materials." << std::endl;
#endif
  for (const auto &raw_material : raw_scene.materials)
  {
    switch (raw_material.material_type)
    {
    case RawMaterialType::kDefault:
      materials_.push_back(std::make_shared<BaseMaterial>(
          raw_material.ambient, raw_material.diffuse, raw_material.specular,
          raw_material.phong_exponent, raw_material.roughness));
      break;

    case RawMaterialType::kMirror:
      materials_.push_back(std::make_shared<MirrorMaterial>(
          raw_material.ambient, raw_material.diffuse, raw_material.specular,
          raw_material.phong_exponent, raw_material.roughness,
          raw_material.mirror));
      break;
    case RawMaterialType::kConductor:
      materials_.push_back(std::make_shared<ConductorMaterial>(
          raw_material.ambient, raw_material.diffuse, raw_material.specular,
          raw_material.phong_exponent, raw_material.roughness,
          raw_material.mirror, raw_material.refraction_index,
          raw_material.absorption_index));
      break;
    case RawMaterialType::kDielectric:
      materials_.push_back(std::make_shared<DielectricMaterial>(
          raw_material.ambient, raw_material.diffuse, raw_material.specular,
          raw_material.phong_exponent, raw_material.roughness,
          raw_material.mirror, raw_material.absorption_coefficient,
          raw_material.refraction_index));
      break;
    }
  }

#ifdef DEBUG
  std::cout << "\tLoading spheres." << std::endl;
#endif
  for (const auto &raw_sphere : raw_scene.spheres)
  {
    RawScalingFlip scaling_flip{false, false, false};
    Mat4x4f transform_matrix = parse_transformation(
        raw_sphere.transformations, scaling_flip, raw_scene.translations,
        raw_scene.scalings, raw_scene.rotations, raw_scene.composites);
    objects_.push_back(
        std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
            std::make_shared<SphereObject>(
                materials_[raw_sphere.material_id - 1],
                raw_scene.vertex_data[raw_sphere.center_vertex_id - 1],
                raw_sphere.radius, raw_sphere.motion_blur, transform_matrix,
                scaling_flip)));
  }
#ifdef DEBUG
  std::cout << "\tLoading triangles." << std::endl;
#endif
  for (const auto &raw_triangle : raw_scene.triangles)
  {
    RawScalingFlip scaling_flip{false, false, false};
    Mat4x4f transform_matrix = parse_transformation(
        raw_triangle.transformations, scaling_flip, raw_scene.translations,
        raw_scene.scalings, raw_scene.rotations, raw_scene.composites);
    objects_.push_back(
        std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
            std::make_shared<TriangleObject>(
                materials_[raw_triangle.material_id - 1],
                raw_scene.vertex_data[raw_triangle.indices.v0_id - 1],
                raw_scene.vertex_data[raw_triangle.indices.v1_id - 1],
                raw_scene.vertex_data[raw_triangle.indices.v2_id - 1],
                raw_triangle.motion_blur, transform_matrix, scaling_flip)));
  }

#ifdef DEBUG
  std::cout << "\tLoading meshes." << std::endl;
#endif

  int meshes_start_index = objects_.size();
  for (const auto &raw_mesh : raw_scene.meshes)
  {
    RawScalingFlip scaling_flip{false, false, false};
    Mat4x4f transform_matrix = parse_transformation(
        raw_mesh.transformations, scaling_flip, raw_scene.translations,
        raw_scene.scalings, raw_scene.rotations, raw_scene.composites);
    if (raw_mesh.ply_filepath != "")
    {
      objects_.push_back(
          std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
              std::make_shared<MeshObject>(
                  materials_[raw_mesh.material_id - 1], raw_mesh.ply_filepath,
                  raw_mesh.motion_blur, transform_matrix, scaling_flip)));
    }
    else
    {
      objects_.push_back(
          std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
              std::make_shared<MeshObject>(
                  materials_[raw_mesh.material_id - 1], raw_mesh.faces,
                  raw_scene.vertex_data, raw_mesh.motion_blur, transform_matrix,
                  scaling_flip)));
    }
  }
  // exit(1);

#ifdef DEBUG
  std::cout << "\tLoading mesh instances." << std::endl;
#endif

  for (auto &raw_mesh_instance : raw_scene.mesh_instances)
  {
    RawScalingFlip scaling_flip{false, false, false};
    Mat4x4f transform_matrix = IDENTITY_MATRIX;

    std::shared_ptr<MeshObject> mesh_object = nullptr;
    std::shared_ptr<BaseMaterial> material = nullptr;

    // Search for the root mesh objects
    auto current_raw_mesh = raw_mesh_instance;
    bool any_reset = false;

    if (current_raw_mesh.reset_transform)
    {
      transform_matrix =
          parse_transformation(current_raw_mesh.transformations, scaling_flip,
                               raw_scene.translations, raw_scene.scalings,
                               raw_scene.rotations, raw_scene.composites);
      any_reset = true;
    }

    // std::cout << "Mesh instance object id " << raw_mesh_instance.object_id
    //           << " base object id " << raw_mesh_instance.base_object_id
    //           << std::endl;
    // std::cout << transform_matrix << std::endl;

    do
    {
      if (!any_reset)
      {
        transform_matrix =
            transform_matrix *
            parse_transformation(current_raw_mesh.transformations, scaling_flip,
                                 raw_scene.translations, raw_scene.scalings,
                                 raw_scene.rotations, raw_scene.composites);
        // std::cout << "Multiplying transform matrix with : "
        //           << current_raw_mesh.object_id << std::endl;
        // std::cout << transform_matrix << std::endl;
      }

      any_reset = any_reset || current_raw_mesh.reset_transform;

      int base_object_id = current_raw_mesh.base_object_id;

      int counter = 0;
      for (auto &temp_raw_mesh : raw_scene.meshes)
      {
        if (temp_raw_mesh.object_id == base_object_id)
        {
          mesh_object = std::dynamic_pointer_cast<MeshObject>(
              objects_[meshes_start_index + counter]);
          // std::cout << "Mesh instance object id " <<
          // raw_mesh_instance.object_id
          //           << " matched with " << temp_raw_mesh.object_id <<
          //           std::endl;
          break;
        }
        counter++;
      }

      for (auto &temp_raw_mesh_instance : raw_scene.mesh_instances)
      {
        if (temp_raw_mesh_instance.object_id == base_object_id)
        {
          current_raw_mesh = temp_raw_mesh_instance;
          // std::cout << "Mesh instance object id " <<
          // raw_mesh_instance.object_id
          //           << " matched with instance : "
          //           << temp_raw_mesh_instance.object_id << std::endl;
          break;
        }
      }
    } while (!mesh_object);

    if (!any_reset)
    {
      transform_matrix = transform_matrix * mesh_object->transform_matrix_;
      scaling_flip.sx = scaling_flip.sx != mesh_object->scaling_flip_.sx;
      scaling_flip.sy = scaling_flip.sy != mesh_object->scaling_flip_.sy;
      scaling_flip.sz = scaling_flip.sz != mesh_object->scaling_flip_.sz;
    }

    if (raw_mesh_instance.material_id != -1)
    {
      material = materials_[raw_mesh_instance.material_id - 1];
    }
    else
    {
      material = mesh_object->material_;
    }

    // std::cout << transform_matrix << std::endl;

    objects_.push_back(
        std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
            std::make_shared<MeshInstanceObject>(
                material, mesh_object, raw_mesh_instance.motion_blur,
                transform_matrix, scaling_flip)));
  }
  // exit(1);

  if (timer.configuration_.timer_.load_scene_)
    timer.AddTimeLog(Section::kLoadScene, Event::kEnd);
}

void Scene::PreprocessScene()
{
#ifdef DEBUG
  int object_index = 0;
#endif

  for (const auto &object : objects_)
  {
#ifdef DEBUG
    std::cout << "\tPreprocessing for object : " << object_index << std::endl;
#endif
    std::shared_ptr<BaseObject> object_casted =
        std::dynamic_pointer_cast<BaseObject>(object);

    object_casted->Preprocess(configuration_.acceleration_.bvh_high_level_,
                              configuration_.acceleration_.bvh_low_level_);
  }

  if (configuration_.acceleration_.bvh_high_level_)
  {
    bvh_root_ = BoundingVolumeHierarchyElement::Construct(objects_, 0,
                                                          objects_.size(), 0);
  }
}

void Scene::Render()
{
  int camera_index = 0;
  for (const auto &camera : cameras_)
  {
    if (timer.configuration_.timer_.render_scene_ ||
        timer.configuration_.timer_.ray_tracing_ ||
        timer.configuration_.timer_.tone_mapping_ ||
        timer.configuration_.timer_.export_image_)
      timer.AddTimeLog(Section::kRenderScene, Event::kStart, camera_index);

#ifdef DEBUG
    std::cout << "Rendering camera " << camera_index << std::endl;
#endif
    scheduling_algorithm_(camera, camera_index);
#ifdef DEBUG
    std::cout << "Tonemapping result " << camera_index << std::endl;
#endif

    if (timer.configuration_.timer_.filtering_)
      timer.AddTimeLog(Section::kFiltering, Event::kStart, camera_index);
    filtering_algorithm_(camera->GetImageSampledDataReference(),
                         camera->GetImageDataReference());
    if (timer.configuration_.timer_.filtering_)
      timer.AddTimeLog(Section::kFiltering, Event::kEnd, camera_index);

    if (timer.configuration_.timer_.tone_mapping_)
      timer.AddTimeLog(Section::kToneMapping, Event::kStart, camera_index);
    tone_mapping_algorithm_(camera->GetImageDataReference(),
                            camera->GetTonemappedImageDataReference());
    if (timer.configuration_.timer_.tone_mapping_)
      timer.AddTimeLog(Section::kToneMapping, Event::kEnd, camera_index);
#ifdef DEBUG
    std::cout << "Exporting result " << camera_index << std::endl;
#endif
    if (timer.configuration_.timer_.export_image_)
      timer.AddTimeLog(Section::kExportImage, Event::kStart, camera_index);
    camera->ExportView(exporter_);
    if (timer.configuration_.timer_.export_image_)
      timer.AddTimeLog(Section::kExportImage, Event::kEnd, camera_index);
    if (timer.configuration_.timer_.render_scene_ ||
        timer.configuration_.timer_.ray_tracing_ ||
        timer.configuration_.timer_.tone_mapping_ ||
        timer.configuration_.timer_.export_image_)
      timer.AddTimeLog(Section::kRenderScene, Event::kEnd, camera_index);
    camera_index++;
  }
}