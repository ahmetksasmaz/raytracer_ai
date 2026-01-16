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

namespace ply_reader{
  PlyProperty vert_props[3] = {
      /* list of property information for a vertex */
      {const_cast<char*>("x"), PLY_FLOAT, PLY_FLOAT, offsetof(Vertex, x), 0, 0, 0,
      0},
      {const_cast<char*>("y"), PLY_FLOAT, PLY_FLOAT, offsetof(Vertex, y), 0, 0, 0,
      0},
      {const_cast<char*>("z"), PLY_FLOAT, PLY_FLOAT, offsetof(Vertex, z), 0, 0, 0,
      0},
  };

  PlyProperty vert_props_uv[5] = {
      {const_cast<char*>("x"), PLY_FLOAT, PLY_FLOAT, offsetof(VertexWithUV, x), 0, 0, 0, 0},
      {const_cast<char*>("y"), PLY_FLOAT, PLY_FLOAT, offsetof(VertexWithUV, y), 0, 0, 0, 0},
      {const_cast<char*>("z"), PLY_FLOAT, PLY_FLOAT, offsetof(VertexWithUV, z), 0, 0, 0, 0},
      {const_cast<char*>("u"), PLY_FLOAT, PLY_FLOAT, offsetof(VertexWithUV, u), 0, 0, 0, 0},
      {const_cast<char*>("v"), PLY_FLOAT, PLY_FLOAT, offsetof(VertexWithUV, v), 0, 0, 0, 0},
  };

  PlyProperty vert_props_uvn[8] = {
      {const_cast<char*>("x"), PLY_FLOAT, PLY_FLOAT, offsetof(VertexWithUVN, x), 0, 0, 0, 0},
      {const_cast<char*>("y"), PLY_FLOAT, PLY_FLOAT, offsetof(VertexWithUVN, y), 0, 0, 0, 0},
      {const_cast<char*>("z"), PLY_FLOAT, PLY_FLOAT, offsetof(VertexWithUVN, z), 0, 0, 0, 0},
      {const_cast<char*>("z"), PLY_FLOAT, PLY_FLOAT, offsetof(VertexWithUVN, nx), 0, 0, 0, 0},
      {const_cast<char*>("z"), PLY_FLOAT, PLY_FLOAT, offsetof(VertexWithUVN, ny), 0, 0, 0, 0},
      {const_cast<char*>("z"), PLY_FLOAT, PLY_FLOAT, offsetof(VertexWithUVN, nz), 0, 0, 0, 0},
      {const_cast<char*>("u"), PLY_FLOAT, PLY_FLOAT, offsetof(VertexWithUVN, u), 0, 0, 0, 0},
      {const_cast<char*>("v"), PLY_FLOAT, PLY_FLOAT, offsetof(VertexWithUVN, v), 0, 0, 0, 0},
  };

  PlyProperty face_props[1] = {
      /* list of property information for a vertex */
      {const_cast<char*>("vertex_indices"), PLY_INT, PLY_INT,
      offsetof(Face, verts), 1, PLY_UCHAR, PLY_UCHAR, offsetof(Face, nverts)},
  };

