#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"

layout(location = 0) in Vertex_data {
	vec3 world_pos;
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 shadowmap_out;

layout(set=1, binding = 0) uniform sampler2D albedo_sampler;

layout(binding = 0) uniform Global_uniforms {
	mat4 view_proj;
	mat4 inv_view_proj;
	vec4 eye_pos;
	vec4 proj_planes;
} global_uniforms;


void main() {
	vec4 albedo = texture(albedo_sampler, vertex_out.tex_coords);

	if(albedo.a < 0.1)
		discard;

	float z = gl_FragCoord.z;
	shadowmap_out = vec4(z, 0, 0, 0);
}
