#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"
#include "global_uniforms.glsl"

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

layout(set=1, binding = 0) uniform sampler2D depth_sampler;
layout(set=1, binding = 1) uniform sampler2D color_sampler;


vec3 read(vec2 uv, float center_depth, inout float weight_sum, float static_weight) {
	float d = texelFetch(depth_sampler, ivec2(uv*textureSize(depth_sampler, 0)), 0).r;
	float dz = center_depth - d;
	float d_dev = mix(0.01, 0.25, d) / global_uniforms.proj_planes.y;
	float w = exp(-dz*dz/(2*d_dev*d_dev)) * static_weight;
	weight_sum += w;

	return texture(color_sampler, uv).rgb * w;
}

void main() {
	float center_depth = texelFetch(depth_sampler, ivec2(vertex_out.uv_center*textureSize(depth_sampler, 0)), 0).r;

	float weight_sum = 0.1964825501511404;
	vec3 result = texture(color_sampler, vertex_out.uv_center).rgb * 0.1964825501511404;
	result += read(vertex_out.uv_l1, center_depth, weight_sum, 0.2969069646728344);
	result += read(vertex_out.uv_l2, center_depth, weight_sum, 0.09447039785044732);
	result += read(vertex_out.uv_l3, center_depth, weight_sum, 0.010381362401148057);

	result += read(vertex_out.uv_r1, center_depth, weight_sum, 0.2969069646728344);
	result += read(vertex_out.uv_r2, center_depth, weight_sum, 0.09447039785044732);
	result += read(vertex_out.uv_r3, center_depth, weight_sum, 0.010381362401148057);

	color_out = vec4(result / weight_sum, 1.0);

//	color_out.rgb = read(vertex_out.uv_center, center_depth, weight_sum, 1.0).rgb;
}
