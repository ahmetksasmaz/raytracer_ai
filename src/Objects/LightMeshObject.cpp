#include "LightMeshObject.hpp"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

LightMeshObject::LightMeshObject(std::shared_ptr<BaseMaterial> material, std::vector<std::shared_ptr<BaseTextureMap>> textures,
                       const std::vector<RawFace>& raw_face_data,
                       const std::vector<Vec3f>& raw_vertex_data,
                       const std::vector<Vec2f>& raw_tex_coord_data,
                        const long long vertex_offset, const long long tex_coord_offset,
                       const Vec3f motion_blur, const Mat4x4f& transform_matrix,
                       RawScalingFlip scaling_flip, Vec3f radiance)
    : BaseObject(material, textures, motion_blur, transform_matrix, scaling_flip), ObjectLightSource(radiance) {
  for (const auto& raw_face : raw_face_data) {
    triangle_objects_.push_back(
            std::make_shared<TriangleObject>(
                material, textures, raw_vertex_data[raw_face.v0_id - 1 + vertex_offset],
                raw_vertex_data[raw_face.v1_id - 1 + vertex_offset],
                raw_vertex_data[raw_face.v2_id - 1 + vertex_offset],
                textures_.size() > 0 ? raw_tex_coord_data[raw_face.v0_id - 1 + tex_coord_offset] : Vec2f{0,0},
                textures_.size() > 0 ? raw_tex_coord_data[raw_face.v1_id - 1 + tex_coord_offset] : Vec2f{0,0},
                textures_.size() > 0 ? raw_tex_coord_data[raw_face.v2_id - 1 + tex_coord_offset] : Vec2f{0,0},
                Vec3f{0, 0, 0},
                IDENTITY_MATRIX, RawScalingFlip{false, false, false}));
  }
};

