#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"
#include "global_uniforms.glsl"

layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out uvec4 voxel_data[2];

layout(set=1, binding = 0) uniform usampler2DArray voxel_sampler;

layout(push_constant) uniform Push_constants {
	mat4 args;
} pcs;


void main() {
	int lod = int(pcs.args[0][0]);

	ivec2 uv = ivec2(vertex_out.tex_coords * textureSize(voxel_sampler, 0).xy);//*2;

	voxel_data[0] = texelFetch(voxel_sampler, ivec3(uv.x, uv.y, 0), 0)
	              | texelFetch(voxel_sampler, ivec3(uv.x+1, uv.y, 0), 0)
	              | texelFetch(voxel_sampler, ivec3(uv.x, uv.y+1, 0), 0)
	              | texelFetch(voxel_sampler, ivec3(uv.x+1, uv.y+1, 0), 0);

	voxel_data[1] = texelFetch(voxel_sampler, ivec3(uv.x, uv.y, 1), 0)
	              | texelFetch(voxel_sampler, ivec3(uv.x+1, uv.y, 1), 0)
	              | texelFetch(voxel_sampler, ivec3(uv.x, uv.y+1, 1), 0)
	              | texelFetch(voxel_sampler, ivec3(uv.x+1, uv.y+1, 1), 0);
}
