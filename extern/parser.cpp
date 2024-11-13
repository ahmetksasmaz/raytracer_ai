#include "parser.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "tinyxml2.h"

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
    auto child = element->FirstChildElement("Position");
    stream << child->GetText() << std::endl;
    child = element->FirstChildElement("Gaze");
    stream << child->GetText() << std::endl;
    child = element->FirstChildElement("Up");
    stream << child->GetText() << std::endl;
    child = element->FirstChildElement("NearPlane");
    stream << child->GetText() << std::endl;
    child = element->FirstChildElement("NearDistance");
    stream << child->GetText() << std::endl;
    child = element->FirstChildElement("ImageResolution");
    stream << child->GetText() << std::endl;
    child = element->FirstChildElement("ImageName");
    stream << child->GetText() << std::endl;

    stream >> camera.position.x >> camera.position.y >> camera.position.z;
    stream >> camera.gaze.x >> camera.gaze.y >> camera.gaze.z;
    stream >> camera.up.x >> camera.up.y >> camera.up.z;
    stream >> camera.near_plane.x >> camera.near_plane.y >>
        camera.near_plane.z >> camera.near_plane.w;
    stream >> camera.near_distance;
    stream >> camera.image_width >> camera.image_height;
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
      material.phong_exponent = -1.0f;
    }

    child = element->FirstChildElement("Roughness");
    if (child) {
      stream << child->GetText() << std::endl;
      stream >> material.roughness;
    }

    materials.push_back(material);
    element = element->NextSiblingElement("Material");
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
