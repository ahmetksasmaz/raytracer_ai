#include "Scene.hpp"

#include "Timer.hpp"

Scene::Scene(const std::string &filename, const Configuration &configuration)
    : filename_(filename), configuration_(configuration) {
  switch (configuration_.strategies_.ray_tracing_algorithm_) {
    case RayTracingAlgorithm::kDefault:
      ray_tracing_algorithm_ = std::bind(
          &Scene::DefaultRayTracingAlgorithm, this, std::placeholders::_1,
          std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
      break;
    case RayTracingAlgorithm::kRecursive:
      ray_tracing_algorithm_ = std::bind(
          &Scene::RecursiveBRDFRayTracingAlgorithm, this, std::placeholders::_1,
          std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
      break;
  }

  switch (configuration_.strategies_.scheduling_algorithm_) {
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
    case SchedulingAlgorithm::kBlockDivide:
      scheduling_algorithm_ = std::bind(&Scene::BlockDivideThreadSchedulingAlgorithm, this,
                    std::placeholders::_1, std::placeholders::_2);
      break;
    case SchedulingAlgorithm::kSlidingThread:
      scheduling_algorithm_ = std::bind(&Scene::SlidingThreadSchedulingAlgorithm, this,
                    std::placeholders::_1, std::placeholders::_2);
      break;
  }

  switch (configuration_.sampling_.area_light_sampling_) {
    case SamplingAlgorithm::kRandom:
      area_light_sampling_algorithm_ = uniform_random_2d;
      break;
  }

  switch (configuration_.sampling_.pixel_filtering_) {
    case FilteringAlgorithm::kBox:
      filtering_algorithm_ = std::bind(
          &Scene::AveragingFilterAlgorithm, this, std::placeholders::_1,
          std::placeholders::_2, std::placeholders::_3, std::placeholders::_4,
          std::placeholders::_5);
      break;
    case FilteringAlgorithm::kGaussian:
      filtering_algorithm_ = std::bind(
          &Scene::GaussianFilterAlgorithm, this, std::placeholders::_1,
          std::placeholders::_2, std::placeholders::_3, std::placeholders::_4,
          std::placeholders::_5);
      break;

    case FilteringAlgorithm::kExtendedGaussian:
      filtering_algorithm_ = std::bind(
          &Scene::ExtendedGaussianFilterAlgorithm, this, std::placeholders::_1,
          std::placeholders::_2, std::placeholders::_3, std::placeholders::_4,
          std::placeholders::_5);
      break;
  }

  LoadScene();
  if (timer.configuration_.timer_.preprocess_scene_)
    timer.AddTimeLog(Section::kPreprocessScene, Event::kStart);
  PreprocessScene();
  if (timer.configuration_.timer_.preprocess_scene_)
    timer.AddTimeLog(Section::kPreprocessScene, Event::kEnd);
}

Scene::~Scene() {
  cameras_.clear();
  point_lights_.clear();
  materials_.clear();
  objects_.clear();
}

void Scene::LoadScene() {
  RawScene raw_scene;
  if (timer.configuration_.timer_.parse_scene_file_)
    timer.AddTimeLog(Section::kParseXML, Event::kStart);
  std::string file_extension = filename_.substr(filename_.find_last_of(".") + 1);
  for (auto &c : file_extension){
    c = std::tolower(c);
  }
  if (file_extension == "json")
      raw_scene.loadFromJSON(filename_);
  else if(file_extension == "xml") {
    raw_scene.loadFromXml(filename_);
  }
  else {
    throw std::runtime_error("Error: Unsupported file format: " + file_extension);
  }
  if (timer.configuration_.timer_.parse_scene_file_)
    timer.AddTimeLog(Section::kParseXML, Event::kEnd);

  if (timer.configuration_.timer_.load_scene_)
    timer.AddTimeLog(Section::kLoadScene, Event::kStart);

  background_color_ = raw_scene.background_color;
  shadow_ray_epsilon_ = raw_scene.shadow_ray_epsilon;
  max_recursion_depth_ = raw_scene.max_recursion_depth;

  for (const auto &raw_image : raw_scene.images) {
    images_.push_back(std::make_shared<BaseImage>(raw_image.path));
  }

  ambient_light_ = std::make_shared<AmbientLightSource>(raw_scene.ambient_light);

  for (const auto &raw_point_light : raw_scene.point_lights) {
    RawScalingFlip scaling_flip{false, false, false};
    Mat4x4f transform_matrix = parse_transformation(
        raw_point_light.transformations, scaling_flip, raw_scene.translations,
        raw_scene.scalings, raw_scene.rotations, raw_scene.composites);
    
    point_lights_.push_back(std::make_shared<PointLightSource>(
        transform_matrix * raw_point_light.position, raw_point_light.intensity));
  }
  for (const auto &raw_area_light : raw_scene.area_lights) {
    RawScalingFlip scaling_flip{false, false, false};
    Mat4x4f transform_matrix = parse_transformation(
        raw_area_light.transformations, scaling_flip, raw_scene.translations,
        raw_scene.scalings, raw_scene.rotations, raw_scene.composites);
    Vec3f transformed_position = transform_matrix * raw_area_light.position;
    Vec3f transformed_second_position = transform_matrix * (raw_area_light.position + raw_area_light.normal);
    Vec3f transformed_normal = normalize(transformed_second_position - transformed_position);
    area_lights_.push_back(std::make_shared<AreaLightSource>(
        transformed_position, raw_area_light.radiance, transformed_normal,
        raw_area_light.size));
  }

  for (const auto &raw_directional_light : raw_scene.directional_lights) {
    directional_lights_.push_back(std::make_shared<DirectionalLightSource>(
        raw_directional_light.direction, raw_directional_light.radiance));
  }

  for (const auto &raw_spot_light : raw_scene.spot_lights) {
    spot_lights_.push_back(std::make_shared<SpotLightSource>(
        raw_spot_light.position, raw_spot_light.direction,
        raw_spot_light.intensity, raw_spot_light.coverage_angle,
        raw_spot_light.falloff_angle));
  }

  {
    const auto &raw_spherical_light = raw_scene.spherical_directional_light;
    if(raw_spherical_light.exists){
      spherical_directional_light_ =
        std::make_shared<SphericalDirectionalLightSource>(
          raw_spherical_light.type, images_[raw_spherical_light.image_id - 1], raw_spherical_light.sampler);
    }
    else{
      spherical_directional_light_ = nullptr;
    }
  }
  for (const auto &raw_camera : raw_scene.cameras) {
    RawScalingFlip scaling_flip{false, false, false};
    Mat4x4f transform_matrix = parse_transformation(
        raw_camera.transformations, scaling_flip, raw_scene.translations,
        raw_scene.scalings, raw_scene.rotations, raw_scene.composites);
    Vec3f transformed_position = raw_camera.position;
    Vec3f transformed_gaze_point = raw_camera.gaze_point;
    Vec3f transformed_gaze = raw_camera.gaze;
    FP_PRECISION near_distance = raw_camera.near_distance;
    if(raw_camera.look_at_camera){
      transformed_position = transform_matrix * raw_camera.position;
      transformed_gaze_point = transform_matrix * raw_camera.gaze_point;
      FP_PRECISION first_distance = norm(transformed_gaze_point - transformed_position);
      FP_PRECISION second_distance = norm(raw_camera.gaze_point - raw_camera.position);
      near_distance = near_distance * first_distance / second_distance;
    }
    else{
      transformed_position = transform_matrix * raw_camera.position;
      transformed_gaze = transform_matrix * (raw_camera.position + normalize(raw_camera.gaze));
      near_distance = near_distance * norm(transformed_gaze - transformed_position);
      transformed_gaze = normalize(transformed_gaze - transformed_position);
    }

    std::vector<std::shared_ptr<BaseToneMapping>> tone_mappings;
    for (const auto &raw_tone_mapping : raw_camera.tone_mappings) {
      switch (raw_tone_mapping.algorithm) {
        case RawToneMappingAlgorithm::kPhotographic:
          tone_mappings.push_back(std::make_shared<PhotographicToneMapping>(raw_camera.image_width, raw_camera.image_height, raw_tone_mapping.key,
                                                                            raw_tone_mapping.burn,
                                                                            raw_tone_mapping.saturation,
                                                                            raw_tone_mapping.gamma,
                                                                            raw_tone_mapping.extension));
          break;
        case RawToneMappingAlgorithm::kFilmic:
          tone_mappings.push_back(std::make_shared<FilmicToneMapping>(raw_camera.image_width, raw_camera.image_height, raw_tone_mapping.key,
                                                                            raw_tone_mapping.burn,
                                                                            raw_tone_mapping.saturation,
                                                                            raw_tone_mapping.gamma,
                                                                            raw_tone_mapping.extension));
          break;
        case RawToneMappingAlgorithm::kACES:
          tone_mappings.push_back(std::make_shared<ACESToneMapping>(raw_camera.image_width, raw_camera.image_height, raw_tone_mapping.key,
                                                                            raw_tone_mapping.burn,
                                                                            raw_tone_mapping.saturation,
                                                                            raw_tone_mapping.gamma,
                                                                            raw_tone_mapping.extension));
          break;
      }
    }

    cameras_.push_back(std::make_shared<BaseCamera>(
        raw_camera.look_at_camera, transformed_position, transformed_gaze,
        transformed_gaze_point, raw_camera.up, raw_camera.near_plane,
        raw_camera.fov_y, near_distance, raw_camera.image_width,
        raw_camera.image_height, raw_camera.image_name, raw_camera.num_samples, tone_mappings, raw_camera.left_handed,
        configuration_.sampling_.time_sampling_,
        configuration_.sampling_.pixel_sampling_, raw_camera.focus_distance,
        raw_camera.aperture_size, configuration_.sampling_.aperture_sampling_,
        configuration_.sampling_.aperture_type_));
  }

  for(const auto &raw_brdf : raw_scene.brdfs){
    switch (raw_brdf.type) {
      case RawBRDFType::kOriginalBlinnPhong:
        brdfs_.push_back(std::make_shared<OriginalBlinnPhong>(raw_brdf.exponent));
        break;
      case RawBRDFType::kOriginalPhong:
        brdfs_.push_back(std::make_shared<OriginalPhong>(raw_brdf.exponent));
        break;
      case RawBRDFType::kModifiedBlinnPhong:
        brdfs_.push_back(std::make_shared<ModifiedBlinnPhong>(raw_brdf.exponent, raw_brdf.normalized));
        break;
      case RawBRDFType::kModifiedPhong:
        brdfs_.push_back(std::make_shared<ModifiedPhong>(raw_brdf.exponent, raw_brdf.normalized));
        break;
      case RawBRDFType::kTorranceSparrow:
        brdfs_.push_back(std::make_shared<TorranceSparrow>(raw_brdf.exponent, raw_brdf.kd_fresnel));
        break;
    }
  }

  for (const auto &raw_material : raw_scene.materials) {
    switch (raw_material.material_type) {
      case RawMaterialType::kDefault:
        materials_.push_back(std::make_shared<BaseMaterial>(
            raw_material.brdf_id < 0 ? std::make_shared<OriginalBlinnPhong>(raw_material.phong_exponent) : brdfs_[raw_material.brdf_id - 1],
            raw_material.ambient, raw_material.diffuse, raw_material.specular,
            raw_material.phong_exponent, raw_material.roughness, raw_material.refraction_index, raw_material.absorption_index));
        break;

      case RawMaterialType::kMirror:
        materials_.push_back(std::make_shared<MirrorMaterial>(
            raw_material.brdf_id < 0 ? std::make_shared<OriginalBlinnPhong>(raw_material.phong_exponent) : brdfs_[raw_material.brdf_id - 1],
            raw_material.ambient, raw_material.diffuse, raw_material.specular,
            raw_material.phong_exponent, raw_material.roughness,
            raw_material.mirror, raw_material.refraction_index, raw_material.absorption_index));
        break;
      case RawMaterialType::kConductor:
        materials_.push_back(std::make_shared<ConductorMaterial>(
            raw_material.brdf_id < 0 ? std::make_shared<OriginalBlinnPhong>(raw_material.phong_exponent) : brdfs_[raw_material.brdf_id - 1],
            raw_material.ambient, raw_material.diffuse, raw_material.specular,
            raw_material.phong_exponent, raw_material.roughness,
            raw_material.mirror, raw_material.refraction_index,
            raw_material.absorption_index));
        break;
      case RawMaterialType::kDielectric:
        materials_.push_back(std::make_shared<DielectricMaterial>(
            raw_material.brdf_id < 0 ? std::make_shared<OriginalBlinnPhong>(raw_material.phong_exponent) : brdfs_[raw_material.brdf_id - 1],
            raw_material.ambient, raw_material.diffuse, raw_material.specular,
            raw_material.phong_exponent, raw_material.roughness,
            raw_material.mirror, raw_material.absorption_coefficient,
            raw_material.refraction_index));
        break;
    }
  }

  for (const auto &raw_texture_map : raw_scene.texture_maps) {
  if (raw_texture_map.type == RawTextureMapType::kCheckerboard)
    {
      texture_maps_.push_back(std::make_shared<CheckerboardTextureMap>(
          raw_texture_map.decal_mode, raw_texture_map.bump_factor,
          raw_texture_map.scale, raw_texture_map.offset,
          raw_texture_map.black_color, raw_texture_map.white_color));
    }
    else if (raw_texture_map.type == RawTextureMapType::kPerlin)
    {
      texture_maps_.push_back(std::make_shared<PerlinTextureMap>(
          raw_texture_map.decal_mode, raw_texture_map.bump_factor,
          raw_texture_map.noise_conversion, raw_texture_map.noise_scale,
          raw_texture_map.num_octaves));
    }
    else if (raw_texture_map.type == RawTextureMapType::kImage)
    {
      if(raw_texture_map.decal_mode == RawTextureMapDecalMode::kReplaceBackground)
      {
        texture_maps_.push_back(std::make_shared<ImageTextureMap>(
        raw_texture_map.decal_mode, raw_texture_map.bump_factor, std::shared_ptr<BaseImage>(images_[raw_texture_map.image_id - 1]),
        raw_texture_map.interpolation_mode, 1.0));
        background_texture_map_ = texture_maps_.back();
      }
      else{
          texture_maps_.push_back(std::make_shared<ImageTextureMap>(
          raw_texture_map.decal_mode, raw_texture_map.bump_factor, std::shared_ptr<BaseImage>(images_[raw_texture_map.image_id - 1]),
          raw_texture_map.interpolation_mode, raw_texture_map.normalizer));
      }
    }
    
  }

  for (const auto &raw_sphere : raw_scene.spheres) {
    RawScalingFlip scaling_flip{false, false, false};
    Mat4x4f transform_matrix = parse_transformation(
        raw_sphere.transformations, scaling_flip, raw_scene.translations,
        raw_scene.scalings, raw_scene.rotations, raw_scene.composites);
    std::vector<std::shared_ptr<BaseTextureMap>> textures;
    std::stringstream ss(raw_sphere.textures);
    std::string texture_id_str;
    while (std::getline(ss, texture_id_str, ' ')) {
      int texture_id = std::stoi(texture_id_str);
      textures.push_back(texture_maps_[texture_id - 1]);
    }
    objects_.push_back(
        std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
            std::make_shared<SphereObject>(
                materials_[raw_sphere.material_id - 1], textures,
                raw_scene.vertex_data[raw_sphere.center_vertex_id - 1],
                raw_sphere.radius, raw_sphere.motion_blur, transform_matrix,
                scaling_flip)));
  }

  for (const auto &raw_plane : raw_scene.planes) {
    RawScalingFlip scaling_flip{false, false, false};
    Mat4x4f transform_matrix = parse_transformation(
        raw_plane.transformations, scaling_flip, raw_scene.translations,
        raw_scene.scalings, raw_scene.rotations, raw_scene.composites);
    std::vector<std::shared_ptr<BaseTextureMap>> textures;
    std::stringstream ss(raw_plane.textures);
    std::string texture_id_str;
    while (std::getline(ss, texture_id_str, ' ')) {
      int texture_id = std::stoi(texture_id_str);
      textures.push_back(texture_maps_[texture_id - 1]);
    }
    plane_objects_.push_back(
            std::make_shared<PlaneObject>(
                materials_[raw_plane.material_id - 1], textures,
                raw_scene.vertex_data[raw_plane.point_vertex_id - 1],
                raw_plane.normal, raw_plane.motion_blur, transform_matrix,
                scaling_flip));
  }

  for (const auto &raw_triangle : raw_scene.triangles) {
    RawScalingFlip scaling_flip{false, false, false};
    Mat4x4f transform_matrix = parse_transformation(
        raw_triangle.transformations, scaling_flip, raw_scene.translations,
        raw_scene.scalings, raw_scene.rotations, raw_scene.composites);
    std::vector<std::shared_ptr<BaseTextureMap>> textures;
    std::stringstream ss(raw_triangle.textures);
    std::string texture_id_str;
    while (std::getline(ss, texture_id_str, ' ')) {
      int texture_id = std::stoi(texture_id_str);
      textures.push_back(texture_maps_[texture_id - 1]);
    }
    objects_.push_back(
        std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
            std::make_shared<TriangleObject>(
                materials_[raw_triangle.material_id - 1], textures,
                raw_scene.vertex_data[raw_triangle.indices.v0_id - 1],
                raw_scene.vertex_data[raw_triangle.indices.v1_id - 1],
                raw_scene.vertex_data[raw_triangle.indices.v2_id - 1],
                textures.size() > 0 ? raw_scene.tex_coord_data[raw_triangle.indices.v0_id - 1] : Vec2f{0,0},
                textures.size() > 0 ? raw_scene.tex_coord_data[raw_triangle.indices.v1_id - 1] : Vec2f{0,0},
                textures.size() > 0 ? raw_scene.tex_coord_data[raw_triangle.indices.v2_id - 1] : Vec2f{0,0},
                raw_triangle.motion_blur, transform_matrix, scaling_flip)));
  }

  int meshes_start_index = objects_.size();
  for (const auto &raw_mesh : raw_scene.meshes) {
    RawScalingFlip scaling_flip{false, false, false};
    Mat4x4f transform_matrix = parse_transformation(
        raw_mesh.transformations, scaling_flip, raw_scene.translations,
        raw_scene.scalings, raw_scene.rotations, raw_scene.composites);
    std::vector<std::shared_ptr<BaseTextureMap>> textures;
    std::stringstream ss(raw_mesh.textures);
    std::string texture_id_str;
    while (std::getline(ss, texture_id_str, ' ')) {
      int texture_id = std::stoi(texture_id_str);
      textures.push_back(texture_maps_[texture_id - 1]);
    }
    if (raw_mesh.ply_filepath != "") {
      objects_.push_back(
          std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
              std::make_shared<MeshObject>(
                  materials_[raw_mesh.material_id - 1], textures, raw_mesh.ply_filepath,
                  raw_mesh.vertex_offset, raw_mesh.tex_coord_offset,
                  raw_mesh.motion_blur, transform_matrix, scaling_flip)));
    } else {
      objects_.push_back(
          std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
              std::make_shared<MeshObject>(
                  materials_[raw_mesh.material_id - 1], textures, raw_mesh.faces,
                  raw_scene.vertex_data, raw_scene.tex_coord_data, raw_mesh.vertex_offset, raw_mesh.tex_coord_offset, raw_mesh.motion_blur, transform_matrix,
                  scaling_flip)));
    }
  }

  for (auto &raw_mesh_instance : raw_scene.mesh_instances) {
    RawScalingFlip scaling_flip{false, false, false};
    Mat4x4f transform_matrix = IDENTITY_MATRIX;

    std::shared_ptr<MeshObject> mesh_object = nullptr;
    std::shared_ptr<BaseMaterial> material = nullptr;

    // Search for the root mesh objects
    auto current_raw_mesh = raw_mesh_instance;
    bool any_reset = false;

    if (current_raw_mesh.reset_transform) {
      transform_matrix =
          parse_transformation(current_raw_mesh.transformations, scaling_flip,
                               raw_scene.translations, raw_scene.scalings,
                               raw_scene.rotations, raw_scene.composites);
      any_reset = true;
    }

    do {
      if (!any_reset) {
        transform_matrix =
            transform_matrix *
            parse_transformation(current_raw_mesh.transformations, scaling_flip,
                                 raw_scene.translations, raw_scene.scalings,
                                 raw_scene.rotations, raw_scene.composites);
      }

      any_reset = any_reset || current_raw_mesh.reset_transform;

      int base_object_id = current_raw_mesh.base_object_id;

      int counter = 0;
      for (auto &temp_raw_mesh : raw_scene.meshes) {
        if (temp_raw_mesh.object_id == base_object_id) {
          mesh_object = std::dynamic_pointer_cast<MeshObject>(
              objects_[meshes_start_index + counter]);
          break;
        }
        counter++;
      }

      for (auto &temp_raw_mesh_instance : raw_scene.mesh_instances) {
        if (temp_raw_mesh_instance.object_id == base_object_id) {
          current_raw_mesh = temp_raw_mesh_instance;
          break;
        }
      }
    } while (!mesh_object);

    if (!any_reset) {
      transform_matrix = transform_matrix * mesh_object->transform_matrix_;
      scaling_flip.sx = scaling_flip.sx != mesh_object->scaling_flip_.sx;
      scaling_flip.sy = scaling_flip.sy != mesh_object->scaling_flip_.sy;
      scaling_flip.sz = scaling_flip.sz != mesh_object->scaling_flip_.sz;
    }

    if (raw_mesh_instance.material_id != -1) {
      material = materials_[raw_mesh_instance.material_id - 1];
    } else {
      material = mesh_object->material_;
    }

    std::vector<std::shared_ptr<BaseTextureMap>> textures;
    std::stringstream ss(raw_mesh_instance.textures);
    std::string texture_id_str;
    while (std::getline(ss, texture_id_str, ' ')) {
      int texture_id = std::stoi(texture_id_str);
      textures.push_back(texture_maps_[texture_id - 1]);
    }

    objects_.push_back(
        std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
            std::make_shared<MeshInstanceObject>(
                material, textures, mesh_object, raw_mesh_instance.motion_blur,
                transform_matrix, scaling_flip)));
  }

  if (timer.configuration_.timer_.load_scene_)
    timer.AddTimeLog(Section::kLoadScene, Event::kEnd);
}

