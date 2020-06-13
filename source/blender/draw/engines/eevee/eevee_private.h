/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup DNA
 */

#ifndef __EEVEE_PRIVATE_H__
#define __EEVEE_PRIVATE_H__

#include "DRW_render.h"

#include "BLI_bitmap.h"

#include "DNA_lightprobe_types.h"

struct EEVEE_ShadowCasterBuffer;
struct GPUFrameBuffer;
struct Object;
struct RenderLayer;

extern struct DrawEngineType draw_engine_eevee_type;

/* Minimum UBO is 16384 bytes */
#define MAX_PROBE 128 /* TODO : find size by dividing UBO max size by probe data size */
#define MAX_GRID 64   /* TODO : find size by dividing UBO max size by grid data size */
#define MAX_PLANAR 16 /* TODO : find size by dividing UBO max size by grid data size */
#define MAX_LIGHT 128 /* TODO : find size by dividing UBO max size by light data size */
#define MAX_CASCADE_NUM 4
#define MAX_SHADOW 128 /* TODO : Make this depends on GL_MAX_ARRAY_TEXTURE_LAYERS */
#define MAX_SHADOW_CASCADE 8
#define MAX_SHADOW_CUBE (MAX_SHADOW - MAX_CASCADE_NUM * MAX_SHADOW_CASCADE)
#define MAX_BLOOM_STEP 16

// #define DEBUG_SHADOW_DISTRIBUTION

/* Only define one of these. */
// #define IRRADIANCE_SH_L2
// #define IRRADIANCE_CUBEMAP
#define IRRADIANCE_HL2
#define HAMMERSLEY_SIZE 1024

#if defined(IRRADIANCE_SH_L2)
#  define SHADER_IRRADIANCE "#define IRRADIANCE_SH_L2\n"
#elif defined(IRRADIANCE_CUBEMAP)
#  define SHADER_IRRADIANCE "#define IRRADIANCE_CUBEMAP\n"
#elif defined(IRRADIANCE_HL2)
#  define SHADER_IRRADIANCE "#define IRRADIANCE_HL2\n"
#endif

/* Macro causes over indentation. */
/* clang-format off */
#define SHADER_DEFINES \
  "#define EEVEE_ENGINE\n" \
  "#define MAX_PROBE " STRINGIFY(MAX_PROBE) "\n" \
  "#define MAX_GRID " STRINGIFY(MAX_GRID) "\n" \
  "#define MAX_PLANAR " STRINGIFY(MAX_PLANAR) "\n" \
  "#define MAX_LIGHT " STRINGIFY(MAX_LIGHT) "\n" \
  "#define MAX_SHADOW " STRINGIFY(MAX_SHADOW) "\n" \
  "#define MAX_SHADOW_CUBE " STRINGIFY(MAX_SHADOW_CUBE) "\n" \
  "#define MAX_SHADOW_CASCADE " STRINGIFY(MAX_SHADOW_CASCADE) "\n" \
  "#define MAX_CASCADE_NUM " STRINGIFY(MAX_CASCADE_NUM) "\n" \
  SHADER_IRRADIANCE
/* clang-format on */

#define EEVEE_PROBE_MAX min_ii(MAX_PROBE, GPU_max_texture_layers() / 6)

#define SWAP_DOUBLE_BUFFERS() \
  { \
    if (effects->swap_double_buffer) { \
      SWAP(struct GPUFrameBuffer *, fbl->main_fb, fbl->double_buffer_fb); \
      SWAP(struct GPUFrameBuffer *, fbl->main_color_fb, fbl->double_buffer_color_fb); \
      SWAP(GPUTexture *, txl->color, txl->color_double_buffer); \
      effects->swap_double_buffer = false; \
    } \
  } \
  ((void)0)

#define SWAP_BUFFERS() \
  { \
    if (effects->target_buffer == fbl->effect_color_fb) { \
      SWAP_DOUBLE_BUFFERS(); \
      effects->source_buffer = txl->color_post; \
      effects->target_buffer = fbl->main_color_fb; \
    } \
    else { \
      SWAP_DOUBLE_BUFFERS(); \
      effects->source_buffer = txl->color; \
      effects->target_buffer = fbl->effect_color_fb; \
    } \
  } \
  ((void)0)

#define SWAP_BUFFERS_TAA() \
  { \
    if (effects->target_buffer == fbl->effect_color_fb) { \
      SWAP(struct GPUFrameBuffer *, fbl->effect_fb, fbl->taa_history_fb); \
      SWAP(struct GPUFrameBuffer *, fbl->effect_color_fb, fbl->taa_history_color_fb); \
      SWAP(GPUTexture *, txl->color_post, txl->taa_history); \
      effects->source_buffer = txl->taa_history; \
      effects->target_buffer = fbl->effect_color_fb; \
    } \
    else { \
      SWAP(struct GPUFrameBuffer *, fbl->main_fb, fbl->taa_history_fb); \
      SWAP(struct GPUFrameBuffer *, fbl->main_color_fb, fbl->taa_history_color_fb); \
      SWAP(GPUTexture *, txl->color, txl->taa_history); \
      effects->source_buffer = txl->taa_history; \
      effects->target_buffer = fbl->main_color_fb; \
    } \
  } \
  ((void)0)

BLI_INLINE bool eevee_hdri_preview_overlay_enabled(const View3D *v3d)
{
  /* Only show the HDRI Preview in Shading Preview in the Viewport. */
  if (v3d == NULL || v3d->shading.type != OB_MATERIAL) {
    return false;
  }

  /* Only show the HDRI Preview when viewing the Combined render pass */
  if (v3d->shading.render_pass != SCE_PASS_COMBINED) {
    return false;
  }

  return ((v3d->flag2 & V3D_HIDE_OVERLAYS) == 0) && (v3d->overlay.flag & V3D_OVERLAY_LOOK_DEV);
}

#define USE_SCENE_LIGHT(v3d) \
  ((!v3d) || \
   ((v3d->shading.type == OB_MATERIAL) && (v3d->shading.flag & V3D_SHADING_SCENE_LIGHTS)) || \
   ((v3d->shading.type == OB_RENDER) && (v3d->shading.flag & V3D_SHADING_SCENE_LIGHTS_RENDER)))
#define LOOK_DEV_STUDIO_LIGHT_ENABLED(v3d) \
  ((v3d) && (((v3d->shading.type == OB_MATERIAL) && \
              ((v3d->shading.flag & V3D_SHADING_SCENE_WORLD) == 0)) || \
             ((v3d->shading.type == OB_RENDER) && \
              ((v3d->shading.flag & V3D_SHADING_SCENE_WORLD_RENDER) == 0))))

#define MIN_CUBE_LOD_LEVEL 3
#define MAX_PLANAR_LOD_LEVEL 9

