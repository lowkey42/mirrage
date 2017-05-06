#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "normal_encoding.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D depth_sampler;
layout(set=1, binding = 1) uniform sampler2D curr_color_sampler;
layout(set=1, binding = 2) uniform sampler2D prev_color_sampler;

layout(push_constant) uniform Push_constants {
	mat4 reprojection;
	vec4 offsets;
} constants;


float luminance(vec3 c) {
	vec3 f = vec3(0.299,0.587,0.114);
	return sqrt(c.r*c.r*f.r + c.g*c.g*f.g + c.b*c.b*f.b);
}


vec3 clip_aabb(vec3 aabb_min, vec3 aabb_max, vec3 p, vec3 q) {
	// note: only clips towards aabb center (but fast!)
	vec3 p_clip = 0.5 * (aabb_max + aabb_min);
	vec3 e_clip = 0.5 * (aabb_max - aabb_min) + 0.00000001;

	vec3 v_clip = q - p_clip;
	vec3 v_unit = v_clip.xyz / e_clip;
	vec3 a_unit = abs(v_unit);
	float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

	if (ma_unit > 1.0)
		return p_clip + v_clip / ma_unit;
	else
		return q;// point inside aabb
}

void main() {

	vec2 uv = vertex_out.tex_coords;

	float depth = texture(depth_sampler, vertex_out.tex_coords).r;

	vec3 curr = texture(curr_color_sampler, uv - constants.offsets.zw/2.0).rgb;

	vec4 prev_uv = constants.reprojection * vec4(uv*2.0-1.0, depth, 1.0);
	prev_uv.xy = prev_uv.xy/prev_uv.w*0.5+0.5 - constants.offsets.xy/2.0;

	if(prev_uv.x<0.0 || prev_uv.x>1.0 || prev_uv.y<0.0 || prev_uv.y>1.0) {
		out_color = vec4(curr, 1.0);
		return;
	}

	vec3 prev  = texture(prev_color_sampler, prev_uv.xy).rgb;


	const float _SubpixelThreshold = 0.5;
	const float _GatherBase = 0.5;
	const float _GatherSubpixelMotion = 0.1666;

	float k_min_max_support = _GatherBase + _GatherSubpixelMotion;

	vec2 texel_size = 1.0 / textureSize(curr_color_sampler, 0);

	vec2 ss_offset01 = k_min_max_support * vec2(-texel_size.x, texel_size.y);
	vec2 ss_offset11 = k_min_max_support * vec2(texel_size.x, texel_size.y);
	vec3 c00 = texture(curr_color_sampler, uv - ss_offset11).rgb;
	vec3 c10 = texture(curr_color_sampler, uv - ss_offset01).rgb;
	vec3 c01 = texture(curr_color_sampler, uv + ss_offset01).rgb;
	vec3 c11 = texture(curr_color_sampler, uv + ss_offset11).rgb;

	vec3 cmin = min(c00, min(c10, min(c01, c11)));
	vec3 cmax = max(c00, max(c10, max(c01, c11)));
	vec3 cavg = (c00 + c10 + c01 + c11) / 4.0;

	prev = clip_aabb(cmin.xyz, cmax.xyz, clamp(cavg, cmin, cmax), prev);



	float lum_curr = luminance(curr);
	float lum_prev = luminance(prev);

	float lum_diff = abs(lum_curr - lum_prev) / max(lum_curr, max(lum_prev, 0.2));
	float weight = 1.0 - lum_diff;
	float t = mix(0.88, 0.97, weight*weight);

	out_color = vec4(max(vec3(0), mix(curr,prev, t)), 1.0);

	//out_color.rgb = vec3(weight*weight);
}
