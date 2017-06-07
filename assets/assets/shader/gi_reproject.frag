#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"
#include "poisson.glsl"
#include "random.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
	vec3 view_ray;
	flat vec3 corner_view_rays[4];
} vertex_out;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_color_result;

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
	const float PI = 3.14159265359;

	float depth = textureLod(depth_sampler, vertex_out.tex_coords, 0.0).r;

	vec3 pos = depth * vertex_out.view_ray;

	vec4 prev_uv = pcs.reprojection * vec4(pos, 1.0);
	prev_uv.xy = (prev_uv.xy/prev_uv.w)*0.5+0.5;

	if(prev_uv.x<0.0 || prev_uv.x>1.0 || prev_uv.y<0.0 || prev_uv.y>1.0) {
		out_color = vec4(0, 0, 0, 0);
		out_color_result = vec4(0, 0, 0, 0);

	} else {
		vec3 gi = calculate_gi(vertex_out.tex_coords, prev_uv.xy, int(pcs.arguments.r),
		                       result_diff_sampler, result_spec_sampler,
		                       albedo_sampler, mat_data_sampler);

		float ao = mix(1.0, texture(ao_sampler, vertex_out.tex_coords).r, pcs.arguments.a);

		out_color = vec4(clamp(gi*1.0, vec3(0.0), vec3(10.0)), 0.0);

		out_color_result = vec4(gi*0.5, 0.0);
		out_color_result.rgb *= mix(1.0, ao, pcs.arguments.a);
	}
}
