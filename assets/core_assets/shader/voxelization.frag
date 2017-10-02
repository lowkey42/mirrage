#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"
#include "global_uniforms.glsl"

layout(location = 0) in Vertex_data {
	vec3 world_pos;
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out uvec4 voxel_data[2];

layout(set=1, binding = 0) uniform sampler2D albedo_sampler;


void main() {
	vec4 albedo = texture(albedo_sampler, vertex_out.tex_coords);

	if(albedo.a < 0.1)
		discard;

	float z    = gl_FragCoord.z;
	float near = global_uniforms.proj_planes.x;
	float far  = global_uniforms.proj_planes.y;

	// TODO
}
