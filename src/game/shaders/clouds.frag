#include "renderer/shaders/shader_shared.h"
#include "game/shaders/clouds_shared.h"

static const float  PI                              = 3.14159265358979;
static const float  EARTH_RADIUS                    = 6300e3;
static const float3 EARTH_CENTER                    = float3(0.0, -EARTH_RADIUS, 0.0);
static const float3 WIND_DIRECTION                  = float3(0.0, 0.0, 1.0);
static const float  ATMOSPHERE_EXTINCTION_THRESHOLD = 0.001;

struct VSOutput
{
    float4 sv_position : SV_Position;
    float2 uv          : TEXCOORD0;
};

struct SunData
{
    float3 light_dir;
    float  mu;
};

float4
sample_2d(uint texture_slot, uint sampler_slot, float2 uv)
{
    Texture2D    tex = ResourceDescriptorHeap[texture_slot];
    SamplerState smp = SamplerDescriptorHeap[sampler_slot];
    return tex.SampleLevel(smp, uv, 0.0);
}

float4
sample_3d(uint texture_slot, uint sampler_slot, float3 uvw)
{
    Texture3D    tex = ResourceDescriptorHeap[texture_slot];
    SamplerState smp = SamplerDescriptorHeap[sampler_slot];
    return tex.SampleLevel(smp, uvw, 0.0);
}

float2
ray_sphere_distance(float3 ro, float3 rd, float3 sphere_origin, float sphere_rad)
{
    float3 oc   = ro - sphere_origin;
    float  b    = 2.0 * dot(rd, oc);
    float  c    = dot(oc, oc) - sphere_rad * sphere_rad;
    float  disc = b * b - 4.0 * c;
    if(disc < 0.0){
        return float2(-1.0, -1.0);
    }
    float q  = (-b - sign(b) * sqrt(disc)) / 2.0;
    float t0 = q;
    float t1 = c / q;
    return float2(min(t0, t1), max(t0, t1));
}

float
hash11(float n)
{
    return frac(sin(n) * 43758.5453);
}

float
smoothmin(float a, float b, float k)
{
    float h = saturate(0.5 + 0.5 * (b - a) / k);
    return lerp(b, a, h) - k * h * (1.0 - h);
}

float
henyey_greenstein(float g, float mu)
{
    float gg = g * g;
    return (1.0 / (4.0 * PI)) * ((1.0 - gg) / pow(1.0 + gg - 2.0 * g * mu, 1.5));
}

float
beers_law(float density, float absorption)
{
    return exp(-density * absorption);
}

float
sugar_powder(float density, float absorption)
{
    return 1.0 - exp(-density * absorption * 2.0);
}

float
simple_sdf(DOT_CloudsRoot *r, float3 p)
{
    float2 normalized_coordinates = p.xz / (r->params.weathermap_tile_radius * 2.0);
    float  map_coverage = sample_2d(r->weather_map_slot, r->repeat_sampler_slot, normalized_coordinates + 0.5).r;

    float3 wind               = WIND_DIRECTION * r->time * r->params.wind_speed * 0.001;
    float4 clouds_layers      = sample_3d(r->noise_slot, r->repeat_sampler_slot, p / r->params.clouds_noise_scale - wind);
    float4 normalized_weights = r->params.clouds_noise_fbm_weights / dot(r->params.clouds_noise_fbm_weights, float4(1.0, 1.0, 1.0, 1.0));
    float  clouds_fbm         = dot(clouds_layers, normalized_weights);

    float atmosphere_height = length(p - EARTH_CENTER) - EARTH_RADIUS;
    float normalized_y      = saturate((atmosphere_height - r->params.clouds_min_height) / (r->params.clouds_max_height - r->params.clouds_min_height));
    float height_gradient   = max(0.0, sample_2d(r->height_density_slot, r->clamp_sampler_slot, float2(normalized_y, 0.0)).r);

    return saturate(smoothmin(map_coverage, clouds_fbm, r->params.noise_weight * 2.0) * r->params.map_density * height_gradient);
}

