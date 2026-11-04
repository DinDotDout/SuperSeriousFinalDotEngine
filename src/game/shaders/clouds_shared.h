#ifndef CLOUDS_SHARED_H
#define CLOUDS_SHARED_H

#include "game/shaders/sun_shared.h"

ReflectEnum(u32, DOT_CloudDebugView, {
    DOT_CloudDebugView_Final,
    DOT_CloudDebugView_SceneDepth,
    DOT_CloudDebugView_RaySegment,
    DOT_CloudDebugView_Coverage,
    DOT_CloudDebugView_Density,
    DOT_CloudDebugView_Transmittance,
    DOT_CloudDebugView_CloudColor,
});

Reflect(DOT_CloudParams, {
    DOT_CloudDebugView debug_view;

    f32 fast_march_transmittance_thresh;
    u32 min_marching_steps;
    u32 max_marching_steps;
    f32 stop_distance;

    u32 max_steps_lights;
    f32 light_step_short;
    f32 light_step_long;

    f32 scattering_aniso_fw;
    f32 scattering_aniso_bw;
    f32 extinction_coefficient_sky;
    f32 scattering_coefficient_clouds;
    f32 scattering_coefficient_sugar;

    f32 wind_speed;

    f32 map_density;
    f32 clouds_min_height;
    f32 clouds_max_height;
    f32 weathermap_tile_radius;

    f32 noise_weight;
    f32 clouds_noise_scale;
    f32x4 clouds_noise_fbm_weights;

    f32 detail_noise_weight;
    f32 detail_noise_scale;
    f32x3 detail_noise_fbm_weights;

    f32 cirrus_height;
    f32 cirrus_scale;
    f32 cirrus_density;
    f32 cirrus_coverage;

    f32rgb sky_color;
    f32 sky_brightness;
    f32 brightness_gradient_strength;

    f32 sun_radius;
});

Reflect(DOT_CloudsRoot, {
    f32x3           camera_position;
    f32             time;
    f32x3           camera_forward;
    f32             tan_half_fov_x;
    f32x3           camera_right;
    f32             tan_half_fov_y;
    f32x3           camera_up;
    f32             z_near;
    f32             z_far;
    RN_TextureSlot  depth_slot;
    u32             repeat_sampler_slot;
    u32             clamp_sampler_slot;
    RN_TextureSlot  weather_map_slot;
    RN_TextureSlot  height_density_slot;
    RN_TextureSlot  noise_slot;
    RN_TextureSlot  detail_noise_slot;
    RN_TextureSlot  cirrus_slot;
    DOT_SunLight    sun;
    DOT_CloudParams params;
});

#endif // !CLOUDS_SHARED_H
