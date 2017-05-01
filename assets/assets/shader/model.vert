#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 tangent;
layout(location = 3) in vec2 tex_coords;

out Vertex_data {
	vec3 world_pos;
	vec3 normal;
	vec3 tangent;
	vec2 tex_coords;
} vertex_out;

layout(binding = 0) uniform Global_uniforms {
	mat4 view_proj;
	mat4 inv_view_proj;
	vec4 eye_pos;
	vec4 proj_planes;
} global_uniforms;

layout(push_constant) uniform Per_model_uniforms {
	mat4 model;
} model_uniforms;

out gl_PerVertex {
	vec4 gl_Position;
};


void main() {
	vec4 world_pos = model_uniforms.model *  vec4(position, 1.0);

	vertex_out.world_pos = world_pos.xyz;
	gl_Position = global_uniforms.view_proj * world_pos;

	vertex_out.normal  = (model_uniforms.model *  vec4(normal, 0.0)).xyz;
	vertex_out.tangent = (model_uniforms.model *  vec4(tangent, 0.0)).xyz;

	vertex_out.tex_coords = tex_coords;
}
