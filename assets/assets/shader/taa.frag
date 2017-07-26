#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#include "global_uniforms.glsl"
#include "normal_encoding.glsl"


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
	vec3 view_ray;
	flat vec3 corner_view_rays[4];
} vertex_out;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec4 out_feedback;

layout(set=1, binding = 0) uniform sampler2D depth_sampler;
layout(set=1, binding = 1) uniform sampler2D curr_color_sampler;
layout(set=1, binding = 2) uniform sampler2D prev_color_sampler;
layout(set=1, binding = 3) uniform sampler2D prev_depth_sampler;
layout(set=1, binding = 4) uniform sampler2D prev_feedback_sampler;

layout(push_constant) uniform Push_constants {
	mat4 reprojection;
	mat4 reprojection_fov;
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

//note: normalized random, float=[0;1[
float PDnrand( vec2 n ) {
	return fract( sin(dot(n.xy, vec2(12.9898f, 78.233f)))* 43758.5453f );
}
vec2 PDnrand2( vec2 n ) {
	return fract( sin(dot(n.xy, vec2(12.9898f, 78.233f)))* vec2(43758.5453f, 28001.8384f) );
}
vec3 PDnrand3( vec2 n ) {
	return fract( sin(dot(n.xy, vec2(12.9898f, 78.233f)))* vec3(43758.5453f, 28001.8384f, 50849.4141f ) );
}
vec4 PDnrand4( vec2 n ) {
	return fract( sin(dot(n.xy, vec2(12.9898f, 78.233f)))* vec4(43758.5453f, 28001.8384f, 50849.4141f, 12996.89f) );
}

//====
//note: signed random, float=[-1;1[
float PDsrand( vec2 n ) {
	return PDnrand( n ) * 2 - 1;
}
vec2 PDsrand2( vec2 n ) {
	return PDnrand2( n ) * 2 - 1;
}
vec3 PDsrand3( vec2 n ) {
	return PDnrand3( n ) * 2 - 1;
}
vec4 PDsrand4( vec2 n ) {
	return PDnrand4( n ) * 2 - 1;
}

float read_feedback(vec2 uv) {
	vec2 offset = 1.0 / textureSize(prev_feedback_sampler,0) * (1 + 1.0/3.0);
	float f = max(texture(prev_feedback_sampler, uv+offset).r, texture(prev_feedback_sampler, uv-offset).r);

	if(f<2.0/255.0)
		f = 0;

	return f;
}

vec3 curr_color_normalized(vec2 uv) {
	vec3 c = texture(curr_color_sampler, uv).rgb;
	return c / (1 + luminance(c));
}
vec3 prev_color_normalized(vec2 uv) {
	vec3 c = texture(prev_color_sampler, uv).rgb;
	return c / (1 + luminance(c));
}

void main() {
	// based on:
	//   http://twvideo01.ubm-us.net/o1/vault/gdc2016/Presentations/Pedersen_LasseJonFuglsang_TemporalReprojectionAntiAliasing.pdf
	//   https://github.com/playdeadgames/temporal

	mat4 reprojection_fov = constants.reprojection_fov;
	vec2 offset = vec2(reprojection_fov[0][3], reprojection_fov[1][3]);
	reprojection_fov[0][3] = 0;
	reprojection_fov[1][3] = 0;

	vec2 uv = vertex_out.tex_coords;
	vec4 uv_tmp = reprojection_fov * vec4(uv*2-1, 0.5, 1);
	uv = uv_tmp.xy/uv_tmp.w *0.5+0.5;

	float depth = texture(depth_sampler, uv).r;

	vec3 position = depth * vertex_out.view_ray;

	vec4 prev_uv = constants.reprojection * vec4(position, 1.0);
	prev_uv.xy = (prev_uv.xy/prev_uv.w)*0.5+0.5;

	uv = uv - offset/2;
	vec3 curr = curr_color_normalized(uv).rgb;

	float prev_depth = texture(prev_depth_sampler, prev_uv.xy).r;
	float proj_prev_depth = abs(prev_uv.z) / global_uniforms.proj_planes.y;

	float depth_mismatch = smoothstep(0.002, 0.01, abs(prev_depth-proj_prev_depth));


	if(prev_uv.x<0.0 || prev_uv.x>=1.0 || prev_uv.y<0.0 || prev_uv.y>=1.0) {
		out_color = vec4(curr / (1 - luminance(curr)), 1.0);
		out_feedback = vec4(0, 0, 0, 1);
		return;
	}

	vec3 prev  = prev_color_normalized(prev_uv.xy).rgb;
	float feedback = read_feedback(prev_uv.xy);

	vec2 texel_size = 1.0 / textureSize(curr_color_sampler, 0);

	texel_size *= 1.33;

	vec2 du = vec2(texel_size.x, 0.0);
	vec2 dv = vec2(0.0, texel_size.y);

	vec3 ctl = curr_color_normalized(uv - dv - du).rgb;
	vec3 ctc = curr_color_normalized(uv - dv).rgb;
	vec3 ctr = curr_color_normalized(uv - dv + du).rgb;
	vec3 cml = curr_color_normalized(uv - du).rgb;
	vec3 cmc = curr_color_normalized(uv).rgb;
	vec3 cmr = curr_color_normalized(uv + du).rgb;
	vec3 cbl = curr_color_normalized(uv + dv - du).rgb;
	vec3 cbc = curr_color_normalized(uv + dv).rgb;
	vec3 cbr = curr_color_normalized(uv + dv + du).rgb;

	vec3 cmin = min(ctl, min(ctc, min(ctr, min(cml, min(cmc, min(cmr, min(cbl, min(cbc, cbr))))))));
	vec3 cmax = max(ctl, max(ctc, max(ctr, max(cml, max(cmc, max(cmr, max(cbl, max(cbc, cbr))))))));

	vec3 cavg = (ctl + ctc + ctr + cml + cmc + cmr + cbl + cbc + cbr) / 9.0;

	vec3 cmin5 = min(ctc, min(cml, min(cmc, min(cmr, cbc))));
	vec3 cmax5 = max(ctc, max(cml, max(cmc, max(cmr, cbc))));
	vec3 cavg5 = (ctc + cml + cmc + cmr + cbc) / 5.0;
	cmin = 0.5 * (cmin + cmin5);
	cmax = 0.5 * (cmax + cmax5);
	cavg = 0.5 * (cavg + cavg5);


	vec3 prev_clamped = clip_aabb(cmin.xyz, cmax.xyz, clamp(cavg, cmin, cmax), prev);
	prev_clamped = mix(prev_clamped, prev, feedback);

	float lum_curr = luminance(curr);
	float lum_prev = luminance(prev_clamped);

	float lum_diff = abs(lum_curr - lum_prev) / max(lum_curr, max(lum_prev, 0.2));
	float weight = 1.0 - lum_diff;
	float t = mix(0.88, 0.97, weight*weight);

	out_color = vec4(max(vec3(0), mix(curr, prev_clamped, t)), 1.0);

	out_color.rgb = out_color.rgb / (1 - luminance(out_color.rgb));

	float lum_prev_unclamped = luminance(prev);
	float prev_lum_diff = abs(lum_prev_unclamped - lum_prev) / max(lum_prev_unclamped, max(lum_prev, 0.2));
	float new_feedback = min(1, pow(abs(prev_lum_diff - lum_diff), 5))-depth_mismatch*0.8;

	feedback = mix(feedback, clamp(new_feedback*15, 0, 1), 0.25);

	out_feedback = vec4(feedback, 0, 0, 1);

//	out_color.rgb = vec3(weight*weight);
}
