#include "Scene.hpp"

#include "Timer.hpp"

namespace {

// Turns a scene-supplied spectrum into a Spectrum, falling back to uplifting
// the RGB value when the scene did not supply one.
//
// An unknown illuminant name or library reference is a hard error rather than a
// warning: silently rendering under the wrong spectrum would corrupt a
// white-balance study in a way that is invisible in the output.
Spectrum ResolveSpectrum(const RawSpectrumData &raw, const Vec3f &rgb_fallback) {
  if (!raw.ref.empty()) {
    const SpectrumRecord &record = SpectrumLibrary::Instance().Require(raw.ref);
    if (record.multichannel) {
      throw std::runtime_error(
          "Spectrum reference '" + raw.ref +
          "' has three channels and cannot be used as a single spectrum."
          " Multi-channel records are camera sensitivities; reference them"
          " from a camera's \"Sensor\" block instead.");
    }
    return record.value * raw.scale;
  }
  if (!raw.illuminant.empty()) {
    Spectrum illuminant;
    if (!IlluminantByName(raw.illuminant, illuminant)) {
      throw std::runtime_error("Unknown illuminant '" + raw.illuminant +
                               "'. Known names: D65, A, E.");
    }
    return illuminant * raw.scale;
  }
  if (!raw.values.empty()) {
    return ResampleSpectrum(raw.wavelengths, raw.values) * raw.scale;
  }
  return UpliftRGB(rgb_fallback);
}

}  // namespace

