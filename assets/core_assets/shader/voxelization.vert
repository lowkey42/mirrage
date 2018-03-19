#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"


layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coords;

layout(location = 0) out Vertex_data {
	float z;
} vertex_out;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
} model_uniforms;

out gl_PerVertex {
	vec4 gl_Position;
};


void main() {
	vec4 world_pos = model_uniforms.model *  vec4(position, 1.0);

	gl_Position = global_uniforms.view_proj_mat * world_pos;

	vertex_out.z = (global_uniforms.view_mat * world_pos).z / global_uniforms.proj_planes.y;
}
