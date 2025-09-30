#include "parser.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "tinyxml2.h"
#include "json.hpp"

// #define PARSER_DEBUG

#ifdef PARSER_DEBUG
#include <iostream>
#endif

void parser::RawScene::loadFromXml(const std::string &filepath) {
  tinyxml2::XMLDocument file;
  std::stringstream stream;

  auto res = file.LoadFile(filepath.c_str());
  if (res) {
    throw std::runtime_error("Error: The xml file cannot be loaded.");
  }

  auto root = file.FirstChild();
  if (!root) {
    throw std::runtime_error("Error: Root is not found.");
  }

  // Get BackgroundColor
  auto element = root->FirstChildElement("BackgroundColor");
  if (element) {
    std::string elem_text = element->GetText();
    std::replace(elem_text.begin(), elem_text.end(), '\t', ' ');
    stream << elem_text << std::endl;
  } else {
    stream << "0 0 0" << std::endl;
  }
  stream >> background_color.x >> background_color.y >> background_color.z;
  stream.clear();

  // Get ShadowRayEpsilon
  element = root->FirstChildElement("ShadowRayEpsilon");
  if (element) {
    std::string elem_text = element->GetText();
    std::replace(elem_text.begin(), elem_text.end(), '\t', ' ');
    stream << elem_text << std::endl;
  } else {
    stream << "0.001" << std::endl;
  }
  stream >> shadow_ray_epsilon;
  stream.clear();

  // Get MaxRecursionDepth
  element = root->FirstChildElement("MaxRecursionDepth");
  if (element) {
    std::string elem_text = element->GetText();
    std::replace(elem_text.begin(), elem_text.end(), '\t', ' ');
    stream << elem_text << std::endl;
  } else {
    stream << "0" << std::endl;
  }
  stream >> max_recursion_depth;
  stream.clear();

  // Get Cameras
  element = root->FirstChildElement("Cameras");
  element = element->FirstChildElement("Camera");
  while (element) {
    RawCamera camera;

    if (element->Attribute("type", "lookAt") != NULL) {
      camera.look_at_camera = true;
    }

    auto child = element->FirstChildElement("Position");
    stream << child->GetText() << std::endl;
    stream >> camera.position.x >> camera.position.y >> camera.position.z;

    child = element->FirstChildElement("Gaze");
    if (child) {
      stream << child->GetText() << std::endl;
      stream >> camera.gaze.x >> camera.gaze.y >> camera.gaze.z;
    }

    child = element->FirstChildElement("GazePoint");
    if (child) {
      stream << child->GetText() << std::endl;
      stream >> camera.gaze_point.x >> camera.gaze_point.y >>
          camera.gaze_point.z;
    }

    child = element->FirstChildElement("Up");
    stream << child->GetText() << std::endl;
    stream >> camera.up.x >> camera.up.y >> camera.up.z;

    child = element->FirstChildElement("NearPlane");
    if (child) {
      stream << child->GetText() << std::endl;
      stream >> camera.near_plane.x >> camera.near_plane.y >>
          camera.near_plane.z >> camera.near_plane.w;
    }

    child = element->FirstChildElement("FovY");
    if (child) {
      stream << child->GetText() << std::endl;
      stream >> camera.fov_y;
    }

    child = element->FirstChildElement("NearDistance");
    stream << child->GetText() << std::endl;
    stream >> camera.near_distance;
    child = element->FirstChildElement("ImageResolution");
    stream << child->GetText() << std::endl;
    stream >> camera.image_width >> camera.image_height;
    child = element->FirstChildElement("ImageName");
    stream << child->GetText() << std::endl;
    stream >> camera.image_name;

    child = element->FirstChildElement("NumSamples");
    if (child) {
      stream << child->GetText() << std::endl;
      stream >> camera.num_samples;
    } else {
      camera.num_samples = 0;
    }

    child = element->FirstChildElement("FocusDistance");
    if (child) {
      stream << child->GetText() << std::endl;
      stream >> camera.focus_distance;
    } else {
      camera.focus_distance = 0;
    }

    child = element->FirstChildElement("ApertureSize");
    if (child) {
      stream << child->GetText() << std::endl;
      stream >> camera.aperture_size;
    } else {
      camera.aperture_size = 0;
    }

    cameras.push_back(camera);
    element = element->NextSiblingElement("Camera");
  }
  stream.clear();

#ifdef PARSER_DEBUG
  std::cout << "\t\tCameras parsed." << std::endl;
#endif

  // Get Lights
  element = root->FirstChildElement("Lights");
  auto child = element->FirstChildElement("AmbientLight");
  stream << child->GetText() << std::endl;
  stream >> ambient_light.x >> ambient_light.y >> ambient_light.z;
  element = element->FirstChildElement("PointLight");
  while (element) {
    RawPointLight point_light;
    child = element->FirstChildElement("Position");
    stream << child->GetText() << std::endl;
    child = element->FirstChildElement("Intensity");
    stream << child->GetText() << std::endl;

    stream >> point_light.position.x >> point_light.position.y >>
        point_light.position.z;
    stream >> point_light.intensity.x >> point_light.intensity.y >>
        point_light.intensity.z;

    point_lights.push_back(point_light);
    element = element->NextSiblingElement("PointLight");
  }

  element = root->FirstChildElement("Lights");
  element = element->FirstChildElement("AreaLight");
  while (element) {
    RawAreaLight area_light;
    child = element->FirstChildElement("Position");
    stream << child->GetText() << std::endl;
    child = element->FirstChildElement("Normal");
    stream << child->GetText() << std::endl;
    child = element->FirstChildElement("Size");
    stream << child->GetText() << std::endl;
    child = element->FirstChildElement("Radiance");
    stream << child->GetText() << std::endl;

    stream >> area_light.position.x >> area_light.position.y >>
        area_light.position.z;
    stream >> area_light.normal.x >> area_light.normal.y >> area_light.normal.z;
    stream >> area_light.size;
    stream >> area_light.radiance.x >> area_light.radiance.y >>
        area_light.radiance.z;

    area_lights.push_back(area_light);
    element = element->NextSiblingElement("AreaLight");
  }

#ifdef PARSER_DEBUG
  std::cout << "\t\tLights parsed." << std::endl;
#endif

  // Get Materials
  element = root->FirstChildElement("Materials");
  element = element->FirstChildElement("Material");
  while (element) {
    RawMaterial material;
    if (element->Attribute("type", "mirror") != NULL) {
      material.material_type = RawMaterialType::kMirror;
    } else if (element->Attribute("type", "conductor") != NULL) {
      material.material_type = RawMaterialType::kConductor;
    } else if (element->Attribute("type", "dielectric") != NULL) {
      material.material_type = RawMaterialType::kDielectric;
    } else {
      material.material_type = RawMaterialType::kDefault;
    }

    child = element->FirstChildElement("AmbientReflectance");
    stream << child->GetText() << std::endl;
    child = element->FirstChildElement("DiffuseReflectance");
    stream << child->GetText() << std::endl;
    child = element->FirstChildElement("SpecularReflectance");
    stream << child->GetText() << std::endl;
    child = element->FirstChildElement("MirrorReflectance");
    bool mirror_reflectance_exists = child != NULL;
    if (mirror_reflectance_exists) {
      // assert(material.material_type == RawMaterialType::kMirror ||
      //        material.material_type == RawMaterialType::kConductor);
      stream << child->GetText() << std::endl;
    }

    child = element->FirstChildElement("AbsorptionCoefficient");
    if (child) {
      assert(material.material_type == RawMaterialType::kDielectric);
      stream << child->GetText() << std::endl;
    }

    child = element->FirstChildElement("RefractionIndex");
    if (child) {
      assert(material.material_type == RawMaterialType::kConductor ||
             material.material_type == RawMaterialType::kDielectric);
      stream << child->GetText() << std::endl;
    }

    child = element->FirstChildElement("AbsorptionIndex");
    if (child) {
      assert(material.material_type == RawMaterialType::kConductor);
      stream << child->GetText() << std::endl;
    }
    child = element->FirstChildElement("PhongExponent");
    bool phong_exponent_exists = child != NULL;
    if (phong_exponent_exists) {
      stream << child->GetText() << std::endl;
    }

    stream >> material.ambient.x >> material.ambient.y >> material.ambient.z;
    stream >> material.diffuse.x >> material.diffuse.y >> material.diffuse.z;
    stream >> material.specular.x >> material.specular.y >> material.specular.z;
    if (mirror_reflectance_exists) {
      stream >> material.mirror.x >> material.mirror.y >> material.mirror.z;
    }
    if (material.material_type == RawMaterialType::kDielectric) {
      stream >> material.absorption_coefficient.x >>
          material.absorption_coefficient.y >>
          material.absorption_coefficient.z;
    }
    if (material.material_type == RawMaterialType::kConductor ||
        material.material_type == RawMaterialType::kDielectric) {
      stream >> material.refraction_index;
    }
    if (material.material_type == RawMaterialType::kConductor) {
      stream >> material.absorption_index;
    }
    if (phong_exponent_exists) {
      stream >> material.phong_exponent;
    } else {
      material.phong_exponent = 0.0f;
    }

    child = element->FirstChildElement("Roughness");
    if (child) {
      stream << child->GetText() << std::endl;
      stream >> material.roughness;
    }

    materials.push_back(material);
    element = element->NextSiblingElement("Material");
  }

  element = root->FirstChildElement("Textures");
  if (element) {
    element = element->FirstChildElement("Images");
    if (element) {
      element = element->FirstChildElement("Image");
      while (element) {
        RawImage image;
        stream << element->GetText() << std::endl;
        stream >> image.path;
        images.push_back(image);
        element = element->NextSiblingElement("Image");
      }
    }
    element = root->FirstChildElement("Textures");
    element = element->FirstChildElement("TextureMap");
    while (element) {
      RawTextureMap texture_map;
      if (element->Attribute("type", "image") != NULL) {
        texture_map.type = RawTextureMapType::kImage;
      } else if (element->Attribute("type", "perlin") != NULL) {
        texture_map.type = RawTextureMapType::kPerlin;
      } else if (element->Attribute("type", "checkerboard") != NULL) {
        texture_map.type = RawTextureMapType::kCheckerboard;
      }

      child = element->FirstChildElement("ImageId");
      if (child) {
        stream << child->GetText() << std::endl;
        stream >> texture_map.image_id;
      }

      child = element->FirstChildElement("DecalMode");
      if (child) {
        std::string decal_mode = child->GetText();
        if (decal_mode == "replace_kd") {
          texture_map.decal_mode = RawTextureMapDecalMode::kReplaceKd;
        } else if (decal_mode == "blend_kd") {
          texture_map.decal_mode = RawTextureMapDecalMode::kBlendKd;
        } else if (decal_mode == "replace_ks") {
          texture_map.decal_mode = RawTextureMapDecalMode::kReplaceKs;
        } else if (decal_mode == "replace_background") {
          texture_map.decal_mode = RawTextureMapDecalMode::kReplaceBackground;
        } else if (decal_mode == "replace_normal") {
          texture_map.decal_mode = RawTextureMapDecalMode::kReplaceNormal;
        } else if (decal_mode == "bump_normal") {
          texture_map.decal_mode = RawTextureMapDecalMode::kBumpNormal;
        } else if (decal_mode == "replace_all") {
          texture_map.decal_mode = RawTextureMapDecalMode::kReplaceAll;
        }
      }

      child = element->FirstChildElement("Interpolation");
      if (child) {
        std::string interpolation = child->GetText();
        if (interpolation == "nearest") {
          texture_map.interpolation_mode =
              RawTextureMapInterpolationMode::kNearest;
        } else if (interpolation == "bilinear") {
          texture_map.interpolation_mode =
              RawTextureMapInterpolationMode::kBilinear;
        } else if (interpolation == "trilinear") {
          texture_map.interpolation_mode =
              RawTextureMapInterpolationMode::kTrilinear;
        }
      }

      child = element->FirstChildElement("Normalizer");
      if (child) {
        stream << child->GetText() << std::endl;
        stream >> texture_map.normalizer;
      }

      child = element->FirstChildElement("BumpFactor");
      if (child) {
        stream << child->GetText() << std::endl;
        stream >> texture_map.bump_factor;
      }

      child = element->FirstChildElement("NoiseConversion");
      if (child) {
        stream << child->GetText() << std::endl;
        stream >> texture_map.noise_conversion;
      }

      child = element->FirstChildElement("NoiseScale");
      if (child) {
        stream << child->GetText() << std::endl;
        stream >> texture_map.noise_scale;
      }

      child = element->FirstChildElement("NumOctaves");
      if (child) {
        stream << child->GetText() << std::endl;
        stream >> texture_map.num_octaves;
      }

      child = element->FirstChildElement("Scale");
      if (child) {
        stream << child->GetText() << std::endl;
        stream >> texture_map.scale;
      }

      child = element->FirstChildElement("Offset");
      if (child) {
        stream << child->GetText() << std::endl;
        stream >> texture_map.offset;
      }

      child = element->FirstChildElement("BlackColor");
      if (child) {
        stream << child->GetText() << std::endl;
        stream >> texture_map.black_color.x >> texture_map.black_color.y >>
            texture_map.black_color.z;
      }

      child = element->FirstChildElement("WhiteColor");
      if (child) {
        stream << child->GetText() << std::endl;
        stream >> texture_map.white_color.x >> texture_map.white_color.y >>
            texture_map.white_color.z;
      }

      element = element->NextSiblingElement("TextureMap");
    }
  }

  // Get Transformations
  element = root->FirstChildElement("Transformations");
  if (element) {
    auto transformation_element = element->FirstChildElement("Translation");
    RawTranslation translation;
    while (transformation_element) {
      stream << transformation_element->GetText() << std::endl;
      stream >> translation.tx >> translation.ty >> translation.tz;
      translations.push_back(translation);
      transformation_element =
          transformation_element->NextSiblingElement("Translation");
    }

    transformation_element = element->FirstChildElement("Scaling");
    RawScaling scaling;
    while (transformation_element) {
      stream << transformation_element->GetText() << std::endl;
      stream >> scaling.sx >> scaling.sy >> scaling.sz;
      scalings.push_back(scaling);
      transformation_element =
          transformation_element->NextSiblingElement("Scaling");
    }

    transformation_element = element->FirstChildElement("Rotation");
    RawRotation rotation;
    while (transformation_element) {
      stream << transformation_element->GetText() << std::endl;
      stream >> rotation.angle >> rotation.x >> rotation.y >> rotation.z;
      rotations.push_back(rotation);
      transformation_element =
          transformation_element->NextSiblingElement("Rotation");
    }

    transformation_element = element->FirstChildElement("Composite");
    RawComposite composite;
    while (transformation_element) {
      stream << transformation_element->GetText() << std::endl;
      stream >> composite.m[0][0] >> composite.m[0][1] >> composite.m[0][2] >>
          composite.m[0][3];
      stream >> composite.m[1][0] >> composite.m[1][1] >> composite.m[1][2] >>
          composite.m[1][3];
      stream >> composite.m[2][0] >> composite.m[2][1] >> composite.m[2][2] >>
          composite.m[2][3];
      stream >> composite.m[3][0] >> composite.m[3][1] >> composite.m[3][2] >>
          composite.m[3][3];
      composites.push_back(composite);
      transformation_element =
          transformation_element->NextSiblingElement("Composite");
    }
  }

#ifdef PARSER_DEBUG
  std::cout << "\t\tMaterials parsed." << std::endl;
#endif

  // Get VertexData
  element = root->FirstChildElement("VertexData");
  std::string elem_text = element->GetText();
  std::replace(elem_text.begin(), elem_text.end(), '\t', ' ');
  stream << elem_text << std::endl;
  Vec3f vertex;
  while (!(stream >> vertex.x).eof()) {
    stream >> vertex.y >> vertex.z;
    vertex_data.push_back(vertex);
  }
  stream.clear();

#ifdef PARSER_DEBUG
  std::cout << "\t\tVertex data parsed." << std::endl;
#endif

  // Get Meshes
  element = root->FirstChildElement("Objects");
  element = element->FirstChildElement("Mesh");
  while (element) {
    RawMesh mesh;
    mesh.object_id = std::stoi(element->Attribute("id"));

    child = element->FirstChildElement("Material");
    stream << child->GetText() << std::endl;
    stream >> mesh.material_id;

    child = element->FirstChildElement("Transformations");
    if (child) {
      mesh.transformations = std::string{child->GetText()};
    }

    child = element->FirstChildElement("Faces");
    if (child->Attribute("plyFile") != NULL) {
      mesh.ply_filepath = std::string{child->Attribute("plyFile")};
    } else {
      stream << child->GetText() << std::endl;
      RawFace face;
      while (!(stream >> face.v0_id).eof()) {
        stream >> face.v1_id >> face.v2_id;
        mesh.faces.push_back(face);
      }
    }
    stream.clear();

    child = element->FirstChildElement("MotionBlur");
    if (child) {
      stream << child->GetText() << std::endl;
      stream >> mesh.motion_blur.x >> mesh.motion_blur.y >> mesh.motion_blur.z;
    }

    meshes.push_back(mesh);
    mesh.faces.clear();
    element = element->NextSiblingElement("Mesh");
  }
  stream.clear();
#ifdef PARSER_DEBUG
  std::cout << "\t\tMeshes parsed." << std::endl;
#endif

  // Get Mesh Instances
  element = root->FirstChildElement("Objects");
  element = element->FirstChildElement("MeshInstance");
  while (element) {
    RawMeshInstance mesh_instance;
    mesh_instance.object_id = std::stoi(element->Attribute("id"));
    mesh_instance.base_object_id = std::stoi(element->Attribute("baseMeshId"));
    mesh_instance.reset_transform =
        element->BoolAttribute("resetTransform", false);

    child = element->FirstChildElement("Material");
    if (child) {
      stream << child->GetText() << std::endl;
      stream >> mesh_instance.material_id;

    } else {
      mesh_instance.material_id = -1;
    }

    child = element->FirstChildElement("Transformations");
    if (child) {
      mesh_instance.transformations = std::string{child->GetText()};
    }

    child = element->FirstChildElement("MotionBlur");
    if (child) {
      stream << child->GetText() << std::endl;
      stream >> mesh_instance.motion_blur.x >> mesh_instance.motion_blur.y >>
          mesh_instance.motion_blur.z;
    }

    mesh_instances.push_back(mesh_instance);
    element = element->NextSiblingElement("MeshInstance");
  }
  stream.clear();

#ifdef PARSER_DEBUG
  std::cout << "\t\tMesh Instances parsed." << std::endl;
#endif

  // Get Triangles
  element = root->FirstChildElement("Objects");
  element = element->FirstChildElement("Triangle");
  while (element) {
    RawTriangle triangle;
    triangle.object_id = std::stoi(element->Attribute("id"));
    child = element->FirstChildElement("Material");
    stream << child->GetText() << std::endl;
    stream >> triangle.material_id;

    child = element->FirstChildElement("Transformations");
    if (child) {
      triangle.transformations = std::string{child->GetText()};
    }

    child = element->FirstChildElement("Indices");
    stream << child->GetText() << std::endl;
    stream >> triangle.indices.v0_id >> triangle.indices.v1_id >>
        triangle.indices.v2_id;

    child = element->FirstChildElement("MotionBlur");
    if (child) {
      stream << child->GetText() << std::endl;
      stream >> triangle.motion_blur.x >> triangle.motion_blur.y >>
          triangle.motion_blur.z;
    }

    triangles.push_back(triangle);
    element = element->NextSiblingElement("Triangle");
  }

#ifdef PARSER_DEBUG
  std::cout << "\t\tTriangles parsed." << std::endl;
#endif

  // Get Spheres
  element = root->FirstChildElement("Objects");
  element = element->FirstChildElement("Sphere");
  while (element) {
    RawSphere sphere;
    sphere.object_id = std::stoi(element->Attribute("id"));
    child = element->FirstChildElement("Material");
    stream << child->GetText() << std::endl;
    stream >> sphere.material_id;

    child = element->FirstChildElement("Transformations");
    if (child) {
      sphere.transformations = std::string{child->GetText()};
    }

    child = element->FirstChildElement("Center");
    stream << child->GetText() << std::endl;
    stream >> sphere.center_vertex_id;

    child = element->FirstChildElement("Radius");
    stream << child->GetText() << std::endl;
    stream >> sphere.radius;

    child = element->FirstChildElement("MotionBlur");
    if (child) {
      stream << child->GetText() << std::endl;
      stream >> sphere.motion_blur.x >> sphere.motion_blur.y >>
          sphere.motion_blur.z;
    }

    spheres.push_back(sphere);
    element = element->NextSiblingElement("Sphere");
  }
#ifdef PARSER_DEBUG
  std::cout << "\t\tSpheres parsed." << std::endl;
#endif
}

