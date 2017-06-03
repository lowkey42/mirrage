#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"

layout(location = 0) in Vertex_data {
	vec3 world_pos;
	vec3 view_pos;
	vec3 normal;
	vec3 tangent;
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 depth_out;
layout(location = 1) out vec4 albedo_mat_id;
layout(location = 2) out vec4 mat_data;

layout(set=1, binding = 0) uniform sampler2D albedo_sampler;
layout(set=1, binding = 1) uniform sampler2D metallic_sampler;
layout(set=1, binding = 2) uniform sampler2D normal_sampler;
layout(set=1, binding = 3) uniform sampler2D roughness_sampler;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
	vec4 light_color;
	vec4 options;
} model_uniforms;

const float PI = 3.14159265359;


vec3 tangent_space_to_world(vec3 N);

void main() {
	vec4 albedo = texture(albedo_sampler, vertex_out.tex_coords);

	if(albedo.a < 0.1)
		discard;

	float metallic  = texture(metallic_sampler, vertex_out.tex_coords).r;
	float roughness = texture(roughness_sampler, vertex_out.tex_coords).r;
	vec3  normal    = tangent_space_to_world(texture(normal_sampler, vertex_out.tex_coords, -0.5).xyz);

	roughness = mix(0.1, 0.99, roughness*roughness);

	depth_out     = vec4(-vertex_out.view_pos.z / global_uniforms.proj_planes.y, 0,0,1);
	albedo_mat_id = vec4(albedo.rgb, 0.0);
	mat_data      = vec4(encode_normal(normal), roughness, metallic);

	float disect = model_uniforms.options.x;
	if(disect>0 && vertex_out.world_pos.z>=disect)
		discard;
}

vec3 tangent_space_to_world(vec3 N) {
	vec3 VN = normalize(vertex_out.normal);

// calculate tangent (assimp generated tangent contain weird artifacts)
	vec3 p_dx = dFdx(vertex_out.view_pos);
	vec3 p_dy = dFdy(vertex_out.view_pos);

	vec2 tc_dx = dFdx(vertex_out.tex_coords);
	vec2 tc_dy = dFdy(vertex_out.tex_coords);
	vec3 VT = normalize( tc_dy.y * p_dx - tc_dx.y * p_dy );

	VT = normalize(VT - dot(VT, VN) * VN);
	vec3 VB = cross(VT, VN);
	mat3 TBN = mat3(VT, VB, VN);

	if(length(N)<0.00001)
		N = vec3(0,0,1);
	else {
		N = normalize(N*2.0 - 1.0);
	}
	return normalize(TBN * N);
}

