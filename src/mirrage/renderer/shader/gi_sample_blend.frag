#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable


layout(location = 0) in Vertex_data {
	vec2 tex_coords;
} vertex_out;

layout(location = 0) out vec4 out_color;

layout(set=1, binding = 0) uniform sampler2D noise_sampler;

layout(set=2, binding = 0) uniform sampler2D color_sampler;
layout(set=2, binding = 1) uniform sampler2D depth_sampler;
layout(set=2, binding = 2) uniform sampler2D mat_data_sampler;
layout(set=2, binding = 3) uniform sampler2D prev_level_result_sampler;
layout(set=2, binding = 4) uniform sampler2D result_sampler;
layout(set=2, binding = 5) uniform sampler2D history_result_sampler;
layout(set=2, binding = 6) uniform sampler2D history_weight_sampler;
layout(set=2, binding = 7) uniform sampler2D prev_depth_sampler;
layout(set=2, binding = 8) uniform sampler2D prev_mat_data_sampler;
layout(set=2, binding = 9) uniform sampler2D ao_sampler;

layout (constant_id = 0) const int INCLUDE_AO = 0;  // 1 if the result should be modulated by AO

// arguments are packet into the matrices to keep the pipeline layouts compatible between GI passes
layout(push_constant) uniform Push_constants {
	// [3][3] = intensity of ambient occlusion (0 = disabled)
	mat4 projection;

	// [0][0] = higher resolution base MIP level
	// [2][0] = exponent for alternative MIP level scaling factor (see PRIORITISE_NEAR_SAMPLES)
	// [0][3] = current MIP level (relative to base MIP)
	// [1][3] = highest relevant MIP level (relative to base MIP)
	// [2][3] = precalculated part of the ds factor used in calc_illumination_from
	// [3][3] = base MIP level
	mat4 prev_projection;
} pcs;


#include "global_uniforms.glsl"
#include "normal_encoding.glsl"
#include "random.glsl"
#include "brdf.glsl"
#include "upsample.glsl"
#include "raycast.glsl"
#include "median.glsl"

// calculate luminance of a color (used for normalization)
float luminance_norm(vec3 c) {
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

float pixel_intensity(vec3 c) {
	return dot(c,c);
}

void main() {
	// read diffuse color, modulate with median if equal to min/max of neighborhood => noise
	ivec2 result_sampler_size = textureSize(result_sampler, 0).xy;
	ivec2 uv = ivec2(result_sampler_size * vertex_out.tex_coords);
	
	if(uv.x<=1 || uv.y<=1 || uv.x>=result_sampler_size.x-2 || uv.y>=result_sampler_size.y-2) {
		out_color = vec4(texelFetch(result_sampler, uv, 0).rgb, 1);
		return;
	}
	
	vec3 colors[9] = vec3[](
		texelFetchOffset(result_sampler, uv, 0, ivec2(-1,-1)).rgb,
		texelFetchOffset(result_sampler, uv, 0, ivec2(-1, 0)).rgb,
		texelFetchOffset(result_sampler, uv, 0, ivec2( 0,-1)).rgb,
		texelFetchOffset(result_sampler, uv, 0, ivec2( 0, 1)).rgb,
		texelFetchOffset(result_sampler, uv, 0, ivec2( 0, 0)).rgb,
		texelFetchOffset(result_sampler, uv, 0, ivec2( 1,-1)).rgb,
		texelFetchOffset(result_sampler, uv, 0, ivec2(-1, 1)).rgb,
		texelFetchOffset(result_sampler, uv, 0, ivec2( 1, 0)).rgb,
		texelFetchOffset(result_sampler, uv, 0, ivec2( 1, 1)).rgb
	);

	float min_c = pixel_intensity(colors[0]);
	float max_c = min_c;

	for(int i=1; i<9; i++) {
		float intensity = pixel_intensity(colors[i]);
		min_c = min(min_c, intensity);
		max_c = max(max_c, intensity);
	}

	vec3 org = colors[4];
	float org_intensity = dot(org, org);
	if(max_c>org_intensity)
		out_color = vec4(org, 1.0);
	else {
		out_color = vec4(median_vec3(colors), 1.0);
	}

	// modulate diffuse GI by ambient occlusion
	if(INCLUDE_AO==1) {
		float ao = texture(ao_sampler, vertex_out.tex_coords).r * 0.75 + 0.25;
		out_color.rgb *= ao;
		for(int i=0; i<9; i++)
			colors[i] *= ao;
	}

	// blend with history
	vec3 c_history = texelFetch(history_result_sampler, uv, 0).rgb;

	vec2 texel_size = 1.0 / textureSize(result_sampler, 0);

	vec2 du = vec2(texel_size.x, 0.0);
	vec2 dv = vec2(0.0, texel_size.y);

	vec3 ctl = colors[0];
	vec3 ctc = colors[1];
	vec3 ctr = colors[2];
	vec3 cml = colors[3];
	vec3 cmc = out_color.rgb;
	vec3 cmr = colors[5];
	vec3 cbl = colors[6];
	vec3 cbc = colors[7];
	vec3 cbr = colors[8];

	vec3 cmin = min(ctl, min(ctc, min(ctr, min(cml, min(cmc, min(cmr, min(cbl, min(cbc, cbr))))))));
	vec3 cmax = max(ctl, max(ctc, max(ctr, max(cml, max(cmc, max(cmr, max(cbl, max(cbc, cbr))))))));

	vec3 cavg = (ctl + ctc + ctr + cml + cmc + cmr + cbl + cbc + cbr) / 9.0;

	vec3 cmin5 = min(ctc, min(cml, min(cmc, min(cmr, cbc))));
	vec3 cmax5 = max(ctc, max(cml, max(cmc, max(cmr, cbc))));
	vec3 cavg5 = (ctc + cml + cmc + cmr + cbc) / 5.0;
	cmin = 0.5 * (cmin + cmin5);
	cmax = 0.5 * (cmax + cmax5);
	cavg = 0.5 * (cavg + cavg5);

	vec3 d = cmax - cmin;
	cmin -= d * 0.5;

	c_history = clip_aabb(cmin.xyz, cmax.xyz, clamp(cavg, cmin, cmax), c_history);

	float weight = texelFetch(history_weight_sampler, uv, 0).r;

	if(weight<=0)
		weight = 0.0;
	else
		weight = 1.0-1.0/(1+weight);

	out_color.rgb = mix(out_color.rgb, c_history, min(weight, 0.94));
}