/* All the renderpasses that use the GPUMaterial for accumulation */
#define EEVEE_RENDERPASSES_MATERIAL \
  (EEVEE_RENDER_PASS_EMIT | EEVEE_RENDER_PASS_DIFFUSE_COLOR | EEVEE_RENDER_PASS_DIFFUSE_LIGHT | \
   EEVEE_RENDER_PASS_SPECULAR_COLOR | EEVEE_RENDER_PASS_SPECULAR_LIGHT | \
   EEVEE_RENDER_PASS_ENVIRONMENT)

/* Material shader variations */
enum {
  VAR_MAT_MESH = (1 << 0),
  VAR_MAT_VOLUME = (1 << 1),
  VAR_MAT_HAIR = (1 << 2),
  VAR_MAT_PROBE = (1 << 3),
  VAR_MAT_BLEND = (1 << 4),
  VAR_MAT_LOOKDEV = (1 << 5),
  VAR_MAT_HOLDOUT = (1 << 6),
  VAR_MAT_HASH = (1 << 7),
  VAR_MAT_DEPTH = (1 << 8),
  VAR_MAT_REFRACT = (1 << 9),
  VAR_WORLD_BACKGROUND = (1 << 10),
  VAR_WORLD_PROBE = (1 << 11),
  VAR_WORLD_VOLUME = (1 << 12),
  VAR_DEFAULT = (1 << 13),
};

/* Material shader cache keys */
enum {
  /* HACK: This assumes the struct GPUShader will never be smaller than our variations.
   * This allow us to only keep one ghash and avoid bigger keys comparisons/hashing.
   * We combine the GPUShader pointer with the key. */
  KEY_CULL = (1 << 0),
  KEY_REFRACT = (1 << 1),
  KEY_HAIR = (1 << 2),
  KEY_SHADOW = (1 << 3),
};

/* ************ PROBE UBO ************* */

/* They are the same struct as their Cache siblings.
 * typedef'ing just to keep the naming consistent with
 * other eevee types. */
typedef LightProbeCache EEVEE_LightProbe;
typedef LightGridCache EEVEE_LightGrid;

typedef struct EEVEE_PlanarReflection {
  float plane_equation[4];
  float clip_vec_x[3], attenuation_scale;
  float clip_vec_y[3], attenuation_bias;
  float clip_edge_x_pos, clip_edge_x_neg;
  float clip_edge_y_pos, clip_edge_y_neg;
  float facing_scale, facing_bias, clipsta, pad;
  float reflectionmat[4][4]; /* Used for sampling the texture. */
  float mtx[4][4];           /* Not used in shader. TODO move elsewhere. */
} EEVEE_PlanarReflection;

/* --------------------------------------- */

typedef struct EEVEE_BoundBox {
  float center[3], halfdim[3];
} EEVEE_BoundBox;

typedef struct EEVEE_PassList {
  /* Shadows */
  struct DRWPass *shadow_pass;
  struct DRWPass *shadow_accum_pass;

  /* Probes */
  struct DRWPass *probe_background;
  struct DRWPass *probe_glossy_compute;
  struct DRWPass *probe_diffuse_compute;
  struct DRWPass *probe_visibility_compute;
  struct DRWPass *probe_grid_fill;
  struct DRWPass *probe_display;
  struct DRWPass *probe_planar_downsample_ps;

  /* Effects */
  struct DRWPass *ao_horizon_search;
  struct DRWPass *ao_horizon_search_layer;
  struct DRWPass *ao_horizon_debug;
  struct DRWPass *ao_accum_ps;
  struct DRWPass *mist_accum_ps;
  struct DRWPass *motion_blur;
  struct DRWPass *bloom_blit;
  struct DRWPass *bloom_downsample_first;
  struct DRWPass *bloom_downsample;
  struct DRWPass *bloom_upsample;
  struct DRWPass *bloom_resolve;
  struct DRWPass *bloom_accum_ps;
  struct DRWPass *dof_down;
  struct DRWPass *dof_scatter;
  struct DRWPass *dof_resolve;
  struct DRWPass *volumetric_world_ps;
  struct DRWPass *volumetric_objects_ps;
  struct DRWPass *volumetric_scatter_ps;
  struct DRWPass *volumetric_integration_ps;
  struct DRWPass *volumetric_resolve_ps;
  struct DRWPass *volumetric_accum_ps;
  struct DRWPass *ssr_raytrace;
  struct DRWPass *ssr_resolve;
  struct DRWPass *sss_blur_ps;
  struct DRWPass *sss_resolve_ps;
  struct DRWPass *sss_translucency_ps;
  struct DRWPass *color_downsample_ps;
  struct DRWPass *color_downsample_cube_ps;
  struct DRWPass *velocity_resolve;
  struct DRWPass *taa_resolve;
  struct DRWPass *alpha_checker;

  /* HiZ */
  struct DRWPass *minz_downlevel_ps;
  struct DRWPass *maxz_downlevel_ps;
  struct DRWPass *minz_downdepth_ps;
  struct DRWPass *maxz_downdepth_ps;
  struct DRWPass *minz_downdepth_layer_ps;
  struct DRWPass *maxz_downdepth_layer_ps;
  struct DRWPass *minz_copydepth_ps;
  struct DRWPass *maxz_copydepth_ps;
  struct DRWPass *maxz_copydepth_layer_ps;

  /* Renderpass Accumulation. */
  struct DRWPass *material_accum_ps;
  struct DRWPass *background_accum_ps;

  struct DRWPass *depth_ps;
  struct DRWPass *depth_cull_ps;
  struct DRWPass *depth_clip_ps;
  struct DRWPass *depth_clip_cull_ps;
  struct DRWPass *depth_refract_ps;
  struct DRWPass *depth_refract_cull_ps;
  struct DRWPass *depth_refract_clip_ps;
  struct DRWPass *depth_refract_clip_cull_ps;
  struct DRWPass *material_ps;
  struct DRWPass *material_cull_ps;
  struct DRWPass *material_refract_ps;
  struct DRWPass *material_refract_cull_ps;
  struct DRWPass *material_sss_ps;
  struct DRWPass *material_sss_cull_ps;
  struct DRWPass *transparent_pass;
  struct DRWPass *background_ps;
  struct DRWPass *update_noise_pass;
  struct DRWPass *lookdev_glossy_pass;
  struct DRWPass *lookdev_diffuse_pass;
  struct DRWPass *renderpass_pass;
} EEVEE_PassList;

