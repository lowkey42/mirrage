#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"
#include "global_uniforms.glsl"

layout(location = 0) in Vertex_data {
	float z;
} vertex_out;

layout(location = 0) out uvec4 voxel_data[2];

layout(set=1, binding = 0) uniform usampler2D mask_sampler;


void main() {
	if(vertex_out.z<0.5) {
		voxel_data[0] = texelFetch(mask_sampler, ivec2(int(clamp(vertex_out.z*2*32*4, 0, 32*4)), 0), 0);
		voxel_data[1]  = uvec4(0);

	} else {
		voxel_data[0] = ~uvec4(0);
		voxel_data[1] = texelFetch(mask_sampler, ivec2(int(clamp((vertex_out.z-0.5)*2*32*4, 0, 32*4)), 0), 0);
	}
}
