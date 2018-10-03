#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "median.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_weight;

layout(set=1, binding = 0) uniform sampler2D prev_level_sampler;

layout(push_constant) uniform Push_constants {
	mat4 projection;
	mat4 prev_projection;
} pcs;

void main() {
	ivec2 result_sampler_size = textureSize(prev_level_sampler, 0).xy;
	ivec2 uv = ivec2(result_sampler_size * vertex_out.tex_coords);
	if(uv.x<=0 || uv.y<=0 || uv.x>=result_sampler_size.x-1 || uv.y>=result_sampler_size.y-1) {
		out_weight = vec4(0, 0,0,1);
		return;
	}
	
	vec4 weights = textureGather(prev_level_sampler, vertex_out.tex_coords.xy, 0);

	float min_weight = min(weights[0], min(weights[1], min(weights[2], weights[3])));
	float avg_weight = dot(vec4(1.0), weights) / 4;

	float mip = pcs.prev_projection[0][3];
	float weight = mix(min_weight, avg_weight, clamp(mip/2,0,1));
	out_weight = vec4(weight, 0,0,1);
}