typedef struct EEVEE_FramebufferList {
  /* Effects */
  struct GPUFrameBuffer *gtao_fb;
  struct GPUFrameBuffer *gtao_debug_fb;
  struct GPUFrameBuffer *downsample_fb;
  struct GPUFrameBuffer *bloom_blit_fb;
  struct GPUFrameBuffer *bloom_down_fb[MAX_BLOOM_STEP];
  struct GPUFrameBuffer *bloom_accum_fb[MAX_BLOOM_STEP - 1];
  struct GPUFrameBuffer *bloom_pass_accum_fb;
  struct GPUFrameBuffer *shadow_accum_fb;
  struct GPUFrameBuffer *ssr_accum_fb;
  struct GPUFrameBuffer *sss_blur_fb;
  struct GPUFrameBuffer *sss_blit_fb;
  struct GPUFrameBuffer *sss_resolve_fb;
  struct GPUFrameBuffer *sss_clear_fb;
  struct GPUFrameBuffer *sss_translucency_fb;
  struct GPUFrameBuffer *sss_accum_fb;
  struct GPUFrameBuffer *dof_down_fb;
  struct GPUFrameBuffer *dof_scatter_fb;
  struct GPUFrameBuffer *volumetric_fb;
  struct GPUFrameBuffer *volumetric_scat_fb;
  struct GPUFrameBuffer *volumetric_integ_fb;
  struct GPUFrameBuffer *volumetric_accum_fb;
  struct GPUFrameBuffer *screen_tracing_fb;
  struct GPUFrameBuffer *refract_fb;
  struct GPUFrameBuffer *mist_accum_fb;
  struct GPUFrameBuffer *material_accum_fb;
  struct GPUFrameBuffer *renderpass_fb;
  struct GPUFrameBuffer *ao_accum_fb;
  struct GPUFrameBuffer *velocity_resolve_fb;

  struct GPUFrameBuffer *update_noise_fb;

  struct GPUFrameBuffer *planarref_fb;
  struct GPUFrameBuffer *planar_downsample_fb;

  struct GPUFrameBuffer *main_fb;
  struct GPUFrameBuffer *main_color_fb;
  struct GPUFrameBuffer *effect_fb;
  struct GPUFrameBuffer *effect_color_fb;
  struct GPUFrameBuffer *double_buffer_fb;
  struct GPUFrameBuffer *double_buffer_color_fb;
  struct GPUFrameBuffer *double_buffer_depth_fb;
  struct GPUFrameBuffer *taa_history_fb;
  struct GPUFrameBuffer *taa_history_color_fb;
} EEVEE_FramebufferList;

typedef struct EEVEE_TextureList {
  /* Effects */
  struct GPUTexture *color_post; /* R16_G16_B16 */
  struct GPUTexture *mist_accum;
  struct GPUTexture *ao_accum;
  struct GPUTexture *sss_accum;
  struct GPUTexture *env_accum;
  struct GPUTexture *diff_color_accum;
  struct GPUTexture *diff_light_accum;
  struct GPUTexture *spec_color_accum;
  struct GPUTexture *spec_light_accum;
  struct GPUTexture *emit_accum;
  struct GPUTexture *bloom_accum;
  struct GPUTexture *ssr_accum;
  struct GPUTexture *shadow_accum;
  struct GPUTexture *refract_color;
  struct GPUTexture *taa_history;

  struct GPUTexture *volume_prop_scattering;
  struct GPUTexture *volume_prop_extinction;
  struct GPUTexture *volume_prop_emission;
  struct GPUTexture *volume_prop_phase;
  struct GPUTexture *volume_scatter;
  struct GPUTexture *volume_transmit;
  struct GPUTexture *volume_scatter_history;
  struct GPUTexture *volume_transmit_history;
  struct GPUTexture *volume_scatter_accum;
  struct GPUTexture *volume_transmittance_accum;

  struct GPUTexture *lookdev_grid_tx;
  struct GPUTexture *lookdev_cube_tx;

  struct GPUTexture *planar_pool;
  struct GPUTexture *planar_depth;

  struct GPUTexture *maxzbuffer;

  struct GPUTexture *renderpass;

  struct GPUTexture *color; /* R16_G16_B16 */
  struct GPUTexture *color_double_buffer;
  struct GPUTexture *depth_double_buffer;
} EEVEE_TextureList;

typedef struct EEVEE_StorageList {
  /* Effects */
  struct EEVEE_EffectsInfo *effects;

  struct EEVEE_PrivateData *g_data;

  struct LightCache *lookdev_lightcache;
  EEVEE_LightProbe *lookdev_cube_data;
  EEVEE_LightGrid *lookdev_grid_data;
  LightCacheTexture *lookdev_cube_mips;
} EEVEE_StorageList;

/* ************ RENDERPASS UBO ************* */
typedef struct EEVEE_RenderPassData {
  int renderPassDiffuse;
  int renderPassDiffuseLight;
  int renderPassGlossy;
  int renderPassGlossyLight;
  int renderPassEmit;
  int renderPassSSSColor;
  int _pad[2];
} EEVEE_RenderPassData;

/* ************ LIGHT UBO ************* */
typedef struct EEVEE_Light {
  float position[3], invsqrdist;
  float color[3], spec;
  float spotsize, spotblend, radius, shadow_id;
  float rightvec[3], sizex;
  float upvec[3], sizey;
  float forwardvec[3], light_type;
} EEVEE_Light;

/* Special type for elliptic area lights, matches lamps_lib.glsl */
#define LAMPTYPE_AREA_ELLIPSE 100.0f

typedef struct EEVEE_Shadow {
  float near, far, bias, type_data_id;
  float contact_dist, contact_bias, contact_spread, contact_thickness;
} EEVEE_Shadow;

typedef struct EEVEE_ShadowCube {
  float shadowmat[4][4];
  float position[3], _pad0[1];
} EEVEE_ShadowCube;

typedef struct EEVEE_ShadowCascade {
  /* World->Light->NDC->Tex : used for sampling the shadow map. */
  float shadowmat[MAX_CASCADE_NUM][4][4];
  float split_start[4];
  float split_end[4];
  float shadow_vec[3], tex_id;
} EEVEE_ShadowCascade;

typedef struct EEVEE_ShadowCascadeRender {
  /* World->Light->NDC : used for rendering the shadow map. */
  float projmat[MAX_CASCADE_NUM][4][4];
  float viewmat[4][4], viewinv[4][4];
  float radius[MAX_CASCADE_NUM];
  float original_bias;
  float cascade_max_dist;
  float cascade_exponent;
  float cascade_fade;
  int cascade_count;
} EEVEE_ShadowCascadeRender;

BLI_STATIC_ASSERT_ALIGN(EEVEE_Light, 16)
BLI_STATIC_ASSERT_ALIGN(EEVEE_Shadow, 16)
BLI_STATIC_ASSERT_ALIGN(EEVEE_ShadowCube, 16)
BLI_STATIC_ASSERT_ALIGN(EEVEE_ShadowCascade, 16)
BLI_STATIC_ASSERT_ALIGN(EEVEE_RenderPassData, 16)

