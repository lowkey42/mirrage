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
layout(location = 3) out vec4 out_success;   // 0=failed reprojection; 1=success

layout(set=1, binding = 0) uniform sampler2D depth_sampler;
layout(set=1, binding = 1) uniform sampler2D mat_data_sampler;
layout(set=1, binding = 2) uniform sampler2D albedo_sampler;
layout(set=1, binding = 3) uniform sampler2D history_diff_sampler;
layout(set=1, binding = 4) uniform sampler2D history_spec_sampler;
layout(set=1, binding = 5) uniform sampler2D prev_depth_sampler;
layout(set=1, binding = 6) uniform sampler2D brdf_sampler;
layout(set=1, binding = 7) uniform sampler2D prev_weight_sampler;

layout(push_constant) uniform Push_constants {
	mat4 reprojection;
	mat4 prev_projection;
} pcs;

#include "gi_blend_common.glsl"


void main() {
	const float PI = 3.14159265359;

	float depth = textureLod(depth_sampler, vertex_out.tex_coords, 0.0).r;
	float depth_dev = fwidth(depth*global_uniforms.proj_planes.y);

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
	out_success  = vec4(0, 0, 0, 1);

	if(prev_uv.x>=0.0 && prev_uv.x<=1.0 && prev_uv.y>=0.0 && prev_uv.y<=1.0) {
		// load diff + spec GI
		vec3 radiance = textureLod(history_diff_sampler, prev_uv.xy, 0).rgb;
		vec3 specular = textureLod(history_spec_sampler, prev_uv.xy, 0).rgb;

		vec3 gi = calculate_gi(vertex_out.tex_coords, radiance, specular*0,
		                       albedo_sampler, mat_data_sampler, brdf_sampler);

		out_input = vec4(gi, 0.0);

		float prev_depth = textureLod(prev_depth_sampler, prev_uv.xy, 0.0).r;

		float pos_error = prev_depth*-global_uniforms.proj_planes.y - prev_pos.z;
		float min_depth_error = mix(0.08, 0.6, depth);
		float max_depth_error = mix(0.12, 0.4, depth);
		out_success.r = clamp(1.0-step(min_depth_error, abs(pos_error)), 0, 1);
		out_success.g = texture(prev_weight_sampler, prev_uv.xy).r;

		out_diffuse.rgb  = radiance;
		out_specular.rgb = specular;
		out_input *= out_success.r;
	}
}