float scene_sdf(DOT_CloudsRoot *r, float3 p)
{
    float clouds = simple_sdf(r, p);

    float3 wind               = WIND_DIRECTION * r->time * r->params.wind_speed * 0.001;
    float3 normalized_weights = r->params.detail_noise_fbm_weights / dot(r->params.detail_noise_fbm_weights, float3(1.0, 1.0, 1.0));
    float3 clouds_degrade     = sample_3d(r->detail_noise_slot, r->repeat_sampler_slot, p / r->params.detail_noise_scale - wind).rgb;
    float  detail_fbm         = dot(clouds_degrade, normalized_weights) * r->params.detail_noise_weight;

    float one_minus_clouds    = 1.0 - clouds;
    float detail_erode_weight = one_minus_clouds * one_minus_clouds * one_minus_clouds;
    return max(0.0, clouds - detail_fbm * detail_erode_weight);
}

float
light_scattering(DOT_CloudsRoot *r, float total_density, SunData sun_data)
{
    float in_scattering     = beers_law(total_density, r->params.scattering_coefficient_clouds);
    float out_scattering    = sugar_powder(total_density, r->params.scattering_coefficient_sugar);
    float inv_mu_normalized = -sun_data.mu * 0.5 + 0.5;
    return lerp(in_scattering, in_scattering + out_scattering, inv_mu_normalized);
}

float
lightmarch_simple_sdf(DOT_CloudsRoot *r, float3 p, SunData sun_data)
{
    float total_density = 0.0;
    for(uint i = 0; i < r->params.max_steps_lights; i++){
        float step_size = lerp(r->params.light_step_short, r->params.light_step_long, float(i) / float(r->params.max_steps_lights));
        total_density += simple_sdf(r, p) * step_size;
        p += sun_data.light_dir * step_size * float(i);
    }
    return light_scattering(r, total_density, sun_data);
}

float
lightmarch(DOT_CloudsRoot *r, float3 p, SunData sun_data)
{
    float total_density = 0.0;
    for(uint i = 0; i < r->params.max_steps_lights; i++){
        float step_size = lerp(r->params.light_step_short, r->params.light_step_long, float(i) / float(r->params.max_steps_lights));
        total_density += scene_sdf(r, p) * step_size;
        p += sun_data.light_dir * step_size * float(i);
    }
    total_density += scene_sdf(r, p + sun_data.light_dir * 1000.0) * 1000.0;
    return light_scattering(r, total_density, sun_data);
}

float4
raymarch(DOT_CloudsRoot *r, float3 ro, float3 rd, float max_distance_travelled, SunData sun_data)
{
    float4 light = float4(0.0, 0.0, 0.0, 1.0);

    float  phase   = lerp(henyey_greenstein(-r->params.scattering_aniso_bw, sun_data.mu), henyey_greenstein(r->params.scattering_aniso_fw, sun_data.mu), 0.5);
    float3 ambient = r->sun.ambient_color * r->sun.ambient_energy;
    float  dither  = hash11(dot(rd, float3(12.256, 2.646, 6.356)) + r->time);

    float view_height_dot      = abs(rd.y);
    uint  total_marching_steps = uint(lerp(float(r->params.max_marching_steps), float(r->params.min_marching_steps), view_height_dot));
    float step_size            = max_distance_travelled / float(total_marching_steps);

    float  depth           = dither * step_size;
    float3 sample_position = ro + depth * rd;
    for(uint i = 0; i < total_marching_steps; i++){
        float point_density = scene_sdf(r, sample_position);
        if(point_density > 0.001){
            float light_transmittance = light.a < r->params.fast_march_transmittance_thresh
                ? lightmarch_simple_sdf(r, sample_position, sun_data)
                : lightmarch(r, sample_position, sun_data);
            float3 radiance = (ambient + r->sun.energy * r->sun.color * light_transmittance * phase) * point_density;
            float  local_scattering      = point_density * r->params.scattering_coefficient_clouds;
            float  step_scattering       = beers_law(step_size * point_density, r->params.scattering_coefficient_clouds);
            float3 integrated_scattering = (radiance - radiance * step_scattering) / local_scattering;
            light.rgb += integrated_scattering * light.a;
            light.a   *= step_scattering;
            if(light.a < 0.01){
                break;
            }
        }
        depth           += step_size;
        sample_position += step_size * rd;
        if(depth >= max_distance_travelled){
            break;
        }
    }
    return light;
}

