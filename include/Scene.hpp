#pragma once
#include <functional>
#include <iostream>
#include <memory>
#include <algorithm>
#include <string>
#include <vector>
#include <cctype>

#include "../extern/parser.h"
#include "Spectrum.hpp"
#include "SpectrumLibrary.hpp"
#include "AmbientLightSource.hpp"
#include "AreaLightSource.hpp"
#include "BaseCamera.hpp"
#include "BaseImage.hpp"
#include "BaseMaterial.hpp"
#include "BaseObject.hpp"
#include "BaseTextureMap.hpp"
#include "ConductorMaterial.hpp"
#include "DielectricMaterial.hpp"
#include "MeshInstanceObject.hpp"
#include "MeshObject.hpp"
#include "MirrorMaterial.hpp"
#include "PointLightSource.hpp"
#include "DirectionalLightSource.hpp"
#include "SpotLightSource.hpp"
#include "SphericalDirectionalLightSource.hpp"
#include "STBExporter.hpp"
#include "SphereObject.hpp"
#include "TriangleObject.hpp"
#include "PlaneObject.hpp"
#include "CheckerboardTextureMap.hpp"
#include "PerlinTextureMap.hpp"
#include "ImageTextureMap.hpp"
#include "PhotographicToneMapping.hpp"
#include "FilmicToneMapping.hpp"
#include "ACESToneMapping.hpp"
#include "BaseBRDF.hpp"
#include "OriginalBlinnPhong.hpp"
#include "OriginalPhong.hpp"
#include "ModifiedBlinnPhong.hpp"
#include "ModifiedPhong.hpp"
#include "TorranceSparrow.hpp"
#include "LightMeshObject.hpp"
#include "LightSphereObject.hpp"
#include "ArrayBVH.hpp"

using namespace parser;

struct PathTracerSettings{
    int max_recursion_depth;
    int min_recursion_depth;
    int splitting_factor;
    bool importance_sampling_enabled;
    bool nee_enabled;
    bool mis_balance_enabled;
    bool russian_roulette_enabled;
    FP_PRECISION sample_max_val = 0.0f;
};

// What the previous path vertex needs to tell the next one.
//
// Both extra fields exist to answer one question at an emitter: "may this path
// claim the emitted radiance, and at what weight?" With next-event estimation
// active, direct lighting is already accounted for by the explicit light sample
// at the previous vertex, so a BSDF ray landing on the same light must not add
// it again -- unless the previous bounce was specular (no light sampling is
// possible through a delta reflection) or MIS is on, in which case both
// strategies contribute under balance-heuristic weights.
struct PathState {
    Spectrum throughput = Spectrum::Constant(1.0);
    // Solid-angle pdf with which the previous vertex generated this ray.
    FP_PRECISION prev_bsdf_pdf = 0.0;
    // Camera rays behave like specular bounces: nothing sampled a light to reach
    // the first hit, so emission there is always taken at full weight.
    bool prev_specular = true;
};

// The renderer's decomposition of what a pixel saw:
//
//     radiance(lambda) = reflectance(lambda) * illumination(lambda)
//
// `reflectance` is the diffuse reflectance of the first non-specular surface the
// camera path reached, textures included. `illumination` is the radiance that
// same pixel would have shown had that surface been perfectly white -- so it is
// the light arriving there, with the surface's own colour divided out.
//
// Defining illumination by re-evaluating the BRDF with kd=1 rather than by
// dividing radiance by the albedo matters twice over. It stays defined on a
// black surface, which still receives light and whose illuminant is still worth
// knowing; and it makes the factorisation above a real claim that a test can
// falsify, instead of an identity that holds by construction.
//
// The factorisation is exact only where the first vertex has no specular lobe.
// No per-band scalar splits a specular highlight into "surface colour" times
// "light colour", so with ks > 0 this is an approximation -- a good one for the
// diffuse-dominated scenes it exists to serve, but an approximation.
struct PathAOV {
  Spectrum reflectance = Spectrum::Zero();
  Spectrum illumination = Spectrum::Zero();
  // Set once, at the first non-specular vertex. Specular vertices pass the
  // struct further down instead of filling it, so a path through a mirror
  // records the surface behind the mirror rather than the mirror itself.
  bool captured = false;
};

