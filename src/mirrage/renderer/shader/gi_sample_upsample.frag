#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D noise_sampler;

layout(set=2, binding = 0) uniform sampler2D color_sampler;
layout(set=2, binding = 1) uniform sampler2D depth_sampler;
layout(set=2, binding = 2) uniform sampler2D mat_data_sampler;
layout(set=2, binding = 3) uniform sampler2D prev_level_result_sampler;
layout(set=2, binding = 4) uniform sampler2D result_sampler;
layout(set=2, binding = 5) uniform sampler2D history_result_sampler;
layout(set=2, binding = 6) uniform sampler2D history_weight_sampler;
layout(set=2, binding = 7) uniform sampler2D prev_depth_sampler;
layout(set=2, binding = 8) uniform sampler2D prev_mat_data_sampler;
layout(set=2, binding = 9) uniform sampler2D ao_sampler;

layout (constant_id = 0) const int NO_PREV_RESULT = 0;

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "upsample.glsl"

void main() {
	vec2 tex_size = textureSize(depth_sampler, 0);
	ivec2 iuv = ivec2(tex_size*vertex_out.tex_coords);
	if(iuv.x<=1 || iuv.y<=1 || iuv.x>=tex_size.x-2 || iuv.y>=tex_size.y-2) {
		out_color = vec4(0,0,0, NO_PREV_RESULT);
		return;
	}
	
	out_color = vec4(upsampled_result(depth_sampler, mat_data_sampler,
	                                  prev_depth_sampler, prev_mat_data_sampler,
	                                  prev_level_result_sampler, vertex_out.tex_coords), NO_PREV_RESULT);
}