BLI_STATIC_ASSERT(sizeof(EEVEE_Shadow) * MAX_SHADOW +
                          sizeof(EEVEE_ShadowCascade) * MAX_SHADOW_CASCADE +
                          sizeof(EEVEE_ShadowCube) * MAX_SHADOW_CUBE <
                      16384,
                  "Shadow UBO is too big!!!")

typedef struct EEVEE_ShadowCasterBuffer {
  struct EEVEE_BoundBox *bbox;
  BLI_bitmap *update;
  uint alloc_count;
  uint count;
} EEVEE_ShadowCasterBuffer;

/* ************ LIGHT DATA ************* */
typedef struct EEVEE_LightsInfo {
  int num_light, cache_num_light;
  int num_cube_layer, cache_num_cube_layer;
  int num_cascade_layer, cache_num_cascade_layer;
  int cube_len, cascade_len, shadow_len;
  int shadow_cube_size, shadow_cascade_size;
  bool shadow_high_bitdepth, soft_shadows;
  /* UBO Storage : data used by UBO */
  struct EEVEE_Light light_data[MAX_LIGHT];
  struct EEVEE_Shadow shadow_data[MAX_SHADOW];
  struct EEVEE_ShadowCube shadow_cube_data[MAX_SHADOW_CUBE];
  struct EEVEE_ShadowCascade shadow_cascade_data[MAX_SHADOW_CASCADE];
  /* Additionnal rendering info for cascade. */
  struct EEVEE_ShadowCascadeRender shadow_cascade_render[MAX_SHADOW_CASCADE];
  /* Back index in light_data. */
  uchar shadow_cube_light_indices[MAX_SHADOW_CUBE];
  uchar shadow_cascade_light_indices[MAX_SHADOW_CASCADE];
  /* Update bitmap. */
  BLI_bitmap sh_cube_update[BLI_BITMAP_SIZE(MAX_SHADOW_CUBE)];
  /* Lights tracking */
  struct BoundSphere shadow_bounds[MAX_LIGHT]; /* Tightly packed light bounds  */
  /* List of bbox and update bitmap. Double buffered. */
  struct EEVEE_ShadowCasterBuffer *shcaster_frontbuffer, *shcaster_backbuffer;
  /* AABB of all shadow casters combined. */
  struct {
    float min[3], max[3];
  } shcaster_aabb;
} EEVEE_LightsInfo;

/* ************ PROBE DATA ************* */
typedef struct EEVEE_LightProbeVisTest {
  struct Collection *collection; /* Skip test if NULL */
  bool invert;
  bool cached; /* Reuse last test results */
} EEVEE_LightProbeVisTest;

typedef struct EEVEE_LightProbesInfo {
  int num_cube, cache_num_cube;
  int num_grid, cache_num_grid;
  int num_planar, cache_num_planar;
  int total_irradiance_samples; /* Total for all grids */
  int cache_irradiance_size[3];
  int update_flag;
  int updated_bounce;
  int num_bounce;
  int cubemap_res;
  /* Update */
  bool do_cube_update;
  bool do_grid_update;
  /* For rendering probes */
  float probemat[6][4][4];
  int layer;
  float texel_size;
  float padding_size;
  float samples_len;
  float samples_len_inv;
  float near_clip;
  float far_clip;
  float roughness;
  float firefly_fac;
  float lodfactor;
  float lod_rt_max, lod_cube_max, lod_planar_max;
  float visibility_range;
  float visibility_blur;
  float intensity_fac;
  int shres;
  EEVEE_LightProbeVisTest planar_vis_tests[MAX_PLANAR];
  /* UBO Storage : data used by UBO */
  EEVEE_LightProbe probe_data[MAX_PROBE];
  EEVEE_LightGrid grid_data[MAX_GRID];
  EEVEE_PlanarReflection planar_data[MAX_PLANAR];
  /* Probe Visibility Collection */
  EEVEE_LightProbeVisTest vis_data;
} EEVEE_LightProbesInfo;

/* EEVEE_LightProbesInfo->update_flag */
enum {
  PROBE_UPDATE_CUBE = (1 << 0),
  PROBE_UPDATE_GRID = (1 << 1),
  PROBE_UPDATE_ALL = 0xFFFFFF,
};

/* ************ EFFECTS DATA ************* */

typedef enum EEVEE_EffectsFlag {
  EFFECT_MOTION_BLUR = (1 << 0),
  EFFECT_BLOOM = (1 << 1),
  EFFECT_DOF = (1 << 2),
  EFFECT_VOLUMETRIC = (1 << 3),
  EFFECT_SSR = (1 << 4),
  EFFECT_DOUBLE_BUFFER = (1 << 5), /* Not really an effect but a feature */
  EFFECT_REFRACT = (1 << 6),
  EFFECT_GTAO = (1 << 7),
  EFFECT_TAA = (1 << 8),
  EFFECT_POST_BUFFER = (1 << 9),    /* Not really an effect but a feature */
  EFFECT_NORMAL_BUFFER = (1 << 10), /* Not really an effect but a feature */
  EFFECT_SSS = (1 << 11),
  EFFECT_VELOCITY_BUFFER = (1 << 12),     /* Not really an effect but a feature */
  EFFECT_TAA_REPROJECT = (1 << 13),       /* should be mutually exclusive with EFFECT_TAA */
  EFFECT_DEPTH_DOUBLE_BUFFER = (1 << 14), /* Not really an effect but a feature */
} EEVEE_EffectsFlag;

