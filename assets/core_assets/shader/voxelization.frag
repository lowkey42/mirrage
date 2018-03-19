#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"
#include "global_uniforms.glsl"

layout(location = 0) in Vertex_data {
	float z;
} vertex_out;

layout(location = 0) out uvec4 voxel_data[2];

layout(set=1, binding = 0) uniform usampler1D mask_sampler;


void main() {
	voxel_data[0] = texelFetch(mask_sampler, int(vertex_out.z/(32.0*4.0)), 0);
	voxel_data[1] = texelFetch(mask_sampler, 0, 0);
}