LightMeshObject::LightMeshObject(std::shared_ptr<BaseMaterial> material, std::vector<std::shared_ptr<BaseTextureMap>> textures,
                       const std::string& ply_filename, const long long vertex_offset, const long long tex_coord_offset, const Vec3f motion_blur,
                       const Mat4x4f& transform_matrix,
                       RawScalingFlip scaling_flip, Vec3f radiance)
    : BaseObject(material, textures, motion_blur, transform_matrix, scaling_flip), ObjectLightSource(radiance) {
  int nelems;
  char** elem_names;
  int file_type;
  float version;

  PlyFile* ply_file = ply_open_for_reading(ply_filename.c_str(), &nelems,
                                           &elem_names, &file_type, &version);

  if (ply_file == NULL) {
    throw std::runtime_error("Error reading file " + ply_filename);
  }



  std::vector<Vec3f> vertex_data_(0);
  std::vector<Vec2f> tex_coord_data_(0);

  bool uvn_read = false;

  for (int i = 0; i < nelems; i++) {
    PlyElement* elem = ply_file->elems[i];
    if (strcmp(elem->name, "vertex") == 0) {
      if(elem->nprops == 3)
      {
        ply_get_property(ply_file, elem->name, &vert_props[0]);
        ply_get_property(ply_file, elem->name, &vert_props[1]);
        ply_get_property(ply_file, elem->name, &vert_props[2]);
        for (size_t j = 0; j < elem->num; j++) {
          Vertex vertex;
          ply_get_element(ply_file, (void*)&vertex);

          vertex_data_.push_back({vertex.x, vertex.y, vertex.z});
        }
      }
      else if(elem->nprops == 5)
      {
        ply_get_property(ply_file, elem->name, &vert_props_uv[0]);
        ply_get_property(ply_file, elem->name, &vert_props_uv[1]);
        ply_get_property(ply_file, elem->name, &vert_props_uv[2]);
        ply_get_property(ply_file, elem->name, &vert_props_uv[3]);
        ply_get_property(ply_file, elem->name, &vert_props_uv[4]);
        for (size_t j = 0; j < elem->num; j++) {
          VertexWithUV vertex;
          ply_get_element(ply_file, (void*)&vertex);

          vertex_data_.push_back({vertex.x, vertex.y, vertex.z});
          tex_coord_data_.push_back({vertex.u, vertex.v});
        }
      }
      else if(elem->nprops == 8)
      {
        uvn_read = true;
        ply_get_property(ply_file, elem->name, &vert_props_uvn[0]);
        ply_get_property(ply_file, elem->name, &vert_props_uvn[1]);
        ply_get_property(ply_file, elem->name, &vert_props_uvn[2]);
        ply_get_property(ply_file, elem->name, &vert_props_uvn[3]);
        ply_get_property(ply_file, elem->name, &vert_props_uvn[4]);
        ply_get_property(ply_file, elem->name, &vert_props_uvn[5]);
        ply_get_property(ply_file, elem->name, &vert_props_uvn[6]);
        ply_get_property(ply_file, elem->name, &vert_props_uvn[7]);
        for (size_t j = 0; j < elem->num; j++) {
          VertexWithUVN vertex;
          ply_get_element(ply_file, (void*)&vertex);

          vertex_data_.push_back({vertex.x, vertex.y, vertex.z});
          tex_coord_data_.push_back({vertex.u, vertex.v});
        }
      }
    } else if (strcmp(elem->name, "face") == 0) {
      if(elem->props[0]->name && strcmp(elem->props[0]->name, "vertex_index") == 0)
      {
        ply_get_property(ply_file, elem->name, &face_props2[0]);
      }
      else{
        ply_get_property(ply_file, elem->name, &face_props[0]);
      }

      for (size_t j = 0; j < elem->num; j++) {
        Face face;
        ply_get_element(ply_file, (void*)&face);

        if (face.nverts == 3) {
          triangle_objects_.push_back(
                  std::make_shared<TriangleObject>(
                      material, textures, vertex_data_[face.verts[0] + vertex_offset],
                      vertex_data_[face.verts[1] + vertex_offset], vertex_data_[face.verts[2] + vertex_offset],
                      textures_.size() > 0 ? tex_coord_data_[face.verts[0] + tex_coord_offset] : Vec2f{0,0},
                      textures_.size() > 0 ? tex_coord_data_[face.verts[1] + tex_coord_offset] : Vec2f{0,0},
                      textures_.size() > 0 ? tex_coord_data_[face.verts[2] + tex_coord_offset] : Vec2f{0,0},
                      Vec3f{0, 0, 0}, IDENTITY_MATRIX,
                      RawScalingFlip{false, false, false}));
        } else if (face.nverts == 4) {
          if(uvn_read){
            triangle_objects_.push_back(
                  std::make_shared<TriangleObject>(
                      material, textures, vertex_data_[face.verts[0] + vertex_offset],
                      vertex_data_[face.verts[1] + vertex_offset], vertex_data_[face.verts[2] + vertex_offset],
                      textures_.size() > 0 ? tex_coord_data_[face.verts[0] + tex_coord_offset] : Vec2f{0,0},
                      textures_.size() > 0 ? tex_coord_data_[face.verts[1] + tex_coord_offset] : Vec2f{0,0},
                      textures_.size() > 0 ? tex_coord_data_[face.verts[2] + tex_coord_offset] : Vec2f{0,0},
                      Vec3f{0, 0, 0}, IDENTITY_MATRIX,
                      RawScalingFlip{false, false, false}));
          }
          else{
            triangle_objects_.push_back(
                std::make_shared<TriangleObject>(
                  material, textures, vertex_data_[face.verts[0] + vertex_offset],
                  vertex_data_[face.verts[1] + vertex_offset], vertex_data_[face.verts[2] + vertex_offset],
                  textures_.size() > 0 ? tex_coord_data_[face.verts[0] + tex_coord_offset] : Vec2f{0,0},
                  textures_.size() > 0 ? tex_coord_data_[face.verts[1] + tex_coord_offset] : Vec2f{0,0},
                  textures_.size() > 0 ? tex_coord_data_[face.verts[2] + tex_coord_offset] : Vec2f{0,0},
                  Vec3f{0, 0, 0}, IDENTITY_MATRIX,
                  RawScalingFlip{false, false, false}));
                  triangle_objects_.push_back(
                      std::make_shared<TriangleObject>(
                        material, textures, vertex_data_[face.verts[0] + vertex_offset],
                        vertex_data_[face.verts[2] + vertex_offset], vertex_data_[face.verts[3] + vertex_offset],
                        textures_.size() > 0 ? tex_coord_data_[face.verts[0] + tex_coord_offset] : Vec2f{0,0},
                        textures_.size() > 0 ? tex_coord_data_[face.verts[2] + tex_coord_offset] : Vec2f{0,0},
                        textures_.size() > 0 ? tex_coord_data_[face.verts[3] + tex_coord_offset] : Vec2f{0,0},
                        Vec3f{0, 0, 0}, IDENTITY_MATRIX,
                        RawScalingFlip{false, false, false}));
          }
        }
      }
    }
  }

  ply_close(ply_file);
}

