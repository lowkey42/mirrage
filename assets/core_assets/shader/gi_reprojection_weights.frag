#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "median.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_weight;    // weight of reprojected GI

layout(set=1, binding = 0) uniform sampler2D success_sampler;


void main() {
	vec2 hws_size = textureSize(success_sampler, 0);
	vec2 hws_step = 1.0 / hws_size;
	ivec2 hws_uv = ivec2(hws_size * vertex_out.tex_coords.xy);

	if(hws_uv.x<=1 || hws_uv.y<=1 || hws_uv.x>=hws_size.x-2 || hws_uv.y>=hws_size.y-2) {
		out_weight = vec4(0,0,0,1);
		return;
	}
	
	float weights[9];
	vec4 w = textureGather(success_sampler, vertex_out.tex_coords.xy-hws_step, 0);
	weights[0] = w[0];
	weights[1] = w[1];
	weights[2] = w[2];
	weights[3] = w[3];
	weights[4] = texelFetchOffset(success_sampler, hws_uv, 0, ivec2(1,-1)).r;
	weights[5] = texelFetchOffset(success_sampler, hws_uv, 0, ivec2(1, 0)).r;
	weights[6] = texelFetchOffset(success_sampler, hws_uv, 0, ivec2(1, 1)).r;
	weights[7] = texelFetchOffset(success_sampler, hws_uv, 0, ivec2(-1,1)).r;
	weights[8] = texelFetchOffset(success_sampler, hws_uv, 0, ivec2( 0,1)).r;

	vec2 weight = vec2(weights[0], texelFetch(success_sampler, hws_uv, 0).g);

	for(int i=0; i<9; i++)
		weight.r = min(weight.r, weights[i]);

	weight.r = mix(weight.r, median_float(weights), 0.2);

	out_weight = vec4(min(100, weight.r * (weight.g+1)), 0, 0, 1);
}