Scene::Scene(const std::string &filename, bool serial, bool collect_aovs)
    : filename_(filename) {
  ray_tracing_algorithm_ = std::bind(
      &Scene::RecursiveBRDFRayTracingAlgorithm, this, std::placeholders::_1,
      std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);

  path_tracing_algorithm_ = std::bind(
      &Scene::RecursiveBRDFPathTracingAlgorithm, this, std::placeholders::_1,
      std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5,
      std::placeholders::_6);

  // Serial rendering exists so a suspected race can be ruled in or out: if a
  // result changes when the tile threads go away, the bug is in the threading
  // and not in the physics. It is much slower, so it is opt-in.
  scheduling_algorithm_ =
      serial ? std::bind(&Scene::NonThreadSchedulingAlgorithm, this,
                         std::placeholders::_1, std::placeholders::_2)
             : std::bind(&Scene::ThreadQueueSchedulingAlgorithm, this,
                         std::placeholders::_1, std::placeholders::_2);

  area_light_sampling_algorithm_ = uniform_random_2d;

  LoadScene();

  // After LoadScene, so the cameras exist and their resolutions are known.
  if (collect_aovs) {
    for (const auto &camera : cameras_) camera->EnableAOVs();
  }

  timer.AddTimeLog(Section::kPreprocessScene, Event::kStart);
  PreprocessScene();
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
  timer.AddTimeLog(Section::kParseXML, Event::kStart);
  std::string file_extension = filename_.substr(filename_.find_last_of(".") + 1);
  for (auto &c : file_extension){
    c = std::tolower(c);
  }
  if (file_extension == "json") {
    raw_scene.loadFromJSON(filename_);
  } else if (file_extension == "xml") {
    // The XML loader is gone. It had drifted years behind the JSON one --
    // no Plane, MeshInstance, LightMesh, LightSphere, BRDFs, DirectionalLight,
    // SpotLight, SphericalDirectionalLight, Tonemap or Renderer -- so it did
    // not fail on a modern scene, it silently rendered a different one. An
    // explicit error is the safer answer.
    throw std::runtime_error(
        "Error: XML scenes are no longer supported; the loader was missing "
        "most of the format. Convert '" + filename_ + "' to JSON.");
  } else {
    throw std::runtime_error("Error: Unsupported file format: " + file_extension);
  }
  timer.AddTimeLog(Section::kParseXML, Event::kEnd);

  timer.AddTimeLog(Section::kLoadScene, Event::kStart);

  // The measured spectral library has to be in place before anything resolves a
  // spectrum, since every _ref in the scene is looked up against it.
  const auto slash = filename_.find_last_of('/');
  const std::string scene_directory =
      slash == std::string::npos ? "." : filename_.substr(0, slash);
  SpectrumLibrary &library = SpectrumLibrary::Instance();
  library.LoadDefault(raw_scene.spectral_library, scene_directory);
  if (library.size() > 0) {
    std::cout << "Loaded " << library.size() << " spectra from "
              << library.directories().front() << std::endl;
  }

  // Authored as an RGB triple, uplifted to a smooth spectrum like any other
  // RGB scene quantity.
  background_color_ = UpliftRGB(Vec3f{
      static_cast<FP_PRECISION>(raw_scene.background_color.x),
      static_cast<FP_PRECISION>(raw_scene.background_color.y),
      static_cast<FP_PRECISION>(raw_scene.background_color.z)});
  shadow_ray_epsilon_ = raw_scene.shadow_ray_epsilon;

  for (const auto &raw_image : raw_scene.images) {
    images_.push_back(std::make_shared<BaseImage>(raw_image.path));
  }

  ambient_light_ = std::make_shared<AmbientLightSource>(UpliftRGB(raw_scene.ambient_light));

  for (const auto &raw_point_light : raw_scene.point_lights) {
    RawScalingFlip scaling_flip{false, false, false};
    Mat4x4f transform_matrix = parse_transformation(
        raw_point_light.transformations, scaling_flip, raw_scene.translations,
        raw_scene.scalings, raw_scene.rotations, raw_scene.composites);
    
    point_lights_.push_back(std::make_shared<PointLightSource>(
        transform_matrix * raw_point_light.position, ResolveSpectrum(raw_point_light.intensity_spectrum, raw_point_light.intensity)));
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
        transformed_position, ResolveSpectrum(raw_area_light.radiance_spectrum, raw_area_light.radiance), transformed_normal,
        raw_area_light.size));
  }

  for (const auto &raw_directional_light : raw_scene.directional_lights) {
    directional_lights_.push_back(std::make_shared<DirectionalLightSource>(
        raw_directional_light.direction, ResolveSpectrum(raw_directional_light.radiance_spectrum, raw_directional_light.radiance)));
  }

  for (const auto &raw_spot_light : raw_scene.spot_lights) {
    spot_lights_.push_back(std::make_shared<SpotLightSource>(
        raw_spot_light.position, raw_spot_light.direction,
        ResolveSpectrum(raw_spot_light.intensity_spectrum, raw_spot_light.intensity), raw_spot_light.coverage_angle,
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
        raw_camera.image_height, raw_camera.image_name, raw_camera.num_samples, tone_mappings,
        raw_camera.max_recursion_depth,
        raw_camera.min_recursion_depth, raw_camera.left_handed,
        raw_camera.path_tracing_enabled,
        raw_camera.importance_sampling_enabled,
        raw_camera.nee_enabled,
        raw_camera.mis_balance_enabled,
        raw_camera.russian_roulette_enabled,
        raw_camera.splitting_factor,
        raw_camera.sample_max_val,
        SamplingAlgorithm::kJittered,
        SamplingAlgorithm::kHammersley,
        raw_camera.focus_distance,
        raw_camera.aperture_size,
        SamplingAlgorithm::kHammersley,
        ApertureType::kCircular));
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

  // A material with no explicit _BRDF needs a default, and the right default
  // depends on the renderer. OriginalBlinnPhong omits the 1/pi normalisation, so
  // under a path tracer -- which divides by a hemisphere pdf -- an albedo-1
  // diffuse surface reflects pi times the energy it receives on every bounce:
  // interreflection diverges instead of converging, and throughput grows without
  // bound so Russian roulette never terminates anything. The ray tracer does not
  // divide by a pdf and is calibrated around the unnormalised form, so it keeps
  // the historical default and existing scenes render unchanged.
  bool any_path_tracing = false;
  for (const auto &raw_camera : raw_scene.cameras) {
    if (raw_camera.path_tracing_enabled) any_path_tracing = true;
  }
  auto make_default_brdf =
      [any_path_tracing](FP_PRECISION exponent) -> std::shared_ptr<BaseBRDF> {
    if (any_path_tracing) {
      return std::make_shared<ModifiedBlinnPhong>(exponent, true);
    }
    return std::make_shared<OriginalBlinnPhong>(exponent);
  };
  if (any_path_tracing) {
    bool uses_default = false;
    for (const auto &raw_material : raw_scene.materials) {
      if (raw_material.brdf_id < 0) uses_default = true;
    }
    if (uses_default) {
      std::cout << "Note: path tracing is enabled and one or more materials do "
                   "not specify _BRDF; using a normalized ModifiedBlinnPhong "
                   "for those instead of OriginalBlinnPhong so that they "
                   "conserve energy."
                << std::endl;
    }
  }

  for (const auto &raw_material : raw_scene.materials) {
    switch (raw_material.material_type) {
      case RawMaterialType::kDefault:
        materials_.push_back(std::make_shared<BaseMaterial>(
            raw_material.brdf_id < 0 ? make_default_brdf(raw_material.phong_exponent) : brdfs_[raw_material.brdf_id - 1],
            UpliftRGB(raw_material.ambient), ResolveSpectrum(raw_material.diffuse_spectrum, raw_material.diffuse), ResolveSpectrum(raw_material.specular_spectrum, raw_material.specular),
            raw_material.phong_exponent, raw_material.roughness, raw_material.refraction_index, raw_material.absorption_index));
        break;

      case RawMaterialType::kMirror:
        materials_.push_back(std::make_shared<MirrorMaterial>(
            raw_material.brdf_id < 0 ? make_default_brdf(raw_material.phong_exponent) : brdfs_[raw_material.brdf_id - 1],
            UpliftRGB(raw_material.ambient), ResolveSpectrum(raw_material.diffuse_spectrum, raw_material.diffuse), ResolveSpectrum(raw_material.specular_spectrum, raw_material.specular),
            raw_material.phong_exponent, raw_material.roughness,
            UpliftRGB(raw_material.mirror), raw_material.refraction_index, raw_material.absorption_index));
        break;
      case RawMaterialType::kConductor:
        materials_.push_back(std::make_shared<ConductorMaterial>(
            raw_material.brdf_id < 0 ? make_default_brdf(raw_material.phong_exponent) : brdfs_[raw_material.brdf_id - 1],
            UpliftRGB(raw_material.ambient), ResolveSpectrum(raw_material.diffuse_spectrum, raw_material.diffuse), ResolveSpectrum(raw_material.specular_spectrum, raw_material.specular),
            raw_material.phong_exponent, raw_material.roughness,
            UpliftRGB(raw_material.mirror), raw_material.refraction_index,
            raw_material.absorption_index));
        break;
      case RawMaterialType::kDielectric:
        materials_.push_back(std::make_shared<DielectricMaterial>(
            raw_material.brdf_id < 0 ? make_default_brdf(raw_material.phong_exponent) : brdfs_[raw_material.brdf_id - 1],
            UpliftRGB(raw_material.ambient), ResolveSpectrum(raw_material.diffuse_spectrum, raw_material.diffuse), ResolveSpectrum(raw_material.specular_spectrum, raw_material.specular),
            raw_material.phong_exponent, raw_material.roughness,
            UpliftRGB(raw_material.mirror), UpliftRGB(raw_material.absorption_coefficient),
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
        raw_texture_map.interpolation_mode, 1.0, raw_texture_map.degamma));
        background_texture_map_ = texture_maps_.back();
      }
      else{
          texture_maps_.push_back(std::make_shared<ImageTextureMap>(
          raw_texture_map.decal_mode, raw_texture_map.bump_factor, std::shared_ptr<BaseImage>(images_[raw_texture_map.image_id - 1]),
          raw_texture_map.interpolation_mode, raw_texture_map.normalizer, raw_texture_map.degamma));
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
            std::make_shared<SphereObject>(
                materials_[raw_sphere.material_id - 1], textures,
                raw_scene.vertex_data[raw_sphere.center_vertex_id - 1],
                raw_sphere.radius, raw_sphere.motion_blur, transform_matrix,
                scaling_flip));
  }

  for (const auto &raw_sphere : raw_scene.light_spheres) {
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
            std::make_shared<LightSphereObject>(
                materials_[raw_sphere.material_id - 1], textures,
                raw_scene.vertex_data[raw_sphere.center_vertex_id - 1],
                raw_sphere.radius, raw_sphere.motion_blur, transform_matrix,
                scaling_flip, ResolveSpectrum(raw_sphere.radiance_spectrum, raw_sphere.radiance)));
    light_objects_.push_back(objects_.back());
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
            std::make_shared<TriangleObject>(
                materials_[raw_triangle.material_id - 1], textures,
                raw_scene.vertex_data[raw_triangle.indices.v0_id - 1],
                raw_scene.vertex_data[raw_triangle.indices.v1_id - 1],
                raw_scene.vertex_data[raw_triangle.indices.v2_id - 1],
                textures.size() > 0 ? raw_scene.tex_coord_data[raw_triangle.indices.v0_id - 1] : Vec2f{0,0},
                textures.size() > 0 ? raw_scene.tex_coord_data[raw_triangle.indices.v1_id - 1] : Vec2f{0,0},
                textures.size() > 0 ? raw_scene.tex_coord_data[raw_triangle.indices.v2_id - 1] : Vec2f{0,0},
                raw_triangle.motion_blur, transform_matrix, scaling_flip));
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
              std::make_shared<MeshObject>(
                  materials_[raw_mesh.material_id - 1], textures, raw_mesh.ply_filepath,
                  raw_mesh.vertex_offset, raw_mesh.tex_coord_offset,
                  raw_mesh.motion_blur, transform_matrix, scaling_flip));
    } else {
      objects_.push_back(
              std::make_shared<MeshObject>(
                  materials_[raw_mesh.material_id - 1], textures, raw_mesh.faces,
                  raw_scene.vertex_data, raw_scene.tex_coord_data, raw_mesh.vertex_offset, raw_mesh.tex_coord_offset, raw_mesh.motion_blur, transform_matrix,
                  scaling_flip));
    }
  }

  for (const auto &raw_mesh : raw_scene.light_meshes) {
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
              std::make_shared<LightMeshObject>(
                  materials_[raw_mesh.material_id - 1], textures, raw_mesh.ply_filepath,
                  raw_mesh.vertex_offset, raw_mesh.tex_coord_offset,
                  raw_mesh.motion_blur, transform_matrix, scaling_flip, ResolveSpectrum(raw_mesh.radiance_spectrum, raw_mesh.radiance)));
    } else {
      objects_.push_back(
              std::make_shared<LightMeshObject>(
                  materials_[raw_mesh.material_id - 1], textures, raw_mesh.faces,
                  raw_scene.vertex_data, raw_scene.tex_coord_data, raw_mesh.vertex_offset, raw_mesh.tex_coord_offset, raw_mesh.motion_blur, transform_matrix,
                  scaling_flip, ResolveSpectrum(raw_mesh.radiance_spectrum, raw_mesh.radiance)));
    }
    light_objects_.push_back(objects_.back());
  }

  for (auto &raw_mesh_instance : raw_scene.mesh_instances) {
    RawScalingFlip scaling_flip{false, false, false};
    Mat4x4f transform_matrix = IDENTITY_MATRIX;

    std::shared_ptr<MeshObject> mesh_object = nullptr;
    std::shared_ptr<BaseMaterial> material = nullptr;

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
            std::make_shared<MeshInstanceObject>(
                material, textures, mesh_object, raw_mesh_instance.motion_blur,
                transform_matrix, scaling_flip));
  }

  timer.AddTimeLog(Section::kLoadScene, Event::kEnd);
}