bool LightMeshObject::Intersect(
    const Ray& ray, FP_PRECISION& t_hit, Vec3f& intersection_normal, Vec2f& tex_coords, Vec2f& hit_u_vector, Vec2f& hit_v_vector, Vec3f& tangent_vector, Vec3f& bitangent_vector, bool backface_culling,
    bool stop_at_any_hit) const {
  bool hit = false;

  Vec3f temp_intersection_normal;

  Vec3f transformed_ray_origin =
      inverse_transform_matrix_ * (ray.origin_ - motion_blur_ * ray.time_);
  Vec3f transformed_ray_direction =
      normalize(inverse_transform_matrix_ ^ ray.direction_);
  Ray transformed_ray{ray.pixel_, transformed_ray_origin,
                      transformed_ray_direction, ray.diff_, ray.time_};

  FP_PRECISION mesh_hit = std::numeric_limits<FP_PRECISION>::max();
  
  int hit_index = triangle_bvh_.Intersect(transformed_ray, triangle_objects_,
                                        mesh_hit, temp_intersection_normal, tex_coords,
                                        hit_u_vector, hit_v_vector, tangent_vector, bitangent_vector,
                                        backface_culling, stop_at_any_hit);
  if (hit_index >= 0) {
    hit = true;
  }
  
  if (hit) {
    Vec3f local_point =
        transformed_ray.origin_ + mesh_hit * transformed_ray.direction_;
    Vec3f global_point = transform_matrix_ * local_point + motion_blur_ * ray.time_;
    Vec3f diff = global_point - ray.origin_;
    t_hit = norm(diff);

    intersection_normal = normalize(transform_matrix_ ^ temp_intersection_normal);
  }

  return hit;
}

void LightMeshObject::Preprocess(bool) {
  FP_PRECISION x_min = std::numeric_limits<FP_PRECISION>::max();
  FP_PRECISION y_min = std::numeric_limits<FP_PRECISION>::max();
  FP_PRECISION z_min = std::numeric_limits<FP_PRECISION>::max();
  FP_PRECISION x_max = std::numeric_limits<FP_PRECISION>::min();
  FP_PRECISION y_max = std::numeric_limits<FP_PRECISION>::min();
  FP_PRECISION z_max = std::numeric_limits<FP_PRECISION>::min();

  for (const auto& triangle_object : triangle_objects_) {
    triangle_object->Preprocess(false);

    x_min = std::min(x_min, triangle_object->min_point_.x);
    y_min = std::min(y_min, triangle_object->min_point_.y);
    z_min = std::min(z_min, triangle_object->min_point_.z);
    x_max = std::max(x_max, triangle_object->max_point_.x);
    y_max = std::max(y_max, triangle_object->max_point_.y);
    z_max = std::max(z_max, triangle_object->max_point_.z);
  }

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

    Vec3f p0_motion = p0 + motion_blur_;
    Vec3f p1_motion = p1 + motion_blur_;
    Vec3f p2_motion = p2 + motion_blur_;
    Vec3f p3_motion = p3 + motion_blur_;
    Vec3f p4_motion = p4 + motion_blur_;
    Vec3f p5_motion = p5 + motion_blur_;
    Vec3f p6_motion = p6 + motion_blur_;
    Vec3f p7_motion = p7 + motion_blur_;

    Vec3f min_point = bounding_volume_min(
        {p0, p1, p2, p3, p4, p5, p6, p7, p0_motion, p1_motion, p2_motion,
         p3_motion, p4_motion, p5_motion, p6_motion, p7_motion});
    Vec3f max_point = bounding_volume_max(
        {p0, p1, p2, p3, p4, p5, p6, p7, p0_motion, p1_motion, p2_motion,
         p3_motion, p4_motion, p5_motion, p6_motion, p7_motion});

    InitializeSelf(min_point, max_point);

    triangle_bvh_.BuildBVH(triangle_objects_);

  std::vector<FP_PRECISION> triangle_areas;
  total_area_ = 0.0;
  for(auto& triangle_object : triangle_objects_)
  {
    Vec3f v0 = transform_matrix_ * triangle_object->v0_ + motion_blur_;
    Vec3f v1 = transform_matrix_ * triangle_object->v1_ + motion_blur_;
    Vec3f v2 = transform_matrix_ * triangle_object->v2_ + motion_blur_;
    FP_PRECISION area = norm(cross(v1 - v0, v2 - v0)) * 0.5;
    triangle_areas.push_back(area);
    total_area_ += area;
  }

  FP_PRECISION cumulative_area = 0.0;
  for (const auto& area : triangle_areas) {
    cumulative_area += area;
    cdf_pdf_.emplace_back(cumulative_area / total_area_, area / total_area_);
  }
}