typedef struct EEVEE_EffectsInfo {
  EEVEE_EffectsFlag enabled_effects;
  bool swap_double_buffer;
  /* SSSS */
  int sss_sample_count;
  int sss_surface_count;
  struct GPUTexture *sss_irradiance; /* Textures from pool */
  struct GPUTexture *sss_radius;
  struct GPUTexture *sss_albedo;
  struct GPUTexture *sss_blur;
  struct GPUTexture *sss_stencil;
  /* Volumetrics */
  int volume_current_sample;
  struct GPUTexture *volume_scatter;
  struct GPUTexture *volume_transmit;
  /* SSR */
  bool reflection_trace_full;
  bool ssr_was_persp;
  bool ssr_was_valid_double_buffer;
  int ssr_neighbor_ofs;
  int ssr_halfres_ofs[2];
  struct GPUTexture *ssr_normal_input; /* Textures from pool */
  struct GPUTexture *ssr_specrough_input;
  struct GPUTexture *ssr_hit_output;
  struct GPUTexture *ssr_pdf_output;
  /* Temporal Anti Aliasing */
  int taa_reproject_sample;
  int taa_current_sample;
  int taa_render_sample;
  int taa_total_sample;
  float taa_alpha;
  bool prev_drw_support;
  bool prev_is_navigating;
  float prev_drw_persmat[4][4];
  struct DRWView *taa_view;
  /* Ambient Occlusion */
  int ao_depth_layer;
  struct GPUTexture *ao_src_depth;  /* pointer copy */
  struct GPUTexture *gtao_horizons; /* Textures from pool */
  struct GPUTexture *gtao_horizons_debug;
  /* Motion Blur */
  float current_world_to_ndc[4][4];
  float current_ndc_to_world[4][4];
  float past_world_to_ndc[4][4];
  int motion_blur_samples;
  bool motion_blur_mat_cached;
  /* Velocity Pass */
  float velocity_curr_persinv[4][4];
  float velocity_past_persmat[4][4];
  struct GPUTexture *velocity_tx; /* Texture from pool */
  /* Depth Of Field */
  float dof_near_far[2];
  float dof_params[2];
  float dof_bokeh[4];
  float dof_bokeh_sides[4];
  int dof_target_size[2];
  struct GPUTexture *dof_down_near; /* Textures from pool */
  struct GPUTexture *dof_down_far;
  struct GPUTexture *dof_coc;
  struct GPUTexture *dof_blur;
  struct GPUTexture *dof_blur_alpha;
  /* Alpha Checker */
  float color_checker_dark[4];
  float color_checker_light[4];
  /* Other */
  float prev_persmat[4][4];
  /* Lookdev */
  int sphere_size;
  int anchor[2];
  struct DRWView *lookdev_view;
  /* Bloom */
  int bloom_iteration_len;
  float source_texel_size[2];
  float blit_texel_size[2];
  float downsamp_texel_size[MAX_BLOOM_STEP][2];
  float bloom_color[3];
  float bloom_clamp;
  float bloom_sample_scale;
  float bloom_curve_threshold[4];
  float unf_source_texel_size[2];
  struct GPUTexture *bloom_blit; /* Textures from pool */
  struct GPUTexture *bloom_downsample[MAX_BLOOM_STEP];
  struct GPUTexture *bloom_upsample[MAX_BLOOM_STEP - 1];
  struct GPUTexture *unf_source_buffer; /* pointer copy */
  struct GPUTexture *unf_base_buffer;   /* pointer copy */
  /* Not alloced, just a copy of a *GPUtexture in EEVEE_TextureList. */
  struct GPUTexture *source_buffer;     /* latest updated texture */
  struct GPUFrameBuffer *target_buffer; /* next target to render to */
  struct GPUTexture *final_tx;          /* Final color to transform to display color space. */
  struct GPUFrameBuffer *final_fb;      /* Framebuffer with final_tx as attachment. */
} EEVEE_EffectsInfo;

/* ***************** COMMON DATA **************** */

/* Common uniform buffer containing all "constant" data over the whole drawing pipeline. */
/* !! CAUTION !!
 * - [i]vec3 need to be padded to [i]vec4 (even in ubo declaration).
 * - Make sure that [i]vec4 start at a multiple of 16 bytes.
 * - Arrays of vec2/vec3 are padded as arrays of vec4.
 * - sizeof(bool) == sizeof(int) in GLSL so use int in C */
typedef struct EEVEE_CommonUniformBuffer {
  float prev_persmat[4][4]; /* mat4 */
  float view_vecs[2][4];    /* vec4[2] */
  float mip_ratio[10][4];   /* vec2[10] */
  /* Ambient Occlusion */
  /* -- 16 byte aligned -- */
  float ao_dist, pad1, ao_factor, pad2;                    /* vec4 */
  float ao_offset, ao_bounce_fac, ao_quality, ao_settings; /* vec4 */
  /* Volumetric */
  /* -- 16 byte aligned -- */
  int vol_tex_size[3], pad3;       /* ivec3 */
  float vol_depth_param[3], pad4;  /* vec3 */
  float vol_inv_tex_size[3], pad5; /* vec3 */
  float vol_jitter[3], pad6;       /* vec3 */
  float vol_coord_scale[4];        /* vec4 */
  /* -- 16 byte aligned -- */
  float vol_history_alpha; /* float */
  float vol_light_clamp;   /* float */
  float vol_shadow_steps;  /* float */
  int vol_use_lights;      /* bool */
  /* Screen Space Reflections */
  /* -- 16 byte aligned -- */
  float ssr_quality, ssr_thickness, ssr_pixelsize[2]; /* vec4 */
  float ssr_border_fac;                               /* float */
  float ssr_max_roughness;                            /* float */
  float ssr_firefly_fac;                              /* float */
  float ssr_brdf_bias;                                /* float */
  int ssr_toggle;                                     /* bool */
  int ssrefract_toggle;                               /* bool */
  /* SubSurface Scattering */
  float sss_jitter_threshold; /* float */
  int sss_toggle;             /* bool */
  /* Specular */
  int spec_toggle; /* bool */
  /* Lights */
  int la_num_light; /* int */
  /* Probes */
  int prb_num_planar;          /* int */
  int prb_num_render_cube;     /* int */
  int prb_num_render_grid;     /* int */
  int prb_irradiance_vis_size; /* int */
  float prb_irradiance_smooth; /* float */
  float prb_lod_cube_max;      /* float */
  float prb_lod_planar_max;    /* float */
  /* Misc */
  int hiz_mip_offset;      /* int */
  int ray_type;            /* int */
  float ray_depth;         /* float */
  float alpha_hash_offset; /* float */
  float alpha_hash_scale;  /* float */
  float pad7;              /* float */
  float pad8;              /* float */
} EEVEE_CommonUniformBuffer;

BLI_STATIC_ASSERT_ALIGN(EEVEE_CommonUniformBuffer, 16)

/* ray_type (keep in sync with rayType) */
#define EEVEE_RAY_CAMERA 0
#define EEVEE_RAY_SHADOW 1
#define EEVEE_RAY_DIFFUSE 2
#define EEVEE_RAY_GLOSSY 3

/* ************** SCENE LAYER DATA ************** */
typedef struct EEVEE_ViewLayerData {
  /* Lights */
  struct EEVEE_LightsInfo *lights;

  struct GPUUniformBuffer *light_ubo;
  struct GPUUniformBuffer *shadow_ubo;
  struct GPUUniformBuffer *shadow_samples_ubo;

  struct GPUFrameBuffer *shadow_fb;

  struct GPUTexture *shadow_cube_pool;
  struct GPUTexture *shadow_cascade_pool;

  struct EEVEE_ShadowCasterBuffer shcasters_buffers[2];

  /* Probes */
  struct EEVEE_LightProbesInfo *probes;

  struct GPUUniformBuffer *probe_ubo;
  struct GPUUniformBuffer *grid_ubo;
  struct GPUUniformBuffer *planar_ubo;

  /* Material Render passes */
  struct {
    struct GPUUniformBuffer *combined;
    struct GPUUniformBuffer *diff_color;
    struct GPUUniformBuffer *diff_light;
    struct GPUUniformBuffer *spec_color;
    struct GPUUniformBuffer *spec_light;
    struct GPUUniformBuffer *emit;
  } renderpass_ubo;

  /* Common Uniform Buffer */
  struct EEVEE_CommonUniformBuffer common_data;
  struct GPUUniformBuffer *common_ubo;

  struct LightCache *fallback_lightcache;

  struct BLI_memblock *material_cache;
} EEVEE_ViewLayerData;

