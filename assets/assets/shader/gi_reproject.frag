#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"
#include "poisson.glsl"
#include "random.glsl"
#include "global_uniforms.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
	vec3 view_ray;
	flat vec3 corner_view_rays[4];
} vertex_out;

layout(location = 0) out vec4 out_input;     // input to current run
layout(location = 1) out vec4 out_diffuse;   // reprojected diffuse GI from last frame
layout(location = 2) out vec4 out_specular;  // reprojected specular GI from last frame
layout(location = 3) out vec4 out_weight;    // weight of reprojected GI

layout(set=1, binding = 1) uniform sampler2D depth_sampler;
layout(set=1, binding = 2) uniform sampler2D mat_data_sampler;
layout(set=1, binding = 5) uniform sampler2D albedo_sampler;
layout(set=1, binding = 7) uniform sampler2D ao_sampler;
layout(set=1, binding = 8) uniform sampler2D history_diff_sampler;
layout(set=1, binding = 9) uniform sampler2D history_spec_sampler;
layout(set=1, binding = 10)uniform sampler2D prev_depth_sampler;

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

	out_input    = vec4(0, 0, 0, 0);
	out_diffuse  = vec4(0, 0, 0, 1);
	out_specular = vec4(0, 0, 0, 1);
	out_weight   = vec4(0, 0, 0, 1);

	if(prev_uv.x>0.0 && prev_uv.x<1.0 && prev_uv.y>0.0 && prev_uv.y<1.0) {
		// load diff + spec GI
		vec3 radiance = upsampled_result(history_diff_sampler, 0, 0, prev_uv.xy).rgb;
		vec3 specular = upsampled_result(history_spec_sampler, 0, 0, prev_uv.xy).rgb;

		vec3 diffuse;
		vec3 gi = calculate_gi(vertex_out.tex_coords, radiance, specular,
		                       albedo_sampler, mat_data_sampler, diffuse);

		float ao = mix(1.0, texture(ao_sampler, vertex_out.tex_coords).r, pcs.arguments.a);
		ao = mix(1.0, ao, pcs.arguments.a);

		out_input = vec4(diffuse * ao, 0.0);

		float prev_depth = textureLod(prev_depth_sampler, prev_uv.xy, 0.0).r;
		out_diffuse.rgb  = radiance;
		out_specular.rgb = specular;
		out_weight.r     = 1.0 - smoothstep(0.01, 1.0, abs(prev_depth-depth)*global_uniforms.proj_planes.y);
	}
}
