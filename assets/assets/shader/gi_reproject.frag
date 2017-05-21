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

layout(set=1, binding = 1) uniform sampler2D depth_sampler;
layout(set=1, binding = 5) uniform sampler2D history_sampler;

layout(push_constant) uniform Push_constants {
	mat4 reprojection;
	vec4 arguments;
} pcs;


void main() {
	float depth = textureLod(depth_sampler, vertex_out.tex_coords, 0.0).r;

	vec4 prev_uv = pcs.reprojection * vec4(vertex_out.tex_coords*2.0-1.0, depth, 1.0);
	prev_uv.xy = (prev_uv.xy/prev_uv.w)*0.5+0.5;

	if(prev_uv.x<0.0 || prev_uv.x>1.0 || prev_uv.y<0.0 || prev_uv.y>1.0) {
		out_color = vec4(0, 0, 0, 0);
	} else {
		out_color = vec4(clamp(textureLod(history_sampler, prev_uv.xy, 0.0).rgb*0.99, vec3(0.0), vec3(10.0)), 0);
	}
}