class Scene {
 public:
  // `serial` swaps the tile-threaded scheduler for the single-threaded one,
  // which is how a suspected race in the trace path gets ruled in or out.
  // `collect_aovs` makes every camera also produce the reflectance and
  // illumination maps. It roughly triples film memory, so it is a knob rather
  // than a constant -- see BaseCamera::EnableAOVs.
  Scene(const std::string &filename, bool serial = false, bool collect_aovs = true);
  void Render();
  ~Scene();

 private:
  void LoadScene();
  void PreprocessScene();

  // The single entry point for tracing a ray against the scene. Covers both the
  // BVH and the plane list, so callers cannot accidentally consult only one of
  // them -- which is exactly how planes ended up visible to primary rays but
  // invisible to shadow and secondary rays.
  // Returns the hit object, or nullptr. With stop_at_any_hit the caller must
  // seed t_hit with the maximum distance of interest (shadow ray usage).
  std::shared_ptr<BaseObject> IntersectScene(
      const Ray &ray, FP_PRECISION &t_hit, Vec3f &hit_normal,
      Vec2f &tex_coords, Vec2f &hit_u_vector, Vec2f &hit_v_vector,
      Vec3f &tangent_vector, Vec3f &bitangent_vector,
      bool stop_at_any_hit = false) const;

  const std::string filename_;

  Spectrum background_color_;
  std::shared_ptr<BaseTextureMap> background_texture_map_ = nullptr;
  FP_PRECISION shadow_ray_epsilon_;

  std::vector<std::shared_ptr<BaseCamera>> cameras_;
  std::vector<std::shared_ptr<PointLightSource>> point_lights_;
  std::vector<std::shared_ptr<AreaLightSource>> area_lights_;
  std::shared_ptr<AmbientLightSource> ambient_light_;
  std::vector<std::shared_ptr<DirectionalLightSource>> directional_lights_;
  std::vector<std::shared_ptr<SpotLightSource>> spot_lights_;
  std::shared_ptr<SphericalDirectionalLightSource> spherical_directional_light_;
  std::vector<std::shared_ptr<BaseBRDF>> brdfs_;
  std::vector<std::shared_ptr<BaseMaterial>> materials_;
  std::vector<std::shared_ptr<BaseObject>> objects_;
  std::vector<std::shared_ptr<BaseObject>> light_objects_;
  std::vector<std::shared_ptr<PlaneObject>> plane_objects_;

  std::vector<std::shared_ptr<BaseImage>> images_;
  std::vector<std::shared_ptr<BaseTextureMap>> texture_maps_;

  ArrayBVH bvh_;

  std::function<void(const std::shared_ptr<BaseCamera>, int)>
      scheduling_algorithm_;
  std::function<Spectrum(Ray &, const std::shared_ptr<BaseObject>, int, int)>
      ray_tracing_algorithm_;
  // The trailing PathAOV* is nullable: nullptr means "not collecting", which is
  // what --no-aov passes, so the extra work costs nothing when it is not wanted.
  std::function<Spectrum(Ray &, const std::shared_ptr<BaseObject>, int, const PathTracerSettings&, const PathState&, PathAOV*)>
      path_tracing_algorithm_;
  std::function<void(Vec3f *, int, int, std::vector<unsigned char> &)>
      tone_mapping_algorithm_;

  std::function<std::vector<Vec2f>(int)> area_light_sampling_algorithm_;

  Spectrum RecursiveBRDFRayTracingAlgorithm(
      Ray &ray,
      const std::shared_ptr<BaseObject> inside_object_ptr,
      int remaining_recursion, int max_recursion);
    Spectrum RecursiveBRDFPathTracingAlgorithm(
      Ray &ray,
      const std::shared_ptr<BaseObject> inside_object_ptr,
      int current_recursion, const PathTracerSettings& settings, const PathState& state = PathState{},
      PathAOV* aov = nullptr);

  // Combined solid-angle pdf of the next-event-estimation strategy for a
  // specific point on a specific emitter: the emitter's own pdf, scaled by the
  // 1/N probability of having picked that emitter out of light_objects_. Both
  // the NEE estimator and the MIS weights must use this same value, including
  // the 1/N factor -- applying it in only one of the two places is a silent
  // weighting error.
  FP_PRECISION LightSamplingPdf(const std::shared_ptr<BaseObject> &light_object,
                                const Vec3f &reference_point,
                                const Vec3f &light_point,
                                const Vec3f &light_normal) const;

  void NonThreadSchedulingAlgorithm(const std::shared_ptr<BaseCamera> camera,
                                    int camera_index);

  void ThreadQueueSchedulingAlgorithm(const std::shared_ptr<BaseCamera> camera,
                                      int camera_index);

};