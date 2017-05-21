#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"

layout(location = 0) in Vertex_data {
	vec3 world_pos;
	vec3 normal;
	vec3 tangent;
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 depth_out;
layout(location = 1) out vec4 albedo_mat_id;
layout(location = 2) out vec4 mat_data;

layout(set=1, binding = 0) uniform sampler2D albedo_sampler;

layout(binding = 0) uniform Global_uniforms {
	mat4 view_proj;
	vec4 eye_pos;
	float disect;
} global_uniforms;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
	vec4 light_color;
	vec4 options;
} model_uniforms;


void main() {
	vec4 albedo = texture(albedo_sampler, vertex_out.tex_coords);

	if(albedo.a < 0.1)
		discard;

	depth_out     = vec4(gl_FragCoord.z, 0,0,0);
	albedo_mat_id = vec4(albedo.rgb, 1.0);
	mat_data      = vec4(0,0,0,0);
}