void LightMeshObject::Sample(const Vec3f& intersection_point, Vec3f &sample_point, Vec3f& sample_normal, FP_PRECISION &pdf) const {
  FP_PRECISION random_value = FastRandom();

  size_t triangle_index = 0;
  size_t left_index = 0;
  size_t right_index = cdf_pdf_.size() - 1;
  while (left_index <= right_index) {
    size_t mid_index = left_index + (right_index - left_index) / 2;
    if (random_value <= cdf_pdf_[mid_index].first) {
      triangle_index = mid_index;
      if (mid_index == 0) break;
      right_index = mid_index - 1;
    } else {
      left_index = mid_index + 1;
    }
  }

  const auto& triangle_object = triangle_objects_[triangle_index];

  FP_PRECISION r1 = FastRandom();
  FP_PRECISION r2 = FastRandom();

  FP_PRECISION sqrt_r1 = sqrt(r1);
  FP_PRECISION u = 1 - sqrt_r1;
  FP_PRECISION v = r2 * sqrt_r1;
  FP_PRECISION w = 1 - u - v;

  Vec3f v0 = triangle_object->v0_;
  Vec3f v1 = triangle_object->v1_;
  Vec3f v2 = triangle_object->v2_;
  Vec3f transformed_v0 = transform_matrix_ * v0 + motion_blur_;
  Vec3f transformed_v1 = transform_matrix_ * v1 + motion_blur_;
  Vec3f transformed_v2 = transform_matrix_ * v2 + motion_blur_;

  sample_point = transform_matrix_ * (u * v0 + v * v1 + w * v2) + motion_blur_;
  sample_normal = normalize(transform_matrix_ ^ triangle_object->normal_);
  // Delegate rather than recompute: PdfSolidAngle is what MIS will call for
  // directions that arrive here some other way, and the two must agree exactly.
  pdf = PdfSolidAngle(intersection_point, sample_point, sample_normal);
}
FP_PRECISION LightMeshObject::PdfSolidAngle(const Vec3f& reference_point,
                                            const Vec3f& light_point,
                                            const Vec3f& light_normal) const {
  // Triangles are chosen with probability proportional to their area, so the
  // per-triangle factor cancels and the area-measure density is uniformly
  // 1 / total_area. Converting area measure to solid angle contributes the
  // usual dist^2 / cos(theta_light) Jacobian.
  if (total_area_ <= 1e-12) return 0.0;

  const Vec3f to_reference = reference_point - light_point;
  const FP_PRECISION dist2 = norm2(to_reference);
  if (dist2 <= 1e-12) return 0.0;

  const FP_PRECISION cos_at_light = dot(light_normal, normalize(to_reference));
  // Emitters are one-sided: the back face cannot produce this direction at all,
  // so the pdf is genuinely zero rather than merely small.
  if (cos_at_light <= 1e-6) return 0.0;

  const FP_PRECISION pdf = dist2 / (total_area_ * cos_at_light);
  return std::isfinite(pdf) && pdf > 0.0 ? pdf : 0.0;
}
