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


vec3 read(vec2 uv, inout float weight_sum, float static_weight) {
	weight_sum += static_weight;

	return textureLod(color_sampler, uv, pcs.options.y).rgb * static_weight;
}

void main() {
	float weight_sum=0;
	vec3 result = vec3(0,0,0);
	result += read(vertex_out.uv_center, weight_sum, 0.1964825501511404);
	result += read(vertex_out.uv_l1, weight_sum, 0.2969069646728344);
	result += read(vertex_out.uv_l2, weight_sum, 0.09447039785044732);
	result += read(vertex_out.uv_l3, weight_sum, 0.010381362401148057);

	result += read(vertex_out.uv_r1, weight_sum, 0.2969069646728344);
	result += read(vertex_out.uv_r2, weight_sum, 0.09447039785044732);
	result += read(vertex_out.uv_r3, weight_sum, 0.010381362401148057);

	color_out = vec4(result / weight_sum, 1.0);

	if(COMBINE>0) {
		for(int i=1; i<COMBINE; i++) {
			color_out.rgb += textureLod(downsampled_color_sampler, vertex_out.uv_center, i).rgb;
		}
	}

//	color_out.rgb = read(vertex_out.uv_center, center_depth, weight_sum, 1.0).rgb;
}