float3
render_sun(DOT_CloudsRoot *r, float mu)
{
    float mu_normalized = max(1.0 - (mu * 0.5 + 0.5), 1e-6);
    float ratio         = r->params.sun_radius / mu_normalized;
    float intensity     = (1.0 - exp(-25.0 * ratio)) * 0.1;
    intensity          += (1.0 - exp(-0.05 * ratio * ratio)) * 2.0;
    return float3(intensity, intensity * 0.6, 0.0) * r->sun.energy;
}

float3
render_sky(DOT_CloudsRoot *r, float3 rd)
{
    float3 background = 3.0 * r->params.sky_color;
    background += r->params.sky_brightness * max(0.0, 1.0 - r->params.brightness_gradient_strength * rd.y);
    return background;
}

float
render_high_clouds(DOT_CloudsRoot *r, float3 ro, float3 rd)
{
    float cirrus_distance = ray_sphere_distance(ro, rd, EARTH_CENTER, EARTH_RADIUS + r->params.cirrus_height).y;
    float below_cirrus    = step(0.0, cirrus_distance);

    float3 wind       = (float3(0.0, 1.0, 0.0) - WIND_DIRECTION) * r->time * r->params.wind_speed * 0.0005;
    float2 cirrus_tex = sample_3d(r->cirrus_slot, r->repeat_sampler_slot, (ro + rd * cirrus_distance) / r->params.cirrus_scale + wind).rg;
    float  high       = cirrus_tex.r * cirrus_tex.g / 2.0;
    high              = max(0.0, high - (1.0 - r->params.cirrus_coverage));
    high             *= r->params.cirrus_density * exp(-r->params.extinction_coefficient_sky * abs(cirrus_distance));
    return high * below_cirrus;
}

float2
get_ray_start_end(DOT_CloudsRoot *r, float3 ro, float3 rd)
{
    float2 inner_sphere_dst = ray_sphere_distance(ro, rd, EARTH_CENTER, EARTH_RADIUS + r->params.clouds_min_height);
    float2 outer_sphere_dst = ray_sphere_distance(ro, rd, EARTH_CENTER, EARTH_RADIUS + r->params.clouds_max_height);

    float ray_start_dst = inner_sphere_dst.y;
    float ray_end_dst   = outer_sphere_dst.y;

    bool inside_inner          = inner_sphere_dst.x < 0.0;
    bool inside_outer          = outer_sphere_dst.x < 0.0;
    bool no_inner_intersection = inner_sphere_dst.x < 0.0 && inner_sphere_dst.y < 0.0;
    if(no_inner_intersection){
        ray_start_dst = max(0.0, outer_sphere_dst.x);
    }else if(!inside_outer){
        ray_start_dst = outer_sphere_dst.x;
        ray_end_dst   = inner_sphere_dst.x;
    }else if(!inside_inner){
        ray_start_dst = 0.0;
        ray_end_dst   = inner_sphere_dst.x;
    }

    return float2(ray_start_dst, min(ray_end_dst, r->params.stop_distance) - ray_start_dst);
}