/* ************ OBJECT DATA ************ */

/* These are the structs stored inside Objects.
 * It works even if the object is in multiple layers
 * because we don't get the same "Object *" for each layer. */
typedef struct EEVEE_LightEngineData {
  DrawData dd;

  bool need_update;
} EEVEE_LightEngineData;

typedef struct EEVEE_LightProbeEngineData {
  DrawData dd;

  bool need_update;
} EEVEE_LightProbeEngineData;

typedef struct EEVEE_ObjectEngineData {
  DrawData dd;

  Object *ob; /* self reference */
  EEVEE_LightProbeVisTest *test_data;
  bool ob_vis, ob_vis_dirty;

  bool need_update;
  uint shadow_caster_id;
} EEVEE_ObjectEngineData;

typedef struct EEVEE_WorldEngineData {
  DrawData dd;
} EEVEE_WorldEngineData;

/* *********************************** */

typedef struct EEVEE_Data {
  void *engine_type;
  EEVEE_FramebufferList *fbl;
  EEVEE_TextureList *txl;
  EEVEE_PassList *psl;
  EEVEE_StorageList *stl;
} EEVEE_Data;

typedef struct EEVEE_PrivateData {
  struct DRWShadingGroup *shadow_shgrp;
  struct DRWShadingGroup *shadow_accum_shgrp;
  struct DRWCallBuffer *planar_display_shgrp;
  struct GHash *material_hash;
  float background_alpha; /* TODO find a better place for this. */
  /* Chosen lightcache: can come from Lookdev or the viewlayer. */
  struct LightCache *light_cache;
  /* For planar probes */
  float planar_texel_size[2];
  /* For double buffering */
  bool view_updated;
  bool valid_double_buffer;
  bool valid_taa_history;
  /* Render Matrices */
  float studiolight_matrix[3][3];
  float overscan, overscan_pixels;
  float size_orig[2];

  /* Mist Settings */
  float mist_start, mist_inv_dist, mist_falloff;

  /* Color Management */
  bool use_color_render_settings;

  /* Compiling shaders count. This is to track if a shader has finished compiling. */
  int queued_shaders_count;
  int queued_shaders_count_prev;

  /* LookDev Settings */
  int studiolight_index;
  float studiolight_rot_z;
  float studiolight_intensity;
  int studiolight_cubemap_res;
  float studiolight_glossy_clamp;
  float studiolight_filter_quality;

  /* Renderpasses */
  /* Bitmask containing the active render_passes */
  eViewLayerEEVEEPassType render_passes;
  /* Uniform references that are referenced inside the `renderpass_pass`. They are updated
   * to reuse the drawing pass and the shading group. */
  int renderpass_type;
  int renderpass_postprocess;
  int renderpass_current_sample;
  GPUTexture *renderpass_input;
  GPUTexture *renderpass_col_input;
  GPUTexture *renderpass_light_input;
  /* Renderpass ubo reference used by material pass. */
  struct GPUUniformBuffer *renderpass_ubo;
  /** For rendering shadows. */
  struct DRWView *cube_views[6];
  /** For rendering probes. */
  struct DRWView *bake_views[6];
  /** Same as bake_views but does not generate culling infos. */
  struct DRWView *world_views[6];
  /** For rendering planar reflections. */
  struct DRWView *planar_views[MAX_PLANAR];
} EEVEE_PrivateData; /* Transient data */

/* eevee_data.c */
void EEVEE_view_layer_data_free(void *sldata);
EEVEE_ViewLayerData *EEVEE_view_layer_data_get(void);
EEVEE_ViewLayerData *EEVEE_view_layer_data_ensure_ex(struct ViewLayer *view_layer);
EEVEE_ViewLayerData *EEVEE_view_layer_data_ensure(void);
EEVEE_ObjectEngineData *EEVEE_object_data_get(Object *ob);
EEVEE_ObjectEngineData *EEVEE_object_data_ensure(Object *ob);
EEVEE_LightProbeEngineData *EEVEE_lightprobe_data_get(Object *ob);
EEVEE_LightProbeEngineData *EEVEE_lightprobe_data_ensure(Object *ob);
EEVEE_LightEngineData *EEVEE_light_data_get(Object *ob);
EEVEE_LightEngineData *EEVEE_light_data_ensure(Object *ob);
EEVEE_WorldEngineData *EEVEE_world_data_get(World *wo);
EEVEE_WorldEngineData *EEVEE_world_data_ensure(World *wo);

/* eevee_materials.c */
struct GPUTexture *EEVEE_materials_get_util_tex(void); /* XXX */
void EEVEE_materials_init(EEVEE_ViewLayerData *sldata,
                          EEVEE_Data *vedata,
                          EEVEE_StorageList *stl,
                          EEVEE_FramebufferList *fbl);
void EEVEE_materials_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_materials_cache_populate(EEVEE_Data *vedata,
                                    EEVEE_ViewLayerData *sldata,
                                    Object *ob,
                                    bool *cast_shadow);
void EEVEE_particle_hair_cache_populate(EEVEE_Data *vedata,
                                        EEVEE_ViewLayerData *sldata,
                                        Object *ob,
                                        bool *cast_shadow);
void EEVEE_object_hair_cache_populate(EEVEE_Data *vedata,
                                      EEVEE_ViewLayerData *sldata,
                                      Object *ob,
                                      bool *cast_shadow);
void EEVEE_materials_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_materials_free(void);
void EEVEE_update_noise(EEVEE_PassList *psl, EEVEE_FramebufferList *fbl, const double offsets[3]);
void EEVEE_update_viewvecs(float invproj[4][4], float winmat[4][4], float (*r_viewvecs)[4]);
void EEVEE_material_renderpasses_init(EEVEE_Data *vedata);
void EEVEE_material_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, uint tot_samples);
void EEVEE_material_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_material_bind_resources(DRWShadingGroup *shgrp,
                                   struct GPUMaterial *gpumat,
                                   EEVEE_ViewLayerData *sldata,
                                   EEVEE_Data *vedata,
                                   int *ssr_id,
                                   float *refract_depth,
                                   bool use_ssrefraction,
                                   bool use_alpha_blend);
