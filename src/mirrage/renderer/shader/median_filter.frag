#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "median.glsl"

layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;

void main() {
	ivec2 color_sampler_size = textureSize(color_sampler, 0).xy;
	ivec2 uv = ivec2(color_sampler_size * vertex_out.tex_coords);

	if(uv.x<=1 || uv.y<=1 || uv.x>=color_sampler_size.x-2 || uv.y>=color_sampler_size.y-2) {
		out_color = vec4(texelFetch(color_sampler, uv, 0).rgb, 1);
		return;
	}

	vec3 colors[9] = vec3[](
		texelFetchOffset(color_sampler, uv, 0, ivec2(-1,-1)).rgb,
		texelFetchOffset(color_sampler, uv, 0, ivec2(-1, 0)).rgb,
		texelFetchOffset(color_sampler, uv, 0, ivec2( 0,-1)).rgb,
		texelFetchOffset(color_sampler, uv, 0, ivec2( 0, 1)).rgb,
		texelFetchOffset(color_sampler, uv, 0, ivec2( 0, 0)).rgb,
		texelFetchOffset(color_sampler, uv, 0, ivec2( 1,-1)).rgb,
		texelFetchOffset(color_sampler, uv, 0, ivec2(-1, 1)).rgb,
		texelFetchOffset(color_sampler, uv, 0, ivec2( 1, 0)).rgb,
		texelFetchOffset(color_sampler, uv, 0, ivec2( 1, 1)).rgb
	);

	vec3 org_c = colors[4];
	out_color = vec4(mix(org_c, median_vec3(colors), 0.9), 1.0);
}
