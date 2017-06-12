#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
	vec3 view_ray;
	flat vec3 corner_view_rays[4];
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D color_sampler;
layout(set=1, binding = 1) uniform sampler2D depth_sampler;
layout(set=1, binding = 2) uniform sampler2D mat_data_sampler;
layout(set=1, binding = 3) uniform sampler2D result_diff_sampler;
layout(set=1, binding = 4) uniform sampler2D result_spec_sampler;
layout(set=1, binding = 5) uniform sampler2D albedo_sampler;
layout(set=1, binding = 7) uniform sampler2D ao_sampler;

layout(push_constant) uniform Push_constants {
	mat4 reprojection;
	vec4 arguments;
} pcs;

#include "gi_blend_common.glsl"


void main() {
	out_color = vec4(calculate_gi(vertex_out.tex_coords, vertex_out.tex_coords, int(pcs.arguments.r),
	                              result_diff_sampler, result_spec_sampler,
	                              albedo_sampler, mat_data_sampler, 1.0)*0.5, 0.0);

//	out_color = vec4(1,1,1,1);

	if(pcs.arguments.a>0.0)
		out_color.rgb *= mix(1.0, texture(ao_sampler, vertex_out.tex_coords).r, pcs.arguments.a);

	if(pcs.arguments.b>=0)
		out_color = vec4(textureLod(result_diff_sampler, vertex_out.tex_coords, pcs.arguments.b).rgb, 1.0);

//	out_color = vec4(texture(ao_sampler, vertex_out.tex_coords).rrr, 1.0);
}
