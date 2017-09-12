#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D depth_sampler;
layout(set=1, binding = 1) uniform sampler2D mat_data_sampler;
layout(set=1, binding = 2) uniform sampler2D result_diff_sampler;
layout(set=1, binding = 3) uniform sampler2D result_spec_sampler;
layout(set=1, binding = 4) uniform sampler2D albedo_sampler;
layout(set=1, binding = 5) uniform sampler2D ao_sampler;
layout(set=1, binding = 6) uniform sampler2D brdf_sampler;

layout(push_constant) uniform Push_constants {
	mat4 reprojection;
	mat4 prev_projection;
} pcs;

#include "gi_blend_common.glsl"


void main() {
	vec3 diffuse;
	out_color = vec4(calculate_gi(vertex_out.tex_coords, vertex_out.tex_coords, 1,
	                              result_diff_sampler, result_spec_sampler,
	                              albedo_sampler, mat_data_sampler, brdf_sampler, diffuse), 0.0);

//	out_color = vec4(1,1,1,1);

	if(pcs.prev_projection[3][3]>0.0) {
		float ao = texture(ao_sampler, vertex_out.tex_coords).r;
		ao = mix(1.0, ao, pcs.prev_projection[3][3]);
		out_color.rgb *= ao*0.9 + 0.1;
	}

	if(pcs.prev_projection[2][3]==0) {
		out_color.rgb = texture(result_spec_sampler, vertex_out.tex_coords).rgb;
		out_color.a = 1;

	} else if(pcs.prev_projection[2][3]>=1) {
		out_color.rgb = textureLod(result_diff_sampler, vertex_out.tex_coords, pcs.prev_projection[2][3]-1).rgb;
		out_color.a = 1;

/*
		out_color.rgb = texelFetch(depth_sampler, ivec2(vertex_out.tex_coords
		                                                * textureSize(depth_sampler, int(pcs.prev_projection[2][3]))),
		        int(pcs.prev_projection[2][3])).rrr;
*/

	//	out_color = vec4(decode_normal(texelFetch(mat_data_sampler, ivec2(vertex_out.tex_coords*textureSize(mat_data_sampler, int(pcs.prev_projection[2][3]))), int(pcs.prev_projection[2][3])).rg), 1.0);
	}

//	out_color = vec4(texture(ao_sampler, vertex_out.tex_coords).rrr, 1.0);
}
