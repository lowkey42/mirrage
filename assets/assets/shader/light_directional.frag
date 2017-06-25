#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "poisson.glsl"
#include "random.glsl"
#include "brdf.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
	vec3 view_ray;
	flat vec3 corner_view_rays[4];
} vertex_out;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_color_diff;

layout(input_attachment_index = 0, set=1, binding = 0) uniform subpassInput depth_sampler;
layout(input_attachment_index = 1, set=1, binding = 1) uniform subpassInput albedo_mat_id_sampler;
layout(input_attachment_index = 2, set=1, binding = 2) uniform subpassInput mat_data_sampler;

layout(set=2, binding = 0) uniform texture2D shadowmaps[1];
layout(set=2, binding = 1) uniform samplerShadow shadowmap_shadow_sampler; // sampler2DShadow
layout(set=2, binding = 2) uniform sampler shadowmap_depth_sampler; // sampler2D

layout (constant_id = 0) const int SHADOW_QUALITY = 1;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
	vec4 light_color;
	vec4 light_data;  // R=src_radius, GBA=direction
	vec4 light_data2; // R=shadowmapID
} model_uniforms;


const float PI = 3.14159265359;

float sample_shadowmap(vec3 world_pos);


void main() {
	float depth         = subpassLoad(depth_sampler).r;
	vec4  albedo_mat_id = subpassLoad(albedo_mat_id_sampler);
	vec4  mat_data      = subpassLoad(mat_data_sampler);

	vec3 position = depth * vertex_out.view_ray;
	vec3 V = -normalize(position);
	vec3 albedo = albedo_mat_id.rgb;
	int  material = int(albedo_mat_id.a*255);

	// material 255 (unlit)
	if(material==255) {
		out_color = out_color_diff = vec4(albedo*10.0, 1.0);
		return;
	}

	// material 0 (default)
	vec3 N = decode_normal(mat_data.rg);
	float roughness = mat_data.b;
	float metallic = mat_data.a;

	vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
	albedo.rgb *= 1.0 - metallic;
	vec3 radiance = model_uniforms.light_color.rgb * model_uniforms.light_color.a;


	float shadow = sample_shadowmap(position + N*0.06);

	out_color = vec4(0,0,0,0);
	out_color_diff = vec4(0,0,0,0);

	if(shadow>0.0) {
		vec3 L = model_uniforms.light_data.gba;

		vec3 diffuse;
		out_color = vec4(brdf(albedo, F0, roughness, N, V, L, radiance, diffuse) * shadow, 1.0);
		out_color_diff = vec4(diffuse * shadow, 1.0);
	}

	// TODO: remove ambient
	out_color.rgb += albedo * radiance * 0.00001;
}


float calc_penumbra(vec3 surface_lightspace, float light_size,
                    out int num_occluders);

float sample_shadowmap(vec3 view_pos) {
	int shadowmap = int(model_uniforms.light_data2.r);
	if(shadowmap<0)
		return 1.0;

	vec4 lightspace_pos = model_uniforms.model * vec4(view_pos, 1.0);
	lightspace_pos /= lightspace_pos.w;
	lightspace_pos.xy = lightspace_pos.xy * 0.5 + 0.5;

	float shadowmap_size = textureSize(sampler2D(shadowmaps[shadowmap], shadowmap_depth_sampler), 0).x;
	float light_size = model_uniforms.light_data.r / 800.0;

	int num_occluders;
	float penumbra_softness = calc_penumbra(lightspace_pos.xyz, light_size, num_occluders);

	//return penumbra_softness>=0.5 ? 1.0 : 0.0;

	if(num_occluders==0 && SHADOW_QUALITY<=1)
		return 1.0;

	float sample_size = mix(2.0/shadowmap_size, light_size, penumbra_softness);
	int samples = int(mix(4, SHADOW_QUALITY<=1 ? 8 : 16, penumbra_softness));
	if(num_occluders>=4)
		samples = min(samples, 8);

	if(SHADOW_QUALITY<=1)
		samples = min(samples, 8);

	float z_bias = 0.00035;

	float angle = random(vec4(lightspace_pos.xyz, global_uniforms.time.y));
	float sin_angle = sin(angle);
	float cos_angle = cos(angle);

	float visiblity = 1.0;
	for (int i=0;i<min(samples, 16);i++) {
		vec2 point = samples <= 8 ? (samples <= 4 ? Poisson4[i] : Poisson8[i]) : Poisson16[i];

		vec2 offset = vec2(point.x*cos_angle - point.y*sin_angle, point.x*sin_angle + point.y*cos_angle);

		vec2 p = lightspace_pos.xy + offset * sample_size;

		visiblity -= 1.0/samples * (1.0 - texture(sampler2DShadow(shadowmaps[shadowmap], shadowmap_shadow_sampler),
		                                     vec3(p, lightspace_pos.z-z_bias)));
	}

	return clamp(smoothstep(0, 1, visiblity), 0.0, 1.0);
}

float calc_avg_occluder(vec3 surface_lightspace, float search_area,
                        out int num_occluders) {
	int shadowmap = int(model_uniforms.light_data2.r);

	float depth_acc;
	float depth_count;
	num_occluders = 0;
#
	float angle = random(vec4(surface_lightspace, global_uniforms.time.y));
	float sin_angle = sin(angle);
	float cos_angle = cos(angle);

	for (int i=0;i<4;i++) {
		vec2 offset = vec2(Poisson4[i].x*cos_angle - Poisson4[i].y*sin_angle, Poisson4[i].x*sin_angle + Poisson4[i].y*cos_angle);
		
		float depth = texture(sampler2D(shadowmaps[shadowmap], shadowmap_depth_sampler),
		                      surface_lightspace.xy + offset * search_area).r;
		if(depth < surface_lightspace.z - 0.0004) {
			depth_acc += depth;
			depth_count += 1.0;
			num_occluders += 1;
		}
	}

	if(num_occluders==0)
		return 1.0;

	return depth_acc / depth_count;
}

float calc_penumbra(vec3 surface_lightspace, float light_size, out int num_occluders) {
	float avg_occluder = calc_avg_occluder(surface_lightspace, light_size, num_occluders);

	const float scale = 10.0;
	float softness = (surface_lightspace.z - avg_occluder) * scale;

	return smoothstep(0.2, 1.0, softness);
}