/* eevee_lights.c */
void eevee_light_matrix_get(const EEVEE_Light *evli, float r_mat[4][4]);
void EEVEE_lights_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lights_cache_add(EEVEE_ViewLayerData *sldata, struct Object *ob);
void EEVEE_lights_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);

/* eevee_shadows.c */
void eevee_contact_shadow_setup(const Light *la, EEVEE_Shadow *evsh);
void EEVEE_shadows_init(EEVEE_ViewLayerData *sldata);
void EEVEE_shadows_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_shadows_caster_register(EEVEE_ViewLayerData *sldata, struct Object *ob);
void EEVEE_shadows_update(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_shadows_cube_add(EEVEE_LightsInfo *linfo, EEVEE_Light *evli, struct Object *ob);
bool EEVEE_shadows_cube_setup(EEVEE_LightsInfo *linfo, const EEVEE_Light *evli, int sample_ofs);
void EEVEE_shadows_cascade_add(EEVEE_LightsInfo *linfo, EEVEE_Light *evli, struct Object *ob);
void EEVEE_shadows_draw(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, struct DRWView *view);
void EEVEE_shadows_draw_cubemap(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, int cube_index);
void EEVEE_shadows_draw_cascades(EEVEE_ViewLayerData *sldata,
                                 EEVEE_Data *vedata,
                                 DRWView *view,
                                 int cascade_index);
void EEVEE_shadow_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, uint tot_samples);
void EEVEE_shadow_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_shadows_free(void);

/* eevee_sampling.c */
void EEVEE_sample_ball(int sample_ofs, float radius, float rsample[3]);
void EEVEE_sample_rectangle(int sample_ofs,
                            const float x_axis[3],
                            const float y_axis[3],
                            float size_x,
                            float size_y,
                            float rsample[3]);
void EEVEE_sample_ellipse(int sample_ofs,
                          const float x_axis[3],
                          const float y_axis[3],
                          float size_x,
                          float size_y,
                          float rsample[3]);
void EEVEE_random_rotation_m4(int sample_ofs, float scale, float r_mat[4][4]);

/* eevee_shaders.c */
void EEVEE_shaders_lightprobe_shaders_init(void);
void EEVEE_shaders_material_shaders_init(void);
struct GPUShader *EEVEE_shaders_probe_filter_glossy_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_default_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_filter_diffuse_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_filter_visibility_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_grid_fill_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_planar_downsample_sh_get(void);
struct GPUShader *EEVEE_shaders_default_studiolight_sh_get(void);
struct GPUShader *EEVEE_shaders_default_background_sh_get(void);
struct GPUShader *EEVEE_shaders_background_studiolight_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_cube_display_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_grid_display_sh_get(void);
struct GPUShader *EEVEE_shaders_probe_planar_display_sh_get(void);
struct GPUShader *EEVEE_shaders_update_noise_sh_get(void);
struct GPUShader *EEVEE_shaders_velocity_resolve_sh_get(void);
struct GPUShader *EEVEE_shaders_taa_resolve_sh_get(EEVEE_EffectsFlag enabled_effects);
struct bNodeTree *EEVEE_shader_default_surface_nodetree(Material *ma);
struct bNodeTree *EEVEE_shader_default_world_nodetree(World *wo);
Material *EEVEE_material_default_diffuse_get(void);
Material *EEVEE_material_default_glossy_get(void);
Material *EEVEE_material_default_error_get(void);
struct GPUMaterial *EEVEE_material_default_get(struct Scene *scene, Material *ma, int options);
struct GPUMaterial *EEVEE_material_get(
    EEVEE_Data *vedata, struct Scene *scene, Material *ma, World *wo, int options);
void EEVEE_shaders_free(void);

/* eevee_lightprobes.c */
bool EEVEE_lightprobes_obj_visibility_cb(bool vis_in, void *user_data);
void EEVEE_lightprobes_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lightprobes_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lightprobes_cache_add(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, Object *ob);
void EEVEE_lightprobes_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lightprobes_refresh(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lightprobes_refresh_planar(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_lightprobes_free(void);

void EEVEE_lightbake_cache_init(EEVEE_ViewLayerData *sldata,
                                EEVEE_Data *vedata,
                                GPUTexture *rt_color,
                                GPUTexture *rt_depth);
void EEVEE_lightbake_render_world(EEVEE_ViewLayerData *sldata,
                                  EEVEE_Data *vedata,
                                  struct GPUFrameBuffer *face_fb[6]);
void EEVEE_lightbake_render_scene(EEVEE_ViewLayerData *sldata,
                                  EEVEE_Data *vedata,
                                  struct GPUFrameBuffer *face_fb[6],
                                  const float pos[3],
                                  float near_clip,
                                  float far_clip);
void EEVEE_lightbake_filter_glossy(EEVEE_ViewLayerData *sldata,
                                   EEVEE_Data *vedata,
                                   struct GPUTexture *rt_color,
                                   struct GPUFrameBuffer *fb,
                                   int probe_idx,
                                   float intensity,
                                   int maxlevel,
                                   float filter_quality,
                                   float firefly_fac);
void EEVEE_lightbake_filter_diffuse(EEVEE_ViewLayerData *sldata,
                                    EEVEE_Data *vedata,
                                    struct GPUTexture *rt_color,
                                    struct GPUFrameBuffer *fb,
                                    int grid_offset,
                                    float intensity);
void EEVEE_lightbake_filter_visibility(EEVEE_ViewLayerData *sldata,
                                       EEVEE_Data *vedata,
                                       struct GPUTexture *rt_depth,
                                       struct GPUFrameBuffer *fb,
                                       int grid_offset,
                                       float clipsta,
                                       float clipend,
                                       float vis_range,
                                       float vis_blur,
                                       int vis_size);

void EEVEE_lightprobes_grid_data_from_object(Object *ob, EEVEE_LightGrid *prb_data, int *offset);
void EEVEE_lightprobes_cube_data_from_object(Object *ob, EEVEE_LightProbe *prb_data);
void EEVEE_lightprobes_planar_data_from_object(Object *ob,
                                               EEVEE_PlanarReflection *eplanar,
                                               EEVEE_LightProbeVisTest *vis_test);

/* eevee_depth_of_field.c */
int EEVEE_depth_of_field_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, Object *camera);
void EEVEE_depth_of_field_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_depth_of_field_draw(EEVEE_Data *vedata);
void EEVEE_depth_of_field_free(void);

/* eevee_bloom.c */
int EEVEE_bloom_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_bloom_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_bloom_draw(EEVEE_Data *vedata);
void EEVEE_bloom_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, uint tot_samples);
void EEVEE_bloom_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_bloom_free(void);

/* eevee_occlusion.c */
int EEVEE_occlusion_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_occlusion_output_init(EEVEE_ViewLayerData *sldata,
                                 EEVEE_Data *vedata,
                                 uint tot_samples);
