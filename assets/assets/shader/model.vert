#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"


layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coords;

layout(location = 0) out Vertex_data {
	vec3 world_pos;
	vec3 view_pos;
	vec3 normal;
	vec2 tex_coords;
} vertex_out;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
} model_uniforms;

out gl_PerVertex {
	vec4 gl_Position;
};


void main() {
	mat4 model_to_view = global_uniforms.view_mat * model_uniforms.model;
	vec4 world_pos = model_uniforms.model * vec4(position, 1.0);
	vec4 view_pos  = model_to_view * vec4(position, 1.0);

	vertex_out.world_pos = world_pos.xyz / world_pos.w;
	vertex_out.view_pos = view_pos.xyz / view_pos.w;
	gl_Position = global_uniforms.proj_mat * view_pos;

	vertex_out.normal  = (model_to_view *  vec4(normal, 0.0)).xyz;

	vertex_out.tex_coords = tex_coords;
}