void Scene::PreprocessScene() {
#ifdef DEBUG
  int object_index = 0;
#endif

  for (const auto &object : objects_) {
    std::shared_ptr<BaseObject> object_casted =
        std::dynamic_pointer_cast<BaseObject>(object);

    object_casted->Preprocess(configuration_.acceleration_.bvh_high_level_,
                              configuration_.acceleration_.bvh_low_level_);
  }

  if (configuration_.acceleration_.bvh_high_level_) {
    bvh_root_ = BoundingVolumeHierarchyElement::Construct(objects_, 0,
                                                          objects_.size(), 0);
  }
}

void Scene::Render() {
  int camera_index = 0;
  for (const auto &camera : cameras_) {
    if (timer.configuration_.timer_.render_scene_ ||
        timer.configuration_.timer_.ray_tracing_ ||
        timer.configuration_.timer_.tone_mapping_ ||
        timer.configuration_.timer_.export_image_)
      timer.AddTimeLog(Section::kRenderScene, Event::kStart, camera_index);
    scheduling_algorithm_(camera, camera_index);

    if (timer.configuration_.timer_.filtering_)
      timer.AddTimeLog(Section::kFiltering, Event::kStart, camera_index);
    filtering_algorithm_(camera->GetImageSampledDataReference(),
                         camera->image_width_, camera->image_height_,
                         camera->mem_num_samples_,
                         camera->GetImageDataReference());
    if (timer.configuration_.timer_.filtering_)
      timer.AddTimeLog(Section::kFiltering, Event::kEnd, camera_index);

    if (timer.configuration_.timer_.tone_mapping_)
      timer.AddTimeLog(Section::kToneMapping, Event::kStart, camera_index);

      camera->ApplyToneMappings();

    if (timer.configuration_.timer_.tone_mapping_)
      timer.AddTimeLog(Section::kToneMapping, Event::kEnd, camera_index);
    if (timer.configuration_.timer_.export_image_)
      timer.AddTimeLog(Section::kExportImage, Event::kStart, camera_index);
    camera->ExportView();
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