void parser::RawScene::loadFromJSON(const std::string &filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Error: The JSON file cannot be loaded.");
  }

  nlohmann::json json_data;
  file >> json_data;

  json_data = json_data["Scene"];

  // Parse background color
  if (json_data.contains("BackgroundColor")) {
    auto bg_color = json_data["BackgroundColor"].get<std::vector<float>>();
    background_color.x = bg_color[0];
    background_color.y = bg_color[1];
    background_color.z = bg_color[2];
  } else {
    background_color = {0, 0, 0};
  }

  // Parse shadow ray epsilon
  if (json_data.contains("ShadowRayEpsilon")) {
    shadow_ray_epsilon = json_data["ShadowRayEpsilon"].get<float>();
  } else {
    shadow_ray_epsilon = 0.001f;
  }

  // Parse max recursion depth
  if (json_data.contains("MaxRecursionDepth")) {
    max_recursion_depth = json_data["MaxRecursionDepth"].get<int>();
  } else {
    max_recursion_depth = 0;
  }

  // Parse intersection test epsilon
  if (json_data.contains("IntersectionTestEpsilon")) {
    intersection_test_epsilon = json_data["IntersectionTestEpsilon"].get<float>();
  } else {
    intersection_test_epsilon = 0.001f;
  }

  // Parse Cameras
  if (json_data.contains("Cameras")) {
    for (const auto& cam : json_data["Cameras"]["Camera"]) {
      RawCamera camera;
      cam.contains("_type") && cam["_type"].get<std::string>() == "lookAt" ? camera.look_at_camera = true : camera.look_at_camera = false;
      auto pos = cam["Position"].get<std::vector<float>>();
      camera.position = {pos[0], pos[1], pos[2]};
      if (cam.contains("Gaze")) {
        auto gaze = cam["Gaze"].get<std::vector<float>>();
        camera.gaze = {gaze[0], gaze[1], gaze[2]};
      }
      if (cam.contains("GazePoint")) {
        auto gp = cam["GazePoint"].get<std::vector<float>>();
        camera.gaze_point = {gp[0], gp[1], gp[2]};
      }
      auto up = cam["Up"].get<std::vector<float>>();
      camera.up = {up[0], up[1], up[2]};
      if (cam.contains("NearPlane")) {
        auto near_plane = cam["NearPlane"].get<std::vector<float>>();
        camera.near_plane = {near_plane[0], near_plane[1], near_plane[2], near_plane[3]};
      }
      if (cam.contains("FovY")) {
        camera.fov_y = cam["FovY"].get<float>();
      }
      camera.near_distance = cam["NearDistance"].get<float>();
      camera.image_width = cam["ImageResolution"][0].get<int>();
      camera.image_height = cam["ImageResolution"][1].get<int>();
      camera.image_name = cam["ImageName"].get<std::string>();
      camera.num_samples = cam.contains("NumSamples") ? cam["NumSamples"].get<int>() : 0;
      camera.focus_distance = cam.contains("FocusDistance") ? cam["FocusDistance"].get<float>() : 0;
      camera.aperture_size = cam.contains("ApertureSize") ? cam["ApertureSize"].get<float>() : 0;
      cameras.push_back(camera);
    }
  }

  // Parse Lights
  if (json_data.contains("Lights")) {
    auto lights = json_data["Lights"];
    auto amb = lights["AmbientLight"].get<std::vector<float>>();
    ambient_light = {amb[0], amb[1], amb[2]};
    
    for (const auto& light : lights["PointLight"]) {
      RawPointLight point_light;
      auto pos = light["Position"].get<std::vector<float>>();
      auto inten = light["Intensity"].get<std::vector<float>>();
      point_light.position = {pos[0], pos[1], pos[2]};
      point_light.intensity = {inten[0], inten[1], inten[2]};
      point_lights.push_back(point_light);
    }
    for (const auto& light : lights["AreaLight"]) {
      RawAreaLight area_light;
      auto pos = light["Position"].get<std::vector<float>>();
      auto norm = light["Normal"].get<std::vector<float>>();
      auto radiance = light["Radiance"].get<std::vector<float>>();
      area_light.position = {pos[0], pos[1], pos[2]};
      area_light.normal = {norm[0], norm[1], norm[2]};
      area_light.size = light["Size"].get<float>();
      area_light.radiance = {radiance[0], radiance[1], radiance[2]};
      area_lights.push_back(area_light);
    }
  }

  // Parse Materials
  if (json_data.contains("Materials")) {
    auto mats = json_data["Materials"]["Material"];
    for (const auto& material_obj : mats) {
      RawMaterial material;
      if (material_obj.contains("_type")) {
        if (material_obj["_type"] == "mirror") {
          material.material_type = RawMaterialType::kMirror;
        } else if (material_obj["_type"] == "conductor") {
          material.material_type = RawMaterialType::kConductor;
        } else if (material_obj["_type"] == "dielectric") {
          material.material_type = RawMaterialType::kDielectric;
        } else {
          material.material_type = RawMaterialType::kDefault;
        }
      } else {
        material.material_type = RawMaterialType::kDefault;
      }

      auto amb = material_obj["AmbientReflectance"].get<std::vector<float>>();
      auto diff = material_obj["DiffuseReflectance"].get<std::vector<float>>();
      auto spec = material_obj["SpecularReflectance"].get<std::vector<float>>();

      material.ambient = {amb[0], amb[1], amb[2]};
      material.diffuse = {diff[0], diff[1], diff[2]};
      material.specular = {spec[0], spec[1], spec[2]};
      if (material_obj.contains("MirrorReflectance")) {
        auto mirror = material_obj["MirrorReflectance"].get<std::vector<float>>();
        material.mirror = {mirror[0], mirror[1], mirror[2]};
      }
      if (material.material_type == RawMaterialType::kDielectric && material_obj.contains("AbsorptionCoefficient")) {
        auto ac = material_obj["AbsorptionCoefficient"].get<std::vector<float>>();
        material.absorption_coefficient = {ac[0], ac[1], ac[2]};
      }
      if ((material.material_type == RawMaterialType::kConductor || material.material_type == RawMaterialType::kDielectric) && material_obj.contains("RefractionIndex")) {
        material.refraction_index = material_obj["RefractionIndex"].get<float>();
      }
      if (material.material_type == RawMaterialType::kConductor && material_obj.contains("AbsorptionIndex")) {
        material.absorption_index = material_obj["AbsorptionIndex"].get<float>();
      }
      material.phong_exponent = material_obj.contains("PhongExponent") ? material_obj["PhongExponent"].get<float>() : 0.0f;
      material.roughness = material_obj.contains("Roughness") ? material_obj["Roughness"].get<float>() : 0.0f;

      materials.push_back(material);
    }
  }

  // Parse Textures
  if (json_data.contains("Textures")) {
    auto tex = json_data["Textures"];
    if (tex.contains("Images")) {
      for (const auto& img : tex["Images"]["Image"]) {
        RawImage image;
        image.path = img.get<std::string>();
        images.push_back(image);
      }
    }
    if (tex.contains("TextureMap")) {
      for (const auto& tm : tex["TextureMap"]) {
        RawTextureMap texture_map;
        if (tm.contains("_type")) {
          if (tm["_type"] == "image") {
            texture_map.type = RawTextureMapType::kImage;
          } else if (tm["_type"] == "perlin") {
            texture_map.type = RawTextureMapType::kPerlin;
          } else if (tm["_type"] == "checkerboard") {
            texture_map.type = RawTextureMapType::kCheckerboard;
          }
        }

        if (tm.contains("ImageId")) {
          texture_map.image_id = tm["ImageId"].get<int>();
        }

        if (tm.contains("DecalMode")) {
          std::string decal_mode = tm["DecalMode"].get<std::string>();
          if (decal_mode == "replace_kd") {
            texture_map.decal_mode = RawTextureMapDecalMode::kReplaceKd;
          } else if (decal_mode == "blend_kd") {
            texture_map.decal_mode = RawTextureMapDecalMode::kBlendKd;
          } else if (decal_mode == "replace_ks") {
            texture_map.decal_mode = RawTextureMapDecalMode::kReplaceKs;
          } else if (decal_mode == "replace_background") {
            texture_map.decal_mode = RawTextureMapDecalMode::kReplaceBackground;
          } else if (decal_mode == "replace_normal") {
            texture_map.decal_mode = RawTextureMapDecalMode::kReplaceNormal;
          } else if (decal_mode == "bump_normal") {
            texture_map.decal_mode = RawTextureMapDecalMode::kBumpNormal;
          } else if (decal_mode == "replace_all") {
            texture_map.decal_mode = RawTextureMapDecalMode::kReplaceAll;
          }
        }

        if (tm.contains("Interpolation")) {
          std::string interpolation = tm["Interpolation"].get<std::string>();
          if (interpolation == "nearest") {
            texture_map.interpolation_mode = RawTextureMapInterpolationMode::kNearest;
          } else if (interpolation == "bilinear") {
            texture_map.interpolation_mode = RawTextureMapInterpolationMode::kBilinear;
          }
          else if (interpolation == "trilinear") {
            texture_map.interpolation_mode = RawTextureMapInterpolationMode::kTrilinear;
          }
        }

        texture_map.normalizer = tm.contains("Normalizer") ? tm["Normalizer"].get<float>() : 1.0f;
        texture_map.bump_factor = tm.contains("BumpFactor") ? tm["BumpFactor"].get<float>() : 1.0f;
        texture_map.noise_conversion = tm.contains("NoiseConversion") ? tm["NoiseConversion"].get<float>() : 1.0f;
        texture_map.noise_scale = tm.contains("NoiseScale") ? tm["NoiseScale"].get<float>() : 1.0f;
        texture_map.num_octaves = tm.contains("NumOctaves") ? tm["NumOctaves"].get<int>() : 1;
        if (tm.contains("Scale")) {
          texture_map.scale = tm["Scale"].get<float>();
        } else {
          texture_map.scale = 1.0f;
        }
        if (tm.contains("Offset")) {
          texture_map.offset = tm["Offset"].get<float>();
        } else {
          texture_map.offset = 0.0f;
        }
        if (tm.contains("BlackColor")) {
          auto black = tm["BlackColor"].get<std::vector<float>>();
          texture_map.black_color = {black[0], black[1], black[2]};
        } else {
          texture_map.black_color = {0.0f, 0.0f, 0.0f};
        }
        if (tm.contains("WhiteColor")) {
          auto white = tm["WhiteColor"].get<std::vector<float>>();
          texture_map.white_color = {white[0], white[1], white[2]};
        } else {
          texture_map.white_color = {1.0f, 1.0f, 1.0f};
        }

        texture_maps.push_back(texture_map);
      }
    }
  }

  

  // Parse Transformations
  if (json_data.contains("Transformations")) {
    auto transforms = json_data["Transformations"];
    for (const auto& t : transforms["Translation"]) {
      RawTranslation translation;
      translation.tx = t["tx"].get<float>();
      translation.ty = t["ty"].get<float>();
      translation.tz = t["tz"].get<float>();
      translations.push_back(translation);
    }
    for (const auto& t : transforms["Scaling"]) {
      RawScaling scaling;
      scaling.sx = t["sx"].get<float>();
      scaling.sy = t["sy"].get<float>();
      scaling.sz = t["sz"].get<float>();
      scalings.push_back(scaling);
    }
    for (const auto& t : transforms["Rotation"]) {
      RawRotation rotation;
      rotation.angle = t["angle"].get<float>();
      rotation.x = t["x"].get<float>();
      rotation.y = t["y"].get<float>();
      rotation.z = t["z"].get<float>();
      rotations.push_back(rotation);
    }
    for (const auto& t : transforms["Composite"]) {
      RawComposite composite;
      auto m = t["m"].get<std::vector<std::vector<float>>>();
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
          composite.m[i][j] = m[i][j];
        }
      }
      composites.push_back(composite);
    }
  }

  // Parse VertexData
  if (json_data.contains("VertexData")) {
    for (const auto& v : json_data["VertexData"]) {
      Vec3f vertex;
      std::vector<float> vec = v.get<std::vector<float>>();
      vertex.x = vec[0];
      vertex.y = vec[1];
      vertex.z = vec[2];
      vertex_data.push_back(vertex);
    }
  }

  // Parse Meshes
  if (json_data.contains("Objects")) {
    for (const auto& obj : json_data["Objects"]["Mesh"]) {
      RawMesh mesh;
      mesh.object_id = obj["_id"];
      mesh.material_id = obj["Material"];
      mesh.transformations = obj.contains("Transformations") ? obj["Transformations"] : "";
      if (obj.contains("Faces")) {
        if (obj["Faces"].contains("_plyFile")) {
          mesh.ply_filepath = obj["Faces"]["_plyFile"];
        } else {
          for (const auto& f : obj["Faces"]) {
            RawFace face;
            auto f_vec = f.get<std::vector<int>>();
            face.v0_id = f_vec[0];
            face.v1_id = f_vec[1];
            face.v2_id = f_vec[2];
            mesh.faces.push_back(face);
          }
        }
      }
      if (obj.contains("MotionBlur")) {
        auto motion_blur = obj["MotionBlur"].get<std::vector<float>>();
        mesh.motion_blur = {motion_blur[0], motion_blur[1], motion_blur[2]};
      }
      meshes.push_back(mesh);
    }
  }

  // Parse Mesh Instances
  if (json_data.contains("Objects")) {
    for (const auto& mi : json_data["Objects"]["MeshInstance"]) {
      RawMeshInstance mesh_instance;
      mesh_instance.object_id = mi["_id"].get<int>();
      mesh_instance.base_object_id = mi["baseMeshId"].get<int>();
      mesh_instance.reset_transform = mi.contains("resetTransform") ? mi["resetTransform"].get<bool>() : false;
      mesh_instance.material_id = mi.contains("Material") ? mi["Material"].get<int>() : -1;
      mesh_instance.transformations = mi.contains("Transformations") ? mi["Transformations"] : "";
      if (mi.contains("MotionBlur")) {
        auto motion_blur = mi["MotionBlur"].get<std::vector<float>>();
        mesh_instance.motion_blur = {motion_blur[0], motion_blur[1], motion_blur[2]};
      }
      mesh_instances.push_back(mesh_instance);
    }
  }

  // Parse Triangles
  if (json_data.contains("Objects")) {
    for (const auto& t : json_data["Objects"]["Triangle"]) {
      RawTriangle triangle;
      triangle.object_id = t["_id"].get<int>();
      triangle.material_id = t["Material"].get<int>();
      triangle.transformations = t.contains("Transformations") ? t["Transformations"] : "";
      triangle.indices.v0_id = t["Indices"][0].get<int>();
      triangle.indices.v1_id = t["Indices"][1].get<int>();
      triangle.indices.v2_id = t["Indices"][2].get<int>();
      if (t.contains("MotionBlur")) {
        auto motion_blur = t["MotionBlur"].get<std::vector<float>>();
        triangle.motion_blur = {motion_blur[0], motion_blur[1], motion_blur[2]};
      }
      triangles.push_back(triangle);
    }
  }

  // Parse Spheres
  if (json_data.contains("Objects")) {
    for (const auto& s : json_data["Objects"]["Sphere"]) {
      RawSphere sphere;
      sphere.object_id = s["_id"].get<int>();
      sphere.material_id = s["Material"].get<int>();
      sphere.transformations = s.contains("Transformations") ? s["Transformations"] : "";
      sphere.center_vertex_id = s["Center"].get<int>();
      sphere.radius = s["Radius"].get<float>();
      if (s.contains("MotionBlur")) {
        sphere.motion_blur = {s["MotionBlur"][0].get<float>(), s["MotionBlur"][1].get<float>(), s["MotionBlur"][2].get<float>()};
      }
      spheres.push_back(sphere);
    }
  }

  file.close();
}