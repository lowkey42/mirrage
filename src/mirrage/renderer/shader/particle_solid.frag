#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "particle/data_structures.glsl"

layout(early_fragment_tests) in;

layout(location = 0) in vec4 view_pos;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coords;
layout(location = 3) in vec4 out_particle_color;
layout(location = 4) in vec4 out_screen_pos;


layout(location = 0) out vec4 depth_out;
layout(location = 1) out vec4 albedo_mat_id_out;
layout(location = 2) out vec4 mat_data_out;
layout(location = 3) out vec4 color_out;
layout(location = 4) out vec4 color_diffuse_out;

layout(set=1, binding = 0) uniform sampler2D albedo_sampler;
layout(set=1, binding = 1) uniform sampler2D normal_sampler;
layout(set=1, binding = 2) uniform sampler2D brdf_sampler;
layout(set=1, binding = 3) uniform sampler2D emission_sampler;

layout(std140, set=2, binding = 0) readonly buffer Particle_type_config {
	PARTICLE_TYPE_CONFIG
} particle_config;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
	vec4 light_color;
	vec4 options;
} model_uniforms;

const float PI = 3.14159265359;


vec3 decode_tangent_normal(vec2 tn);
vec3 tangent_space_to_world(vec3 N);

void main() {
	vec4 albedo = texture(albedo_sampler, tex_coords);
	albedo *= out_particle_color;

	vec3  N    = tangent_space_to_world(decode_tangent_normal(texture(normal_sampler, tex_coords).rg));

	vec4 brdf = texture(brdf_sampler, tex_coords);
	float roughness = brdf.r;
	float metallic  = brdf.g;
	roughness = mix(0.01, 0.99, roughness*roughness);

	depth_out         = vec4(-view_pos.z/view_pos.w / global_uniforms.proj_planes.y, 0,0,1);
	albedo_mat_id_out = vec4(albedo.rgb, 0.0);
	mat_data_out      = vec4(encode_normal(N), roughness, metallic);

	float emissive_power = texture(emission_sampler, tex_coords).r;

	color_out     = vec4(albedo.rgb * emissive_power * albedo.a, 1.0);
	color_diffuse_out = color_out;
}

vec3 decode_tangent_normal(vec2 tn) {
	if(dot(tn,tn)<0.00001)
		return vec3(0,0,1);

	vec3 N = vec3(tn*2-1, 0);
	N.z = sqrt(1 - dot(N.xy, N.xy));
	return N;
}

vec3 tangent_space_to_world(vec3 N) {
	vec3 VN = normalize(normal);

	vec3 vp = view_pos.xyz / view_pos.w;

	// calculate tangent
	vec3 p_dx = dFdx(vp);
	vec3 p_dy = dFdy(vp);

	vec2 tc_dx = dFdx(tex_coords);
	vec2 tc_dy = dFdy(tex_coords);

	vec3 p_dy_N = cross(p_dy, VN);
	vec3 p_dx_N = cross(VN, p_dx);

	vec3 T = p_dy_N * tc_dx.x + p_dx_N * tc_dy.x;
	vec3 B = p_dy_N * tc_dx.y + p_dx_N * tc_dy.y;

	float inv_max = inversesqrt(max(dot(T,T), dot(B,B)));
	mat3 TBN = mat3(T*inv_max, B*inv_max, VN);
	return normalize(TBN * N);
}

