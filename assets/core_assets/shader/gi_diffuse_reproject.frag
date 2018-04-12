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

layout(location = 0) out vec4 out_diffuse;   // reprojected diffuse GI from last frame

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

	out_diffuse  = vec4(0, 0, 0, 1);

	if(prev_uv.x>=0.0 && prev_uv.x<=1.0 && prev_uv.y>=0.0 && prev_uv.y<=1.0) {
		out_diffuse.rgb  = textureLod(history_diff_sampler, prev_uv.xy, current_mip).rgb;
	}
}
