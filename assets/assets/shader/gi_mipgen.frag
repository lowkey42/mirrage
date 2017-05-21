#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"
#include "poisson.glsl"
#include "random.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_depth;
layout(location = 2) out vec4 out_mat_data;

layout(set=1, binding = 0) uniform sampler2D color_sampler;
layout(set=1, binding = 1) uniform sampler2D depth_sampler;
layout(set=1, binding = 2) uniform sampler2D mat_data_sampler;

layout(push_constant) uniform Push_constants {
	mat4 reprojection;
	vec4 arguments;
} pcs;


void main() {
	float src_lod = pcs.arguments.x - 1.0;

	// TODO: should use a more advanced sampling algorithm (see paper)
	out_color = textureLod(color_sampler, vertex_out.tex_coords, src_lod);
	out_depth = textureLod(depth_sampler, vertex_out.tex_coords, src_lod);
	out_mat_data = textureLod(mat_data_sampler, vertex_out.tex_coords, src_lod);
}