[shader("fragment")]
float4
main(VSOutput input) : SV_Target
{
    DOT_CloudsRoot *r = (DOT_CloudsRoot*)g_root.addr;

    Texture2D depth_texture    = ResourceDescriptorHeap[r->depth_slot];
    float     normalized_depth = depth_texture.Load(int3(int2(input.sv_position.xy), 0)).r;
    bool      valid_depth      = normalized_depth < 1.0;

    float2 ndc = input.uv * 2.0 - 1.0;
    float3 ro  = r->camera_position;
    float3 rd  = normalize(r->camera_forward
                         + ndc.x * r->tan_half_fov_x * r->camera_right
                         - ndc.y * r->tan_half_fov_y * r->camera_up);

    float depth_a        = r->z_far / (r->z_near - r->z_far);
    float depth_b        = r->z_far * r->z_near / (r->z_near - r->z_far);
    float eye_depth      = depth_b / (normalized_depth + depth_a);
    float scene_distance = eye_depth / dot(rd, r->camera_forward);

    float2 start_end      = get_ray_start_end(r, ro, rd);
    float  ray_start_dst  = start_end.x;
    float  ray_travel_dst = start_end.y;

    SunData sun_data;
    sun_data.light_dir = normalize(r->sun.direction);
    sun_data.mu        = dot(rd, sun_data.light_dir);

    float3 sky         = render_sky(r, rd);
    float3 sun         = render_sun(r, sun_data.mu);
    float  high_clouds = render_high_clouds(r, ro, rd);
    float3 skybox      = sky + high_clouds + sun;

    float atmosphere_extinction = exp(-r->params.extinction_coefficient_sky * ray_start_dst);
    float scene_extinction      = exp(-r->params.extinction_coefficient_sky * scene_distance);
    float distance_to_threshold = -log(ATMOSPHERE_EXTINCTION_THRESHOLD) / r->params.extinction_coefficient_sky;
    bool  extinction_too_high   = ray_start_dst > distance_to_threshold;
    bool  scene_under_clouds    = scene_distance < ray_start_dst;

    if(r->params.debug_view != DOT_CloudDebugView_Final){
        float3 debug        = float3(0.0, 0.0, 0.0);
        bool   has_segment  = ray_travel_dst > 0.0;
        float3 march_origin = ro + rd * ray_start_dst;
        DOT_CloudDebugView view = r->params.debug_view;
        if(view == DOT_CloudDebugView_SceneDepth){
            debug = valid_depth ? float3(1.0, 1.0, 1.0) * saturate(scene_distance / r->z_far) : float3(0.0, 0.0, 1.0);
        }else if(view == DOT_CloudDebugView_RaySegment){
            debug = float3(saturate(ray_start_dst / r->params.stop_distance), saturate(ray_travel_dst / r->params.stop_distance), has_segment ? 0.0 : 1.0);
        }else if(has_segment && view == DOT_CloudDebugView_Coverage){
            float2 uv = march_origin.xz / (r->params.weathermap_tile_radius * 2.0) + 0.5;
            debug = sample_2d(r->weather_map_slot, r->repeat_sampler_slot, uv).rrr;
        }else if(has_segment && view == DOT_CloudDebugView_Density){
            float max_density = 0.0;
            for(uint i = 0; i < 32; i++){
                float3 p = march_origin + rd * (ray_travel_dst * (float(i) + 0.5) / 32.0);
                max_density = max(max_density, scene_sdf(r, p));
            }
            debug = float3(max_density, max_density, max_density);
        }else if(has_segment && view == DOT_CloudDebugView_Transmittance){
            float opacity = 1.0 - raymarch(r, march_origin, rd, ray_travel_dst, sun_data).w;
            debug = float3(opacity, opacity, opacity);
        }else if(has_segment && view == DOT_CloudDebugView_CloudColor){
            debug = raymarch(r, march_origin, rd, ray_travel_dst, sun_data).rgb;
        }
        return float4(debug, 0.0);
    }

    float3 color = skybox;
    float  alpha = 0.0;
    if(valid_depth && (scene_under_clouds || r->params.map_density <= 0.0 || extinction_too_high || ray_travel_dst <= 0.0)){
        color = sky * (1.0 - scene_extinction);
        alpha = scene_extinction;
    }else if(!extinction_too_high && ray_travel_dst > 0.0){
        float3 march_origin = ro + rd * ray_start_dst;
        if(!valid_depth){
            float4 result        = raymarch(r, march_origin, rd, ray_travel_dst, sun_data);
            float  transmittance = result.w;
            float3 clouds_skybox = skybox * transmittance + result.rgb;
            color = lerp(skybox, clouds_skybox, atmosphere_extinction);
        }else{
            float4 result        = raymarch(r, march_origin, rd, min(ray_travel_dst, scene_distance - ray_start_dst), sun_data);
            float  transmittance = result.w;
            color = result.rgb * scene_extinction + sky * (1.0 - scene_extinction);
            alpha = transmittance * scene_extinction;
        }
    }

    return float4(color, alpha);
}
