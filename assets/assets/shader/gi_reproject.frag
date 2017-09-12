#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"
#include "poisson.glsl"
#include "random.glsl"
#include "global_uniforms.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_input;     // input to current run
layout(location = 1) out vec4 out_diffuse;   // reprojected diffuse GI from last frame
layout(location = 2) out vec4 out_specular;  // reprojected specular GI from last frame
layout(location = 3) out vec4 out_weight;    // weight of reprojected GI

layout(set=1, binding = 0) uniform sampler2D depth_sampler;
layout(set=1, binding = 1) uniform sampler2D mat_data_sampler;
layout(set=1, binding = 2) uniform sampler2D albedo_sampler;
layout(set=1, binding = 3) uniform sampler2D ao_sampler;
layout(set=1, binding = 4) uniform sampler2D history_diff_sampler;
layout(set=1, binding = 5) uniform sampler2D history_spec_sampler;
layout(set=1, binding = 6) uniform sampler2D prev_depth_sampler;
layout(set=1, binding = 7) uniform sampler2D brdf_sampler;

layout(push_constant) uniform Push_constants {
	mat4 reprojection;
	mat4 prev_projection;
} pcs;

#include "gi_blend_common.glsl"


void main() {
	const float PI = 3.14159265359;

	float depth = textureLod(depth_sampler, vertex_out.tex_coords, 0.0).r;

	vec3 pos = position_from_ldepth(vertex_out.tex_coords, depth);

	mat4 prev_projection = pcs.prev_projection;
	prev_projection[0][3] = 0;
	prev_projection[1][3] = 0;
	prev_projection[3][3] = 0;

	mat4 reprojection = pcs.reprojection;
	vec4 prev_projection_info = vec4(reprojection[0][3], reprojection[1][3],
	                                 reprojection[2][3], reprojection[3][3]);
	reprojection[0][3] = 0;
	reprojection[1][3] = 0;
	reprojection[2][3] = 0;
	reprojection[3][3] = 1;

	float current_mip = pcs.prev_projection[0][3];
	float max_mip     = pcs.prev_projection[1][3];
	float ao_factor   = pcs.prev_projection[3][3];


	vec4 prev_pos = reprojection * vec4(pos, 1.0);
	prev_pos /= prev_pos.w;

	vec4 prev_uv = prev_projection * prev_pos;
	prev_uv /= prev_uv.w;
	prev_uv.xy = prev_uv.xy*0.5+0.5;

	out_input    = vec4(0, 0, 0, 0);
	out_diffuse  = vec4(0, 0, 0, 1);
	out_specular = vec4(0, 0, 0, 1);
	out_weight   = vec4(0, 0, 0, 1);

	float global_weight = 1.0;

	if(prev_uv.x<0) {
		global_weight *= 1 + prev_uv.x*20.0;
		prev_uv.x = 0;
	} else if(prev_uv.x>1) {
		global_weight *= 1 + (prev_uv.x-1)*20.0;
		prev_uv.x = 1;
	}
	if(prev_uv.y<0) {
		global_weight *= 1 + prev_uv.y*20.0;
		prev_uv.y = 0;
	} else if(prev_uv.y>1) {
		global_weight *= 1 + (prev_uv.y-1)*20.0;
		prev_uv.y = 1;
	}

	global_weight = clamp(global_weight, 0, 1);

	if(prev_uv.x>=0.0 && prev_uv.x<=1.0 && prev_uv.y>=0.0 && prev_uv.y<=1.0) {
		// load diff + spec GI
		vec3 radiance = textureLod(history_diff_sampler, prev_uv.xy, 0).rgb;
		vec3 specular = upsampled_result(prev_depth_sampler, mat_data_sampler, history_spec_sampler, 0, 0, prev_uv.xy, 1.0).rgb;

		vec3 diffuse;
		vec3 gi = calculate_gi(vertex_out.tex_coords, radiance, specular,
		                       albedo_sampler, mat_data_sampler, brdf_sampler, diffuse);

		float ao = mix(1.0, texture(ao_sampler, vertex_out.tex_coords).r, ao_factor);
		ao = ao*0.5 + 0.5;

		out_input = vec4(diffuse * ao, 0.0);

		float proj_prev_depth = abs(prev_pos.z);
		float prev_depth = textureLod(prev_depth_sampler, prev_uv.xy, 0.0).r;

		vec3 real_prev_pos = vec3((prev_uv.xy * prev_projection_info.xy + prev_projection_info.zw), 1)
		        * prev_depth * -global_uniforms.proj_planes.y;

		vec3 pos_error = real_prev_pos - prev_pos.xyz;

		out_diffuse.rgb  = radiance;
		out_specular.rgb = specular;
		out_weight.r     = (1.0 - smoothstep(0.08, 0.1, dot(pos_error,pos_error)))
		                 * global_weight;
		out_input *= out_weight.r;
	}
}