void EEVEE_occlusion_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_occlusion_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_occlusion_compute(EEVEE_ViewLayerData *sldata,
                             EEVEE_Data *vedata,
                             struct GPUTexture *depth_src,
                             int layer);
void EEVEE_occlusion_draw_debug(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_occlusion_free(void);

/* eevee_screen_raytrace.c */
int EEVEE_screen_raytrace_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_screen_raytrace_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_refraction_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_reflection_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_reflection_output_init(EEVEE_ViewLayerData *sldata,
                                  EEVEE_Data *vedata,
                                  uint tot_samples);
void EEVEE_reflection_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);

void EEVEE_screen_raytrace_free(void);

/* eevee_subsurface.c */
void EEVEE_subsurface_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_draw_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_output_init(EEVEE_ViewLayerData *sldata,
                                  EEVEE_Data *vedata,
                                  uint tot_samples);
void EEVEE_subsurface_add_pass(EEVEE_ViewLayerData *sldata,
                               EEVEE_Data *vedata,
                               Material *ma,
                               DRWShadingGroup *shgrp,
                               struct GPUMaterial *gpumat);
void EEVEE_subsurface_data_render(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_subsurface_free(void);

/* eevee_motion_blur.c */
int EEVEE_motion_blur_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, Object *camera);
void EEVEE_motion_blur_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_motion_blur_draw(EEVEE_Data *vedata);
void EEVEE_motion_blur_free(void);

/* eevee_mist.c */
void EEVEE_mist_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_mist_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_mist_free(void);

/* eevee_renderpasses.c */
void EEVEE_renderpasses_init(EEVEE_Data *vedata);
void EEVEE_renderpasses_output_init(EEVEE_ViewLayerData *sldata,
                                    EEVEE_Data *vedata,
                                    uint tot_samples);
void EEVEE_renderpasses_output_accumulate(EEVEE_ViewLayerData *sldata,
                                          EEVEE_Data *vedata,
                                          bool post_effect);
void EEVEE_renderpasses_postprocess(EEVEE_ViewLayerData *sldata,
                                    EEVEE_Data *vedata,
                                    eViewLayerEEVEEPassType renderpass_type);
void EEVEE_renderpasses_draw(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_renderpasses_draw_debug(EEVEE_Data *vedata);
void EEVEE_renderpasses_free(void);
bool EEVEE_renderpasses_only_first_sample_pass_active(EEVEE_Data *vedata);

/* eevee_temporal_sampling.c */
void EEVEE_temporal_sampling_reset(EEVEE_Data *vedata);
int EEVEE_temporal_sampling_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_temporal_sampling_offset_calc(const double ht_point[2],
                                         const float filter_size,
                                         float r_offset[2]);
void EEVEE_temporal_sampling_matrices_calc(EEVEE_EffectsInfo *effects, const double ht_point[2]);
void EEVEE_temporal_sampling_update_matrices(EEVEE_Data *vedata);
void EEVEE_temporal_sampling_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_temporal_sampling_draw(EEVEE_Data *vedata);

/* eevee_volumes.c */
void EEVEE_volumes_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_set_jitter(EEVEE_ViewLayerData *sldata, uint current_sample);
void EEVEE_volumes_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_cache_object_add(EEVEE_ViewLayerData *sldata,
                                    EEVEE_Data *vedata,
                                    struct Scene *scene,
                                    Object *ob);
void EEVEE_volumes_cache_finish(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_draw_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_compute(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_resolve(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_output_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata, uint tot_samples);
void EEVEE_volumes_output_accumulate(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_volumes_free_smoke_textures(void);
void EEVEE_volumes_free(void);

/* eevee_effects.c */
void EEVEE_effects_init(EEVEE_ViewLayerData *sldata,
                        EEVEE_Data *vedata,
                        Object *camera,
                        const bool minimal);
void EEVEE_effects_cache_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_effects_draw_init(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_create_minmax_buffer(EEVEE_Data *vedata, struct GPUTexture *depth_src, int layer);
void EEVEE_downsample_buffer(EEVEE_Data *vedata, struct GPUTexture *texture_src, int level);
void EEVEE_downsample_cube_buffer(EEVEE_Data *vedata, struct GPUTexture *texture_src, int level);
void EEVEE_draw_effects(EEVEE_ViewLayerData *sldata, EEVEE_Data *vedata);
void EEVEE_effects_free(void);

/* eevee_render.c */
bool EEVEE_render_init(EEVEE_Data *vedata,
                       struct RenderEngine *engine,
                       struct Depsgraph *depsgraph);
void EEVEE_render_cache(void *vedata,
                        struct Object *ob,
                        struct RenderEngine *engine,
                        struct Depsgraph *depsgraph);
void EEVEE_render_draw(EEVEE_Data *vedata,
                       struct RenderEngine *engine,
                       struct RenderLayer *render_layer,
                       const struct rcti *rect);
void EEVEE_render_update_passes(struct RenderEngine *engine,
                                struct Scene *scene,
                                struct ViewLayer *view_layer);

/** eevee_lookdev.c */
void EEVEE_lookdev_cache_init(EEVEE_Data *vedata,
                              EEVEE_ViewLayerData *sldata,
                              DRWShadingGroup **grp,
                              DRWPass *pass,
                              struct World *world,
                              EEVEE_LightProbesInfo *pinfo);
void EEVEE_lookdev_draw(EEVEE_Data *vedata);

/** eevee_engine.c */
void EEVEE_cache_populate(void *vedata, Object *ob);

/* Shadow Matrix */
static const float texcomat[4][4] = {
    /* From NDC to TexCo */
    {0.5f, 0.0f, 0.0f, 0.0f},
    {0.0f, 0.5f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.5f, 0.0f},
    {0.5f, 0.5f, 0.5f, 1.0f},
};

/* Cubemap Matrices */
static const float cubefacemat[6][4][4] = {
    /* Pos X */
    {{0.0f, 0.0f, -1.0f, 0.0f},
     {0.0f, -1.0f, 0.0f, 0.0f},
     {-1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
    /* Neg X */
    {{0.0f, 0.0f, 1.0f, 0.0f},
     {0.0f, -1.0f, 0.0f, 0.0f},
     {1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
    /* Pos Y */
    {{1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, -1.0f, 0.0f},
     {0.0f, 1.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
    /* Neg Y */
    {{1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 1.0f, 0.0f},
     {0.0f, -1.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
    /* Pos Z */
    {{1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, -1.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, -1.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
    /* Neg Z */
    {{-1.0f, 0.0f, 0.0f, 0.0f},
     {0.0f, -1.0f, 0.0f, 0.0f},
     {0.0f, 0.0f, 1.0f, 0.0f},
     {0.0f, 0.0f, 0.0f, 1.0f}},
};

#endif /* __EEVEE_PRIVATE_H__ */