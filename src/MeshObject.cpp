#include "MeshObject.hpp"

#include <cstring>
#include <iostream>
#include <limits>

typedef struct Vertex {
  float x, y, z; /* the usual 3-space position of a vertex */
} Vertex;

typedef struct Face {
  unsigned char nverts; /* number of vertex indices in list */
  int* verts;           /* vertex index list */
} Face;

PlyProperty vert_props[] = {
    /* list of property information for a vertex */
    {const_cast<char*>("x"), PLY_FLOAT, PLY_FLOAT, offsetof(Vertex, x), 0, 0, 0,
     0},
    {const_cast<char*>("y"), PLY_FLOAT, PLY_FLOAT, offsetof(Vertex, y), 0, 0, 0,
     0},
    {const_cast<char*>("z"), PLY_FLOAT, PLY_FLOAT, offsetof(Vertex, z), 0, 0, 0,
     0},
};

PlyProperty face_props[] = {
    /* list of property information for a vertex */
    {const_cast<char*>("vertex_indices"), PLY_INT, PLY_INT,
     offsetof(Face, verts), 1, PLY_UCHAR, PLY_UCHAR, offsetof(Face, nverts)},
};

MeshObject::MeshObject(std::shared_ptr<BaseMaterial> material,
                       const std::vector<RawFace>& raw_face_data,
                       const std::vector<Vec3f>& raw_vertex_data,
                       const Mat4x4f& transform_matrix)
    : BaseObject(material, transform_matrix) {
  for (const auto& raw_face : raw_face_data) {
    triangle_objects_.push_back(
        std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
            std::make_shared<TriangleObject>(
                material, raw_vertex_data[raw_face.v0_id - 1],
                raw_vertex_data[raw_face.v1_id - 1],
                raw_vertex_data[raw_face.v2_id - 1], transform_matrix)));
  }
};

MeshObject::MeshObject(std::shared_ptr<BaseMaterial> material,
                       const std::string& ply_filename,
                       const Mat4x4f& transform_matrix)
    : BaseObject(material, transform_matrix) {
  int nelems;
  char** elem_names;
  int file_type;
  float version;

  PlyFile* ply_file = ply_open_for_reading(ply_filename.c_str(), &nelems,
                                           &elem_names, &file_type, &version);

  if (ply_file == NULL) {
    std::cout << "Error reading file" << std::endl;
  }

  std::vector<Vec3f> vertex_data_;

  for (int i = 0; i < nelems; i++) {
    PlyElement* elem = ply_file->elems[i];
    if (strcmp(elem->name, "vertex") == 0) {
      ply_get_property(ply_file, elem->name, &vert_props[0]);
      ply_get_property(ply_file, elem->name, &vert_props[1]);
      ply_get_property(ply_file, elem->name, &vert_props[2]);
      for (size_t j = 0; j < elem->num; j++) {
        Vertex vertex;
        ply_get_element(ply_file, (void*)&vertex);

        vertex_data_.push_back({vertex.x, vertex.y, vertex.z});
      }
    } else if (strcmp(elem->name, "face") == 0) {
      ply_get_property(ply_file, elem->name, &face_props[0]);

      for (size_t j = 0; j < elem->num; j++) {
        Face face;
        ply_get_element(ply_file, (void*)&face);

        triangle_objects_.push_back(
            std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
                std::make_shared<TriangleObject>(
                    material, vertex_data_[face.verts[0]],
                    vertex_data_[face.verts[1]], vertex_data_[face.verts[2]],
                    transform_matrix)));
      }
    }
  }

  ply_close(ply_file);
}

std::shared_ptr<BoundingVolumeHierarchyElement> MeshObject::Intersect(
    const Ray& ray, float& t_hit, Vec3f& intersection_normal,
    bool backface_culling, bool stop_at_any_hit) const {
  std::shared_ptr<BoundingVolumeHierarchyElement> hit = nullptr;

  if (left_) {
    hit = left_->Intersect(ray, t_hit, intersection_normal, backface_culling,
                           stop_at_any_hit);
  } else {
    for (size_t i = 0; i < triangle_objects_.size(); i++) {
      float t;
      Vec3f normal;
      if (!triangle_objects_[i]->Intersect(ray, t, normal, backface_culling,
                                           stop_at_any_hit)) {
        continue;
      }

      if (t < t_hit) {
        t_hit = t;
        intersection_normal = normal;
      }
      hit = std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
          std::const_pointer_cast<BaseObject>(this->shared_from_this()));
      if (stop_at_any_hit) {
        break;
      }
    }
  }
  return hit;
}

void MeshObject::Preprocess(bool high_level_bvh_enabled,
                            bool low_level_bvh_enabled, bool) {
  float x_min = std::numeric_limits<float>::max();
  float y_min = std::numeric_limits<float>::max();
  float z_min = std::numeric_limits<float>::max();
  float x_max = std::numeric_limits<float>::min();
  float y_max = std::numeric_limits<float>::min();
  float z_max = std::numeric_limits<float>::min();

  for (const auto& triangle_object : triangle_objects_) {
    std::shared_ptr<TriangleObject> triangle_object_casted =
        std::dynamic_pointer_cast<TriangleObject>(triangle_object);

    triangle_object_casted->Preprocess(high_level_bvh_enabled,
                                       low_level_bvh_enabled, false);

    x_min = std::min(x_min, triangle_object->min_point_.x);
    y_min = std::min(y_min, triangle_object->min_point_.y);
    z_min = std::min(z_min, triangle_object->min_point_.z);
    x_max = std::max(x_max, triangle_object->max_point_.x);
    y_max = std::max(y_max, triangle_object->max_point_.y);
    z_max = std::max(z_max, triangle_object->max_point_.z);
  }

  if (high_level_bvh_enabled || low_level_bvh_enabled) {
    Vec3f p0 = Vec3f{x_min, y_min, z_min};
    Vec3f p1 = Vec3f{x_max, y_min, z_min};
    Vec3f p2 = Vec3f{x_min, y_max, z_min};
    Vec3f p3 = Vec3f{x_max, y_max, z_min};
    Vec3f p4 = Vec3f{x_min, y_min, z_max};
    Vec3f p5 = Vec3f{x_max, y_min, z_max};
    Vec3f p6 = Vec3f{x_min, y_max, z_max};
    Vec3f p7 = Vec3f{x_max, y_max, z_max};

    p0 = transform_matrix_ * p0;
    p1 = transform_matrix_ * p1;
    p2 = transform_matrix_ * p2;
    p3 = transform_matrix_ * p3;
    p4 = transform_matrix_ * p4;
    p5 = transform_matrix_ * p5;
    p6 = transform_matrix_ * p6;
    p7 = transform_matrix_ * p7;

    Vec3f min_point = bounding_volume_min({p0, p1, p2, p3, p4, p5, p6, p7});
    Vec3f max_point = bounding_volume_max({p0, p1, p2, p3, p4, p5, p6, p7});

    InitializeSelf(min_point, max_point);
  }

  if (low_level_bvh_enabled) {
    left_ = BoundingVolumeHierarchyElement::Construct(
        triangle_objects_, 0, triangle_objects_.size(), 0);

    if (!left_) {
      std::cout << "Error constructing BVH" << std::endl;
    }
  }
}