  PlyProperty face_props2[1] = {
      /* list of property information for a vertex */
      {const_cast<char*>("vertex_index"), PLY_INT, PLY_INT,
      offsetof(Face, verts), 1, PLY_UCHAR, PLY_UCHAR, offsetof(Face, nverts)},
  };
}

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
        std::string noise_conversion = child->GetText();
        if (noise_conversion == "abs_val") {
          texture_map.noise_conversion = RawTextureMapNoiseConversionType::kAbsVal;
        } else if (noise_conversion == "linear") {
          texture_map.noise_conversion = RawTextureMapNoiseConversionType::kLinear;
        }
        
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

  // Get TextCoordData
  element = root->FirstChildElement("TextCoordData");
  elem_text = element->GetText();
  std::replace(elem_text.begin(), elem_text.end(), '\t', ' ');
  stream << elem_text << std::endl;
  Vec2f texcoord;
  while (!(stream >> texcoord.x).eof()) {
    stream >> texcoord.y;
    tex_coord_data.push_back(texcoord);
  }
  stream.clear();

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

    child = element->FirstChildElement("Textures");
    if (child) {
      mesh.textures = std::string{child->GetText()};
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
    child = element->FirstChildElement("Textures");
    if (child) {
      mesh_instance.textures = std::string{child->GetText()};
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

    child = element->FirstChildElement("Textures");
    if (child) {
      triangle.textures = std::string{child->GetText()};
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

    child = element->FirstChildElement("Textures");
    if (child) {
      sphere.textures = std::string{child->GetText()};
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
    auto bg_color = json_data["BackgroundColor"].get<std::string>();
    std::stringstream ss(bg_color);
    ss >> background_color.x >> background_color.y >> background_color.z;
  } else {
    background_color = {0, 0, 0};
  }

  // Parse shadow ray epsilon
  if (json_data.contains("ShadowRayEpsilon")) {
    shadow_ray_epsilon = std::stof(json_data["ShadowRayEpsilon"].get<std::string>());
  } else {
    shadow_ray_epsilon = 0.00001;
  }
  
  // Parse max recursion depth
  if (json_data.contains("MaxRecursionDepth")) {
    max_recursion_depth = std::stoi(json_data["MaxRecursionDepth"].get<std::string>());
  } else {
    max_recursion_depth = 1;
  }


  // Parse intersection test epsilon
  if (json_data.contains("IntersectionTestEpsilon")) {
    intersection_test_epsilon = std::stof(json_data["IntersectionTestEpsilon"].get<std::string>());
  } else {
    intersection_test_epsilon = 0.001f;
  }

  // Parse Cameras
  if (json_data.contains("Cameras")) {
    // Type check
    std::vector<nlohmann::json> json_cams;
    try {
      auto cam_id = json_data["Cameras"]["Camera"]["_id"].get<std::string>();
      json_cams.push_back(json_data["Cameras"]["Camera"]);
    } catch (nlohmann::json::type_error& e) {
      for (const auto& cam : json_data["Cameras"]["Camera"]) {
        json_cams.push_back(cam);
      }
    }

    for (const auto& cam : json_cams) {
      RawCamera camera;
      cam.contains("_type") && cam["_type"].get<std::string>() == "lookAt" ? camera.look_at_camera = true : camera.look_at_camera = false;
      auto pos = cam["Position"].get<std::string>();
      std::stringstream ss(pos);
      ss >> camera.position.x >> camera.position.y >> camera.position.z;
      if (cam.contains("Gaze")) {
        auto gaze = cam["Gaze"].get<std::string>();
        std::stringstream ss_gaze(gaze);
        ss_gaze >> camera.gaze.x >> camera.gaze.y >> camera.gaze.z;
      }
      if (cam.contains("GazePoint")) {
        auto gp = cam["GazePoint"].get<std::string>();
        std::stringstream ss_gp(gp);
        ss_gp >> camera.gaze_point.x >> camera.gaze_point.y >> camera.gaze_point.z;
      }
      auto up = cam["Up"].get<std::string>();
      std::stringstream ss_up(up);
      ss_up >> camera.up.x >> camera.up.y >> camera.up.z;
      if (cam.contains("NearPlane")) {
        auto near_plane = cam["NearPlane"].get<std::string>();
        std::stringstream ss_near(near_plane);
        ss_near >> camera.near_plane.x >> camera.near_plane.y >> camera.near_plane.z >> camera.near_plane.w;
      }
      if (cam.contains("FovY")) {
        camera.fov_y = std::stof(cam["FovY"].get<std::string>());
      }
      camera.transformations = cam.contains("Transformations") ? cam["Transformations"].get<std::string>() : "";
      camera.near_distance = std::stof(cam["NearDistance"].get<std::string>());
      auto img_res = cam["ImageResolution"].get<std::string>();
      std::stringstream ss_img_res(img_res);
      ss_img_res >> camera.image_width >> camera.image_height;
      camera.image_name = cam["ImageName"].get<std::string>();
      camera.num_samples = cam.contains("NumSamples") ? std::stoi(cam["NumSamples"].get<std::string>()) : 0;
      camera.focus_distance = cam.contains("FocusDistance") ? std::stof(cam["FocusDistance"].get<std::string>()) : 0;
      camera.aperture_size = cam.contains("ApertureSize") ? std::stof(cam["ApertureSize"].get<std::string>()) : 0;
      camera.left_handed = cam.contains("_handedness") ? cam["_handedness"].get<std::string>() == "left" : false;

      // Read tonemappings
      if(cam.contains("Tonemap")){
        std::vector<nlohmann::json> json_tonemaps;
        try {
          auto tm_tmo = cam["Tonemap"]["TMO"].get<std::string>();
          json_tonemaps.push_back(cam["Tonemap"]);
        } catch (nlohmann::json::type_error& e) {
          for (const auto& tm : cam["Tonemap"]) {
            json_tonemaps.push_back(tm);
          }
        }

        for(auto & tm : json_tonemaps){
          RawToneMapping tonemap;
          std::string algorithm = tm["TMO"].get<std::string>();
          if(algorithm == "Photographic"){
            tonemap.algorithm = RawToneMappingAlgorithm::kPhotographic;
          } else if (algorithm == "Filmic"){
            tonemap.algorithm = RawToneMappingAlgorithm::kFilmic;
          } else if (algorithm == "ACES"){
            tonemap.algorithm = RawToneMappingAlgorithm::kACES;
          }
          std::string options = tm["TMOOptions"].get<std::string>();
          std::stringstream ss_options(options);
          ss_options >> tonemap.key >> tonemap.burn;

          tonemap.saturation = tm.contains("Saturation") ? std::stof(tm["Saturation"].get<std::string>()) : 1.0f;
          tonemap.gamma = tm.contains("Gamma") ? std::stof(tm["Gamma"].get<std::string>()) : 1.0f;
          tonemap.extension = tm.contains("Extension") ? tm["Extension"].get<std::string>() : ".png";

          camera.tone_mappings.push_back(tonemap);
        }

      }
      
      cameras.push_back(camera);
    }
  }

  // Parse Lights
  if (json_data.contains("Lights")) {
    auto lights = json_data["Lights"];
    if (lights.contains("AmbientLight")) {
      auto amb = lights["AmbientLight"].get<std::string>();
      std::stringstream ss_amb(amb);
      ss_amb >> ambient_light.x >> ambient_light.y >> ambient_light.z;
    }
    else{
      ambient_light = {0, 0, 0};
    }
    
    // Type check
    if (json_data["Lights"].contains("PointLight")) {
      std::vector<nlohmann::json> json_point_lights;
      try {
        auto light_id = json_data["Lights"]["PointLight"]["_id"].get<std::string>();
        json_point_lights.push_back(json_data["Lights"]["PointLight"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& cam : json_data["Lights"]["PointLight"]) {
          json_point_lights.push_back(cam);
        }
      }
      
      for (const auto& light : json_point_lights) {
        RawPointLight point_light;
        auto pos = light["Position"].get<std::string>();
        auto inten = light["Intensity"].get<std::string>();
        point_light.transformations = light.contains("Transformations") ? light["Transformations"].get<std::string>() : "";
        std::stringstream ss_pos(pos);
        ss_pos >> point_light.position.x >> point_light.position.y >> point_light.position.z;
        std::stringstream ss_inten(inten);
        ss_inten >> point_light.intensity.x >> point_light.intensity.y >> point_light.intensity.z;
        point_lights.push_back(point_light);
      }
    }
    
    if (json_data["Lights"].contains("AreaLight")){
      // Type check
      std::vector<nlohmann::json> json_area_lights;
      try {
        auto light_id = json_data["Lights"]["AreaLight"]["_id"].get<std::string>();
        json_area_lights.push_back(json_data["Lights"]["AreaLight"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& cam : json_data["Lights"]["AreaLight"]) {
          json_area_lights.push_back(cam);
        }
      }
      
      for (const auto& light : json_area_lights) {
        RawAreaLight area_light;
        auto pos = light["Position"].get<std::string>();
        auto norm = light["Normal"].get<std::string>();
        auto radiance = light["Radiance"].get<std::string>();
        area_light.transformations = light.contains("Transformations") ? light["Transformations"].get<std::string>() : "";
        std::stringstream ss_pos(pos);
        ss_pos >> area_light.position.x >> area_light.position.y >> area_light.position.z;
        std::stringstream ss_norm(norm);
        ss_norm >> area_light.normal.x >> area_light.normal.y >> area_light.normal.z;
        area_light.size = (FP_PRECISION)(std::stof(light["Size"].get<std::string>()));
        std::stringstream ss_rad(radiance);
        ss_rad >> area_light.radiance.x >> area_light.radiance.y >> area_light.radiance.z;
        area_lights.push_back(area_light);
      }
    }

    if(json_data["Lights"].contains("DirectionalLight")){
      // Type check
      std::vector<nlohmann::json> json_directional_lights;
      try {
        auto light_id = json_data["Lights"]["DirectionalLight"]["_id"].get<std::string>();
        json_directional_lights.push_back(json_data["Lights"]["DirectionalLight"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& cam : json_data["Lights"]["DirectionalLight"]) {
          json_directional_lights.push_back(cam);
        }
      }
      
      for (const auto& light : json_directional_lights) {
        RawDirectionalLight directional_light;
        auto dir = light["Direction"].get<std::string>();
        auto radiance = light["Radiance"].get<std::string>();
        std::stringstream ss_dir(dir);
        ss_dir >> directional_light.direction.x >> directional_light.direction.y >> directional_light.direction.z;
        std::stringstream ss_rad(radiance);
        ss_rad >> directional_light.radiance.x >> directional_light.radiance.y >> directional_light.radiance.z;
        directional_lights.push_back(directional_light);
      }
    }

    if(json_data["Lights"].contains("SpotLight")){
      // Type check
      std::vector<nlohmann::json> json_spot_lights;
      try {
        auto light_id = json_data["Lights"]["SpotLight"]["_id"].get<std::string>();
        json_spot_lights.push_back(json_data["Lights"]["SpotLight"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& cam : json_data["Lights"]["SpotLight"]) {
          json_spot_lights.push_back(cam);
        }
      }
      
      for (const auto& light : json_spot_lights) {
        RawSpotLight spot_light;
        auto pos = light["Position"].get<std::string>();
        auto dir = light["Direction"].get<std::string>();
        auto intensity = light["Intensity"].get<std::string>();
        std::stringstream ss_pos(pos);
        ss_pos >> spot_light.position.x >> spot_light.position.y >> spot_light.position.z;
        std::stringstream ss_dir(dir);
        ss_dir >> spot_light.direction.x >> spot_light.direction.y >> spot_light.direction.z;
        std::stringstream ss_rad(intensity);
        ss_rad >> spot_light.intensity.x >> spot_light.intensity.y >> spot_light.intensity.z;
        spot_light.coverage_angle = (FP_PRECISION)(std::stof(light["CoverageAngle"].get<std::string>()));
        spot_light.falloff_angle = (FP_PRECISION)(std::stof(light["FalloffAngle"].get<std::string>()));
        spot_lights.push_back(spot_light);
      }
    }

    if(json_data["Lights"].contains("SphericalDirectionalLight")){
        auto light = json_data["Lights"]["SphericalDirectionalLight"];
        spherical_directional_light.image_id = std::stoi(light["ImageId"].get<std::string>());
        std::string type = light["_type"].get<std::string>();
        std::string sampler = light.contains("Sampler") ? light["Sampler"].get<std::string>() : "";
        if(type == "latlong"){
          spherical_directional_light.type = RawEnvironmentMapType::kLatLong;
        } else if (type == "probe"){
          spherical_directional_light.type = RawEnvironmentMapType::kProbe;
        }
        if(sampler == "uniform"){
          spherical_directional_light.sampler = RawEnvironmentMapSampler::kUniform;
        } else if (sampler == "cosine"){
          spherical_directional_light.sampler = RawEnvironmentMapSampler::kCosine;
        }else{
          spherical_directional_light.sampler = RawEnvironmentMapSampler::kCosine;
        }
        spherical_directional_light.exists = true;
    }

  }
  // Parse BRDFs
  if (json_data.contains("BRDFs")) {
    nlohmann::json brdfs_json = json_data["BRDFs"];
    if(brdfs_json.contains("OriginalBlinnPhong")){
      std::vector<nlohmann::json> json_original_bps;
      try {
        auto brdf_id = brdfs_json["OriginalBlinnPhong"]["_id"].get<std::string>();
        json_original_bps.push_back(brdfs_json["OriginalBlinnPhong"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& brdf : brdfs_json["OriginalBlinnPhong"]) {
          json_original_bps.push_back(brdf);
        }
      }
      for(const auto& brdf : json_original_bps){
        RawBRDF obp;
        obp.type = RawBRDFType::kOriginalBlinnPhong;
        obp.id = std::stoi(brdf["_id"].get<std::string>());
        obp.normalized = brdf.contains("_normalized") ? brdf["_normalized"].get<std::string>() == "true" : false;
        obp.exponent = std::stof(brdf["Exponent"].get<std::string>());
        brdfs.push_back(obp);
      }
    }
    if(brdfs_json.contains("ModifiedBlinnPhong")){
      std::vector<nlohmann::json> json_modified_bps;
      try {
        auto brdf_id = brdfs_json["ModifiedBlinnPhong"]["_id"].get<std::string>();
        json_modified_bps.push_back(brdfs_json["ModifiedBlinnPhong"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& brdf : brdfs_json["ModifiedBlinnPhong"]) {
          json_modified_bps.push_back(brdf);
        }
      }
      for(const auto& brdf : json_modified_bps){
        RawBRDF mbp;
        mbp.type = RawBRDFType::kModifiedBlinnPhong;
        mbp.id = std::stoi(brdf["_id"].get<std::string>());
        mbp.normalized = brdf.contains("_normalized") ? brdf["_normalized"].get<std::string>() == "true" : false;
        mbp.exponent = std::stof(brdf["Exponent"].get<std::string>());
        brdfs.push_back(mbp);
      }
    }
    if(brdfs_json.contains("OriginalPhong")){
      std::vector<nlohmann::json> json_original_phongs;
      try {
        auto brdf_id = brdfs_json["OriginalPhong"]["_id"].get<std::string>();
        json_original_phongs.push_back(brdfs_json["OriginalPhong"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& brdf : brdfs_json["OriginalPhong"]) {
          json_original_phongs.push_back(brdf);
        }
      }
      for(const auto& brdf : json_original_phongs){
        RawBRDF op;
        op.type = RawBRDFType::kOriginalPhong;
        op.id = std::stoi(brdf["_id"].get<std::string>());
        op.normalized = brdf.contains("_normalized") ? brdf["_normalized"].get<std::string>() == "true" : false;
        op.exponent = std::stof(brdf["Exponent"].get<std::string>());
        brdfs.push_back(op);
      }
    }
    if(brdfs_json.contains("ModifiedPhong")){
      std::vector<nlohmann::json> json_modified_phongs;
      try {
        auto brdf_id = brdfs_json["ModifiedPhong"]["_id"].get<std::string>();
        json_modified_phongs.push_back(brdfs_json["ModifiedPhong"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& brdf : brdfs_json["ModifiedPhong"]) {
          json_modified_phongs.push_back(brdf);
        }
      }
      for(const auto& brdf : json_modified_phongs){
        RawBRDF mp;
        mp.type = RawBRDFType::kModifiedPhong;
        mp.id = std::stoi(brdf["_id"].get<std::string>());
        mp.normalized = brdf.contains("_normalized") ? brdf["_normalized"].get<std::string>() == "true" : false;
        mp.exponent = std::stof(brdf["Exponent"].get<std::string>());
        brdfs.push_back(mp);
      }
    }
    if(brdfs_json.contains("TorranceSparrow")){
      std::vector<nlohmann::json> json_torrance_sparrows;
      try {
        auto brdf_id = brdfs_json["TorranceSparrow"]["_id"].get<std::string>();
        json_torrance_sparrows.push_back(brdfs_json["TorranceSparrow"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& brdf : brdfs_json["TorranceSparrow"]) {
          json_torrance_sparrows.push_back(brdf);
        }
      }
      for(const auto& brdf : json_torrance_sparrows){
        RawBRDF ct;
        ct.type = RawBRDFType::kTorranceSparrow;
        ct.kd_fresnel = brdf.contains("_kdfresnel") ? brdf["_kdfresnel"].get<std::string>() == "true" : false;
        ct.id = std::stoi(brdf["_id"].get<std::string>());
        ct.exponent = std::stof(brdf["Exponent"].get<std::string>());
        brdfs.push_back(ct);
      }
    }
    std::sort(brdfs.begin(), brdfs.end(), [](const RawBRDF &a, const RawBRDF &b) {
      return a.id < b.id;
    });
  }

  // Parse Materials
  if (json_data.contains("Materials")) {
    // Type check
    std::vector<nlohmann::json> json_materials;
    try {
      auto light_id = json_data["Materials"]["Material"]["_id"].get<std::string>();
      json_materials.push_back(json_data["Materials"]["Material"]);
    } catch (nlohmann::json::type_error& e) {
      for (const auto& cam : json_data["Materials"]["Material"]) {
        json_materials.push_back(cam);
      }
    }
    for (const auto& material_obj : json_materials) {
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
      if(material_obj.contains("_BRDF")){
        material.brdf_id = std::stoi(material_obj["_BRDF"].get<std::string>());
      } else {
        material.brdf_id = -1;
      }
      material.degamma = material_obj.contains("_degamma") ? material_obj["_degamma"].get<std::string>() == "true" : false;
      
      auto amb = material_obj["AmbientReflectance"].get<std::string>();
      auto diff = material_obj["DiffuseReflectance"].get<std::string>();
      auto spec = material_obj["SpecularReflectance"].get<std::string>();
      
      std::stringstream ss_amb(amb);
      ss_amb >> material.ambient.x >> material.ambient.y >> material.ambient.z;
      std::stringstream ss_diff(diff);
      ss_diff >> material.diffuse.x >> material.diffuse.y >> material.diffuse.z;
      std::stringstream ss_spec(spec);
      ss_spec >> material.specular.x >> material.specular.y >> material.specular.z;

      if(material.degamma){
        material.ambient.x = std::pow(material.ambient.x, 2.2f);
        material.ambient.y = std::pow(material.ambient.y, 2.2f);
        material.ambient.z = std::pow(material.ambient.z, 2.2f);
        material.diffuse.x = std::pow(material.diffuse.x, 2.2f);
        material.diffuse.y = std::pow(material.diffuse.y, 2.2f);
        material.diffuse.z = std::pow(material.diffuse.z, 2.2f);
        material.specular.x = std::pow(material.specular.x, 2.2f);
        material.specular.y = std::pow(material.specular.y, 2.2f);
        material.specular.z = std::pow(material.specular.z, 2.2f);
      }

      if (material_obj.contains("MirrorReflectance")) {
        auto mirror = material_obj["MirrorReflectance"].get<std::string>();
        std::stringstream ss_mirror(mirror);
        ss_mirror >> material.mirror.x >> material.mirror.y >> material.mirror.z;
      }
      if (material_obj.contains("AbsorptionCoefficient")) {
        auto ac = material_obj["AbsorptionCoefficient"].get<std::string>();
        std::stringstream ss_ac(ac);
        ss_ac >> material.absorption_coefficient.x >> material.absorption_coefficient.y >> material.absorption_coefficient.z;
      }
      else{
        material.absorption_coefficient = {-1.0f, -1.0f, -1.0f};
      }
      if (material_obj.contains("RefractionIndex")) {
        material.refraction_index = std::stof(material_obj["RefractionIndex"].get<std::string>());
      }
      else{
        material.refraction_index = -1.0f;
      }
      if (material_obj.contains("AbsorptionIndex")) {
        material.absorption_index = std::stof(material_obj["AbsorptionIndex"].get<std::string>());
      }
      else{
        material.absorption_index = -1.0f;
      }
      material.phong_exponent = material_obj.contains("PhongExponent") ? std::stof(material_obj["PhongExponent"].get<std::string>()) : 0.0f;
      material.roughness = material_obj.contains("Roughness") ? std::stof(material_obj["Roughness"].get<std::string>()) : 0.0f;
      
      materials.push_back(material);
    }
  }

  // Parse Textures
  if (json_data.contains("Textures")) {
    auto tex = json_data["Textures"];
    if (tex.contains("Images")) {
      std::vector<nlohmann::json> json_images;
      try {
        auto img_path = tex["Images"]["Image"]["_data"].get<std::string>();
        json_images.push_back(tex["Images"]["Image"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& img : tex["Images"]["Image"]) {
          json_images.push_back(img);
        }
      }
      for (const auto& img : json_images) {
        RawImage image;
        image.path = img["_data"].get<std::string>();
        images.push_back(image);
      }
    }
    if (tex.contains("TextureMap")) {
      std::vector<nlohmann::json> json_texture_maps;
      try {
        auto tm_id = tex["TextureMap"]["_type"].get<std::string>();
        json_texture_maps.push_back(tex["TextureMap"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& tm : tex["TextureMap"]) {
          json_texture_maps.push_back(tm);
        }
      }
      for (const auto& tm : json_texture_maps) {
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
          texture_map.image_id = std::stoi(tm["ImageId"].get<std::string>());
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
        if (tm.contains("NoiseConversion")) {
          std::string noise_conversion = tm["NoiseConversion"].get<std::string>();
          if (noise_conversion == "absval") {
            texture_map.noise_conversion = RawTextureMapNoiseConversionType::kAbsVal;
          } else if (noise_conversion == "linear") {
            texture_map.noise_conversion = RawTextureMapNoiseConversionType::kLinear;
          }
        }
        
        texture_map.normalizer = tm.contains("Normalizer") ? std::stof(tm["Normalizer"].get<std::string>()) : 255.0f;
        texture_map.bump_factor = tm.contains("BumpFactor") ? std::stof(tm["BumpFactor"].get<std::string>()) : 1.0f;
        texture_map.noise_scale = tm.contains("NoiseScale") ? std::stof(tm["NoiseScale"].get<std::string>()) : 1.0f;
        texture_map.num_octaves = tm.contains("NumOctaves") ? std::stoi(tm["NumOctaves"].get<std::string>()) : 1;
        if (tm.contains("Scale")) {
          texture_map.scale = std::stof(tm["Scale"].get<std::string>());
        } else {
          texture_map.scale = 1.0f;
        }
        if (tm.contains("Offset")) {
          texture_map.offset = std::stof(tm["Offset"].get<std::string>());
        } else {
          texture_map.offset = 0.0f;
        }
        if (tm.contains("BlackColor")) {
          auto black = tm["BlackColor"].get<std::string>();
          std::stringstream ss_black(black);
          ss_black >> texture_map.black_color.x >> texture_map.black_color.y >> texture_map.black_color.z;
        } else {
          texture_map.black_color = {0.0f, 0.0f, 0.0f};
        }
        if (tm.contains("WhiteColor")) {
          auto white = tm["WhiteColor"].get<std::string>();
          std::stringstream ss_white(white);
          ss_white >> texture_map.white_color.x >> texture_map.white_color.y >> texture_map.white_color.z;
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
    // Type check
    std::vector<nlohmann::json> json_translations;
    std::vector<nlohmann::json> json_scalings;
    std::vector<nlohmann::json> json_rotations;
    std::vector<nlohmann::json> json_composites;
    if (transforms.contains("Translation")){

      try {
        auto light_id = transforms["Translation"]["_id"].get<std::string>();
        json_translations.push_back(transforms["Translation"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& cam : transforms["Translation"]) {
          json_translations.push_back(cam);
        }
      }
    }
    if (transforms.contains("Scaling")){

      try {
        auto light_id = transforms["Scaling"]["_id"].get<std::string>();
        json_scalings.push_back(transforms["Scaling"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& cam : transforms["Scaling"]) {
          json_scalings.push_back(cam);
        }
      }
    }
    if (transforms.contains("Rotation")){
      try {
      auto light_id = transforms["Rotation"]["_id"].get<std::string>();
      json_rotations.push_back(transforms["Rotation"]);
    } catch (nlohmann::json::type_error& e) {
      for (const auto& cam : transforms["Rotation"]) {
        json_rotations.push_back(cam);
      }
    }
  }
  if (transforms.contains("Composite")){
    try {
      auto light_id = transforms["Composite"]["_id"].get<std::string>();
      json_composites.push_back(transforms["Composite"]);
    } catch (nlohmann::json::type_error& e) {
      for (const auto& cam : transforms["Composite"]) {
        json_composites.push_back(cam);
      }
    }
  }
    for (const auto& t : json_translations) {
      RawTranslation translation;
      auto data = t["_data"].get<std::string>();
      std::stringstream ss_data(data);
      ss_data >> translation.tx >> translation.ty >> translation.tz;
      translations.push_back(translation);
    }
    for (const auto& t : json_scalings) {
      RawScaling scaling;
      auto data = t["_data"].get<std::string>();
      std::stringstream ss_data(data);
      ss_data >> scaling.sx >> scaling.sy >> scaling.sz;
      scalings.push_back(scaling);
    }
    for (const auto& t : json_rotations) {
      RawRotation rotation;
      auto data = t["_data"].get<std::string>();
      std::stringstream ss_data(data);
      ss_data >> rotation.angle >> rotation.x >> rotation.y >> rotation.z;
      rotations.push_back(rotation);
    }
    for (const auto& t : json_composites) {
      RawComposite composite;
      auto composite_str = t["_data"].get<std::string>();
      std::stringstream ss(composite_str);
      for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
          ss >> composite.m[i][j];
        }
      }
      composites.push_back(composite);
    }
  }
  // Parse VertexData
  if (json_data.contains("VertexData")) {
    auto vertex_datas = json_data["VertexData"]["_data"].get<std::string>();
    std::stringstream ss(vertex_datas);
    while (ss.peek() != EOF) {
      Vec3f vertex;
      ss >> vertex.x >> vertex.y >> vertex.z;
      vertex_data.push_back(vertex);
    }
  }
  // Parse TexCoordData
  if (json_data.contains("TexCoordData")) {
    auto tex_coord_datas = json_data["TexCoordData"]["_data"].get<std::string>();
    std::stringstream ss(tex_coord_datas);
    while (ss.peek() != EOF) {
      Vec2f tex_coord;
      ss >> tex_coord.x >> tex_coord.y;
      tex_coord_data.push_back(tex_coord);
    }
  }
  // Parse Meshes
  if (json_data.contains("Objects")) {
    std::vector<nlohmann::json> json_meshes(0);
    std::vector<nlohmann::json> json_mesh_instances(0);
    std::vector<nlohmann::json> json_triangles(0);
    std::vector<nlohmann::json> json_spheres(0);
    std::vector<nlohmann::json> json_planes(0);
    std::vector<nlohmann::json> json_light_meshes(0);
    std::vector<nlohmann::json> json_light_spheres(0);
    if(json_data["Objects"].contains("Mesh")){
      try {
        auto light_id = json_data["Objects"]["Mesh"]["_id"].get<std::string>();
        json_meshes.push_back(json_data["Objects"]["Mesh"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& cam : json_data["Objects"]["Mesh"]) {
          json_meshes.push_back(cam);
        }
      }
    }
    if(json_data["Objects"].contains("MeshInstance")){
      try {
        auto light_id = json_data["Objects"]["MeshInstance"]["_id"].get<std::string>();
        json_mesh_instances.push_back(json_data["Objects"]["MeshInstance"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& cam : json_data["Objects"]["MeshInstance"]) {
          json_mesh_instances.push_back(cam);
        }
      }
    }
    if(json_data["Objects"].contains("Triangle")){
      try {
        auto light_id = json_data["Objects"]["Triangle"]["_id"].get<std::string>();
        json_triangles.push_back(json_data["Objects"]["Triangle"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& cam : json_data["Objects"]["Triangle"]) {
          json_triangles.push_back(cam);
        }
      }
    }
    if(json_data["Objects"].contains("Sphere")){
      try {
        auto light_id = json_data["Objects"]["Sphere"]["_id"].get<std::string>();
        json_spheres.push_back(json_data["Objects"]["Sphere"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& cam : json_data["Objects"]["Sphere"]) {
          json_spheres.push_back(cam);
        }
      }
    }
    if(json_data["Objects"].contains("Plane")){
      try {
        auto light_id = json_data["Objects"]["Plane"]["_id"].get<std::string>();
        json_planes.push_back(json_data["Objects"]["Plane"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& cam : json_data["Objects"]["Plane"]) {
          json_planes.push_back(cam);
        }
      }
    }
    if(json_data["Objects"].contains("LightMesh")){
      try {
        auto light_id = json_data["Objects"]["LightMesh"]["_id"].get<std::string>();
        json_light_meshes.push_back(json_data["Objects"]["LightMesh"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& cam : json_data["Objects"]["LightMesh"]) {
          json_light_meshes.push_back(cam);
        }
      }
    }
    if(json_data["Objects"].contains("LightSphere")){
      try {
        auto light_id = json_data["Objects"]["LightSphere"]["_id"].get<std::string>();
        json_light_spheres.push_back(json_data["Objects"]["LightSphere"]);
      } catch (nlohmann::json::type_error& e) {
        for (const auto& cam : json_data["Objects"]["LightSphere"]) {
          json_light_spheres.push_back(cam);
        }
      }
    }
    for (const auto& obj : json_meshes) {
      RawMesh mesh;
      mesh.object_id = std::stoi(obj["_id"].get<std::string>());
      mesh.material_id = std::stoi(obj["Material"].get<std::string>());
      mesh.transformations = obj.contains("Transformations") ? obj["Transformations"].get<std::string>() : "";
      mesh.textures = obj.contains("Textures") ? obj["Textures"].get<std::string>() : "";
      if (obj.contains("Faces")) {
        if (obj["Faces"].contains("_plyFile")) {
          mesh.ply_filepath = obj["Faces"]["_plyFile"].get<std::string>();
        } else {
          auto face_data = obj["Faces"]["_data"].get<std::string>();
          std::stringstream ss(face_data);
          while (ss.peek() != EOF) {
            RawFace face;
            ss >> face.v0_id >> face.v1_id >> face.v2_id;
            mesh.faces.push_back(face);
          }
        }
        if(obj["Faces"].contains("_vertexOffset")) {
          mesh.vertex_offset = std::stoll(obj["Faces"]["_vertexOffset"].get<std::string>());
        } else {
          mesh.vertex_offset = 0;
        }
        if(obj["Faces"].contains("_textureOffset")) {
          mesh.tex_coord_offset = std::stoll(obj["Faces"]["_textureOffset"].get<std::string>());
        } else {
          mesh.tex_coord_offset = 0;
        }
      }
      if (obj.contains("MotionBlur")) {
        auto motion_blur = obj["MotionBlur"].get<std::string>();
        std::stringstream ss(motion_blur);
        ss >> mesh.motion_blur.x >> mesh.motion_blur.y >> mesh.motion_blur.z;
      }
      meshes.push_back(mesh);
    }
    for (const auto& obj : json_light_meshes) {
      RawLightMesh mesh;
      mesh.object_id = std::stoi(obj["_id"].get<std::string>());
      mesh.material_id = std::stoi(obj["Material"].get<std::string>());
      mesh.transformations = obj.contains("Transformations") ? obj["Transformations"].get<std::string>() : "";
      mesh.textures = obj.contains("Textures") ? obj["Textures"].get<std::string>() : "";
      if (obj.contains("Faces")) {
        if (obj["Faces"].contains("_plyFile")) {
          mesh.ply_filepath = obj["Faces"]["_plyFile"].get<std::string>();
        } else {
          auto face_data = obj["Faces"]["_data"].get<std::string>();
          std::stringstream ss(face_data);
          while (ss.peek() != EOF) {
            RawFace face;
            ss >> face.v0_id >> face.v1_id >> face.v2_id;
            mesh.faces.push_back(face);
          }
        }
        if(obj["Faces"].contains("_vertexOffset")) {
          mesh.vertex_offset = std::stoll(obj["Faces"]["_vertexOffset"].get<std::string>());
        } else {
          mesh.vertex_offset = 0;
        }
        if(obj["Faces"].contains("_textureOffset")) {
          mesh.tex_coord_offset = std::stoll(obj["Faces"]["_textureOffset"].get<std::string>());
        } else {
          mesh.tex_coord_offset = 0;
        }
      }
      if (obj.contains("MotionBlur")) {
        auto motion_blur = obj["MotionBlur"].get<std::string>();
        std::stringstream ss(motion_blur);
        ss >> mesh.motion_blur.x >> mesh.motion_blur.y >> mesh.motion_blur.z;
      }
      if (obj.contains("Radiance")) {
        auto radiance = obj["Radiance"].get<std::string>();
        std::stringstream ss(radiance);
        ss >> mesh.radiance.x >> mesh.radiance.y >> mesh.radiance.z;
      }
      light_meshes.push_back(mesh);
    }
    
    for (const auto& mi : json_mesh_instances) {
      RawMeshInstance mesh_instance;
      mesh_instance.object_id = std::stoi(mi["_id"].get<std::string>());
      mesh_instance.base_object_id = std::stoi(mi["_baseMeshId"].get<std::string>());
      mesh_instance.reset_transform = mi.contains("_resetTransform") ? (mi["_resetTransform"].get<std::string>() == "true") : false;
      mesh_instance.material_id = mi.contains("Material") ? std::stoi(mi["Material"].get<std::string>()) : -1;
      mesh_instance.transformations = mi.contains("Transformations") ? mi["Transformations"].get<std::string>() : "";
      mesh_instance.textures = mi.contains("Textures") ? mi["Textures"].get<std::string>() : "";
      if (mi.contains("MotionBlur")) {
        auto motion_blur = mi["MotionBlur"].get<std::string>();
        std::stringstream ss(motion_blur);
        ss >> mesh_instance.motion_blur.x >> mesh_instance.motion_blur.y >> mesh_instance.motion_blur.z;
      }
      mesh_instances.push_back(mesh_instance);
    }
    
    for (const auto& t : json_triangles) {
      RawTriangle triangle;
      triangle.object_id = std::stoi(t["_id"].get<std::string>());
      triangle.material_id = std::stoi(t["Material"].get<std::string>());
      triangle.transformations = t.contains("Transformations") ? t["Transformations"].get<std::string>() : "";
      triangle.textures = t.contains("Textures") ? t["Textures"].get<std::string>() : "";
      auto indices = t["Indices"].get<std::string>();
      std::stringstream ss_indices(indices);
      ss_indices >> triangle.indices.v0_id >> triangle.indices.v1_id >> triangle.indices.v2_id;
      if (t.contains("MotionBlur")) {
        auto motion_blur = t["MotionBlur"].get<std::string>();
        std::stringstream ss(motion_blur);
        ss >> triangle.motion_blur.x >> triangle.motion_blur.y >> triangle.motion_blur.z;
      }
      triangles.push_back(triangle);
    }
    
    for (const auto& s : json_spheres) {
      RawSphere sphere;
      sphere.object_id = std::stoi(s["_id"].get<std::string>());
      sphere.material_id = std::stoi(s["Material"].get<std::string>());
      sphere.transformations = s.contains("Transformations") ? s["Transformations"].get<std::string>() : "";
      sphere.textures = s.contains("Textures") ? s["Textures"].get<std::string>() : "";
      sphere.center_vertex_id = std::stoi(s["Center"].get<std::string>());
      sphere.radius = std::stof(s["Radius"].get<std::string>());
      if (s.contains("MotionBlur")) {
        auto motion_blur = s["MotionBlur"].get<std::string>();
        std::stringstream ss(motion_blur);
        ss >> sphere.motion_blur.x >> sphere.motion_blur.y >> sphere.motion_blur.z;
      }
      spheres.push_back(sphere);
    }

    for (const auto& s : json_light_spheres) {
      RawLightSphere sphere;
      sphere.object_id = std::stoi(s["_id"].get<std::string>());
      sphere.material_id = std::stoi(s["Material"].get<std::string>());
      sphere.transformations = s.contains("Transformations") ? s["Transformations"].get<std::string>() : "";
      sphere.textures = s.contains("Textures") ? s["Textures"].get<std::string>() : "";
      sphere.center_vertex_id = std::stoi(s["Center"].get<std::string>());
      sphere.radius = std::stof(s["Radius"].get<std::string>());
      if (s.contains("MotionBlur")) {
        auto motion_blur = s["MotionBlur"].get<std::string>();
        std::stringstream ss(motion_blur);
        ss >> sphere.motion_blur.x >> sphere.motion_blur.y >> sphere.motion_blur.z;
      }
      if (s.contains("Radiance")) {
        auto radiance = s["Radiance"].get<std::string>();
        std::stringstream ss(radiance);
        ss >> sphere.radiance.x >> sphere.radiance.y >> sphere.radiance.z;
      }
      light_spheres.push_back(sphere);
    }

    for(const auto& p : json_planes) {
      RawPlane plane;
      plane.object_id = std::stoi(p["_id"].get<std::string>());
      plane.material_id = std::stoi(p["Material"].get<std::string>());
      plane.transformations = p.contains("Transformations") ? p["Transformations"].get<std::string>() : "";
      plane.textures = p.contains("Textures") ? p["Textures"].get<std::string>() : "";
      plane.point_vertex_id = std::stoi(p["Point"].get<std::string>());
      auto norm = p["Normal"].get<std::string>();
      std::stringstream ss_norm(norm);
      ss_norm >> plane.normal.x >> plane.normal.y >> plane.normal.z;
      if (p.contains("MotionBlur")) {
        auto motion_blur = p["MotionBlur"].get<std::string>();
        std::stringstream ss(motion_blur);
        ss >> plane.motion_blur.x >> plane.motion_blur.y >> plane.motion_blur.z;
      }
      planes.push_back(plane);
    }
  }
  file.close();
}