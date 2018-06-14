#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"


layout(location = 0) in Vertex_data {
	vec2 uv_center;
	vec2 uv_l1;
	vec2 uv_l2;
	vec2 uv_l3;
	vec2 uv_r1;
	vec2 uv_r2;
	vec2 uv_r3;
} vertex_out;

layout(location = 0) out vec4 color_out;

layout (constant_id = 0) const int HORIZONTAL = 1;
layout (constant_id = 1) const int COMBINE = 0;

layout(set=1, binding = 0) uniform sampler2D color_sampler;
layout(set=1, binding = 1) uniform sampler2D downsampled_color_sampler;

layout(push_constant) uniform Settings {
	vec4 options;
} pcs;


vec3 read(vec2 uv, float static_weight) {
	return textureLod(color_sampler, uv, round(pcs.options.x)).rgb * static_weight;
}

void main() {
	vec3 result = vec3(0,0,0);
	result += read(vertex_out.uv_center, 0.1964825501511404);
	result += read(vertex_out.uv_l1, 0.2969069646728344);
	result += read(vertex_out.uv_l2, 0.09447039785044732);
	result += read(vertex_out.uv_l3, 0.010381362401148057);

	result += read(vertex_out.uv_r1, 0.2969069646728344);
	result += read(vertex_out.uv_r2, 0.09447039785044732);
	result += read(vertex_out.uv_r3, 0.010381362401148057);

	if(COMBINE>0) {
		result += textureLod(downsampled_color_sampler, vertex_out.uv_center, 0).rgb;
	}

	color_out = vec4(result, 1.0);

//	color_out.rgb = read(vertex_out.uv_center, center_depth, weight_sum, 1.0).rgb;
}