FP_PRECISION Scene::LightSamplingPdf(
    const std::shared_ptr<BaseObject> &light_object, const Vec3f &reference_point,
    const Vec3f &light_point, const Vec3f &light_normal) const {
  if (light_objects_.empty()) return 0.0;
  auto light = std::dynamic_pointer_cast<ObjectLightSource>(light_object);
  if (!light) return 0.0;
  const FP_PRECISION pdf =
      light->PdfSolidAngle(reference_point, light_point, light_normal);
  return pdf / static_cast<FP_PRECISION>(light_objects_.size());
}

std::shared_ptr<BaseObject> Scene::IntersectScene(
    const Ray &ray, FP_PRECISION &t_hit, Vec3f &hit_normal, Vec2f &tex_coords,
    Vec2f &hit_u_vector, Vec2f &hit_v_vector, Vec3f &tangent_vector,
    Vec3f &bitangent_vector, bool stop_at_any_hit) const {
  std::shared_ptr<BaseObject> hit_object_ptr = nullptr;

  const int hit_index = bvh_.Intersect(
      ray, objects_, t_hit, hit_normal, tex_coords, hit_u_vector, hit_v_vector,
      tangent_vector, bitangent_vector, false, stop_at_any_hit);
  if (hit_index >= 0) {
    hit_object_ptr = objects_[hit_index];
    if (stop_at_any_hit) return hit_object_ptr;
  }

  // Planes are deliberately kept out of the BVH: they are unbounded, so their
  // AABB would cover the whole scene and every traversal would be forced to
  // descend into it. They are cheap to test directly instead.
  for (const auto &plane : plane_objects_) {
    FP_PRECISION temp_hit = std::numeric_limits<FP_PRECISION>::max();
    Vec3f normal;
    if (plane->IntersectPlane(ray, temp_hit, normal) && temp_hit < t_hit) {
      t_hit = temp_hit;
      hit_normal = normal;
      hit_object_ptr = plane;
      if (stop_at_any_hit) return hit_object_ptr;
    }
  }

  return hit_object_ptr;
}

