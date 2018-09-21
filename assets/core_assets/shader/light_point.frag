#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "poisson.glsl"
#include "random.glsl"
#include "brdf.glsl"


layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_color_diff;

layout(input_attachment_index = 0, set=1, binding = 0) uniform subpassInput depth_sampler;
layout(input_attachment_index = 1, set=1, binding = 1) uniform subpassInput albedo_mat_id_sampler;
layout(input_attachment_index = 2, set=1, binding = 2) uniform subpassInput mat_data_sampler;

layout(push_constant) uniform Per_model_uniforms {
	mat4 transform;
	vec4 light_color;
	vec4 light_data;  // R=src_radius, GBA=position
	vec4 light_data2; // R=shadowmapID, GB=screen width/height
} model_uniforms;


const float PI = 3.14159265359;

float calc_att(float dist) {
	dist /= model_uniforms.light_data.r;
	float att = 1.0 / max(dist*dist, 0.01*0.01);
	return att;
}

void main() {
	float depth         = subpassLoad(depth_sampler).r;

	vec3 position = position_from_ldepth(gl_FragCoord.xy/model_uniforms.light_data2.gb, depth);
	vec3 L = model_uniforms.light_data.gba - position;
	float dist = length(L);
	L /= dist;

	dist /= model_uniforms.light_data.r;
	float att = 1.0 / max(dist*dist, 0.01*0.01);

	float intensity = model_uniforms.light_color.a * calc_att(dist);
	intensity = max(0.0, intensity-0.01) / (1.0 - 0.01);
	if(intensity<=0.0) {
		out_color = vec4(0,0,0,0);
		out_color_diff = vec4(0,0,0,0);
		return;
	}

	vec3 radiance = model_uniforms.light_color.rgb * intensity;

	vec4  albedo_mat_id = subpassLoad(albedo_mat_id_sampler);
	vec4  mat_data      = subpassLoad(mat_data_sampler);

	vec3 V = -normalize(position);
	vec3 albedo = albedo_mat_id.rgb;
	int  material = int(albedo_mat_id.a*255);

	vec3 N = decode_normal(mat_data.rg);
	float roughness = mat_data.b;
	float metallic = mat_data.a;

	vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
	albedo.rgb *= 1.0 - metallic;

	vec3 diffuse;
	out_color = vec4(brdf(albedo, F0, roughness, N, V, L, radiance, diffuse), 1.0);
	out_color_diff = vec4(diffuse, 1.0);
}

