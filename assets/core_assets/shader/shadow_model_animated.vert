#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"


layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec2 tex_coords;
layout(location = 3) in ivec4 bone_ids;
layout(location = 4) in vec4 bone_weights;

layout(location = 0) out Vertex_data {
	vec3 world_pos;
	vec2 tex_coords;
} vertex_out;

layout(set=2, binding = 0, std140) uniform Bone_uniforms {
	mat3x4 offset[64];
} bones;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
	mat4 light_view_proj;
} model_uniforms;

out gl_PerVertex {
	vec4 gl_Position;
};


void main() {
	float unused_weight = 1.0 - dot(bone_weights, vec4(1.0));

	vec3 p = (vec4(position, 1.0) * bones.offset[bone_ids[0]]) * bone_weights[0]
	       + (vec4(position, 1.0) * bones.offset[bone_ids[1]]) * bone_weights[1]
	       + (vec4(position, 1.0) * bones.offset[bone_ids[2]]) * bone_weights[2]
	       + (vec4(position, 1.0) * bones.offset[bone_ids[3]]) * bone_weights[3]
	       + position * unused_weight;

	vec4 world_pos = model_uniforms.model * vec4(p, 1.0);
	vertex_out.world_pos = world_pos.xyz;
	gl_Position = model_uniforms.light_view_proj * world_pos;
	vertex_out.tex_coords = tex_coords;
}
