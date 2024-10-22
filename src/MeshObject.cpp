#include "MeshObject.hpp"

#include <iostream>

MeshObject::MeshObject(std::shared_ptr<BaseMaterial> material,
                       const std::vector<RawFace>& raw_face_data,
                       const std::vector<Vec3f>& raw_vertex_data)
    : BaseObject(material) {
  std::unordered_map<int, int> face_id_map;
  for (const auto& raw_face : raw_face_data) {
    int v0_id = raw_face.v0_id;
    int v1_id = raw_face.v1_id;
    int v2_id = raw_face.v2_id;

    if (face_id_map.find(v0_id) == face_id_map.end()) {
      face_id_map[v0_id] = vertex_data_.size();
      vertex_data_.push_back(raw_vertex_data[v0_id - 1]);
    }
    if (face_id_map.find(v1_id) == face_id_map.end()) {
      face_id_map[v1_id] = vertex_data_.size();
      vertex_data_.push_back(raw_vertex_data[v1_id - 1]);
    }
    if (face_id_map.find(v2_id) == face_id_map.end()) {
      face_id_map[v2_id] = vertex_data_.size();
      vertex_data_.push_back(raw_vertex_data[v2_id - 1]);
    }
    face_data_.push_back(
        {face_id_map[v0_id], face_id_map[v1_id], face_id_map[v2_id]});
  }
};

MeshObject::MeshObject(std::shared_ptr<BaseMaterial> material,
             const std::string& ply_filename) : BaseObject(material){
// vertex_data_;
// face_data_;

             }

bool MeshObject::Intersect(const Ray& ray, float& t_hit,
                           Vec3f& intersection_normal, bool backface_culling,
                           bool stop_at_any_hit) const {
  bool hit = false;
  for (size_t i = 0; i < face_data_.size(); i++) {
    const Vec3f& v0 = vertex_data_[face_data_[i].x];
    const Vec3f& v1 = vertex_data_[face_data_[i].y];
    const Vec3f& v2 = vertex_data_[face_data_[i].z];
    const Vec3f normal = normals_[i];

    if (backface_culling && dot(ray.direction_, normal) > 0) {
      continue;
    }

    Vec3f edge1 = v1 - v0;
    Vec3f edge2 = v2 - v0;
    Vec3f ray_cross_e2 = cross(ray.direction_, edge2);
    float det = dot(edge1, ray_cross_e2);

    float inv_det = 1.0 / det;
    Vec3f s = ray.origin_ - v0;
    float u = inv_det * dot(s, ray_cross_e2);

    if (u < 0 || u > 1) {
      continue;
    }

    Vec3f s_cross_e1 = cross(s, edge1);
    float v = inv_det * dot(ray.direction_, s_cross_e1);

    if (v < 0 || u + v > 1) {
      continue;
    }

    float t = inv_det * dot(edge2, s_cross_e1);

    if (t > 1e-5) {
      if (t < t_hit) {
        t_hit = t;
        intersection_normal = normal;
      }
      hit = true;
      if (stop_at_any_hit) {
        break;
      }
    }
  }
  return hit;
}

void MeshObject::Preprocess() {
  for (const auto& face : face_data_) {
    Vec3f v0 = vertex_data_[face.x];
    Vec3f v1 = vertex_data_[face.y];
    Vec3f v2 = vertex_data_[face.z];
    Vec3f normal = normalize(cross(v1 - v0, v2 - v0));
    normals_.push_back(normal);
  }
}