void Scene::PreprocessScene() {
#ifdef DEBUG
  int object_index = 0;
#endif

  for (const auto &object : objects_) {
    object->Preprocess(true);
  }

  bvh_.BuildBVH(objects_);
}

void Scene::Render() {
  int camera_index = 0;
  for (const auto &camera : cameras_) {
    timer.AddTimeLog(Section::kRenderScene, Event::kStart, camera_index);
    scheduling_algorithm_(camera, camera_index);

    // Reconstruction already happened during tracing: samples were splatted
    // into the film with their filter weights. This just normalises by the
    // accumulated weight, producing the spectral image and the sRGB image.
    timer.AddTimeLog(Section::kFiltering, Event::kStart, camera_index);
    camera->ResolveAccumulator();
    timer.AddTimeLog(Section::kFiltering, Event::kEnd, camera_index);

    timer.AddTimeLog(Section::kToneMapping, Event::kStart, camera_index);

      camera->ApplyToneMappings();

    timer.AddTimeLog(Section::kToneMapping, Event::kEnd, camera_index);
    timer.AddTimeLog(Section::kExportImage, Event::kStart, camera_index);
    camera->ExportView();
    timer.AddTimeLog(Section::kExportImage, Event::kEnd, camera_index);
    timer.AddTimeLog(Section::kRenderScene, Event::kEnd, camera_index);
    camera_index++;
  }
}